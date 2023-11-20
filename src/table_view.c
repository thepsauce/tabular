#include "tabular.h"

int table_view_init(TableView *view, Table *table)
{
	memset(view, 0, sizeof(*view));
	view->table = table;
	return 0;
}

static int table_view_regeneratepads(TableView *view)
{
	WINDOW *preview;
	WINDOW **pads;
	int x, y;

	Table *const table = view->table;

	(void) y;

	const int cols = COLS < 28 ? 28 : COLS;
	const int mostMaxWidth = cols / 4 + ((cols / 4) & 1);

	preview = newpad(2, mostMaxWidth);
	if (preview == NULL)
		return -1;
	pads = malloc(sizeof(*pads) * table->numActiveColumns);
	if (pads == NULL) {
		delwin(preview);
		return -1;
	}

	for (size_t i = 0; i < table->numActiveColumns; i++) {
		int maxWidth;
		int titleWidth;
		WINDOW *pad;

		const size_t column = table->activeColumns[i];
		mvwaddstr(preview, 0, 0, table->columnNames[column]);
		getyx(preview, y, x);
		if (y == 1)
			maxWidth = mostMaxWidth;
		else
			maxWidth = x;
		titleWidth = maxWidth;

		for (size_t j = 0; j < table->numActiveRows; j++) {
			int w;

			const size_t row = table->activeRows[j];
			mvwaddstr(preview, 0, 0, table->cells[row][column]);
			getyx(preview, y, x);
			if (y == 1)
				w = mostMaxWidth;
			else
				w = x;
			if (w > maxWidth)
				maxWidth = w;
		}

		pad = newpad(table->numActiveRows + 1, maxWidth);
		if (pad == NULL) {
			while (i > 0) {
				i--;
				delwin(pads[i]);
			}
			free(pads);
			delwin(preview);
			return -1;
		}

		wattr_on(pad, A_REVERSE, NULL);
		for (x = 0; x < maxWidth; x++)
			mvwaddch(pad, 0, x, ' ');
		wmove(pad, 0, (maxWidth - titleWidth) / 2);
		for (char *s = table->columnNames[column]; *s != '\0'; s++) {
			waddch(pad, *s);
			getyx(pad, y, x);
			if (y == 1 && x > 0) {
				wmove(pad, 1, 0);
				wclrtoeol(pad);
				mvwaddstr(pad, 0, maxWidth - 3, "...");
				break;
			}
		}
		wattr_off(pad, A_REVERSE, NULL);

		for (size_t j = 0; j < table->numActiveRows; j++) {
			const size_t row = table->activeRows[j];
			wmove(pad, 1 + j, 0);
			for (char *s = table->cells[row][column]; *s != '\0'; s++)
				waddch(pad, *s);
		}

		pads[i] = pad;
	}
	delwin(preview);
	if (view->pads) {
		for (size_t i = 0; i < table->numActiveColumns; i++)
			delwin(view->pads[i]);
		free(view->pads);
	}
	view->pads = pads;
	return 0;
}

static void table_view_render(TableView *view)
{
	int x, y;
	int sx, ex;
	int columnsx, columnex;
	int maxx, maxy;

	Table *const table = view->table;

	x = -view->scroll.x;
	for (size_t i = 0; i < table->numActiveColumns; i++, x += maxx + 1) {
		WINDOW *const pad = view->pads[i];
		getmaxyx(pad, maxy, maxx);
		if (maxy > LINES)
			maxy = LINES;
		sx = x;
		ex = x + maxx - 1;
		if (sx < 0)
			sx = 0;
		if (ex > COLS - 1)
			ex = COLS - 1;
		if (i == view->cursor.column) {
			columnsx = sx;
			columnex = ex;
		}
		if (ex < 0)
			continue;
		mvaddch(0, x + maxx, '|' | A_REVERSE);
		for (y = 1; y < LINES; y++)
			mvaddch(y, x + maxx, '|');
		pnoutrefresh(pad, 0, 0, 0, sx, 0, ex);
		pnoutrefresh(pad, (int) view->scroll.y + 1, 0, 1, sx, maxy - 1, ex);
		/* if this is wasn't here, it wouldn't render properly ¯\_(ツ)_/¯ */
		/* maybe ncurses behavior that I don't understand */
		/* TODO: investigate */
		refresh();
	}

	y = view->cursor.row - view->scroll.y + 1;
	if (y >= 1 && y < LINES && columnsx >= 0 && columnex < COLS) {
		WINDOW *const pad = view->cursor.cell;
		prefresh(pad, 0, view->cursor.scroll, y, columnsx, y, columnex);
	}
	x = columnsx + view->cursor.subColumn - view->cursor.scroll;
	if (x < 0 || x > columnex || y < 1 || y >= LINES) {
		curs_set(0);
	} else {
		move(y, x);
		curs_set(1);
	}
}

static size_t string_endpoint(const char *str)
{
	size_t ep;

	ep = strlen(str);
	if (ep == 0)
		return 0;
	do
		ep--;
	while ((str[ep] & 0xc0) == 0x80);
	return ep;
}

static void table_view_movecursor(TableView *view, int c)
{
	char *cell;
	size_t x, begx, maxx;

	Table *const table = view->table;
	switch (c) {
	case KEY_UP:
		if (view->cursor.row == 0)
			break;
		view->cursor.row--;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		view->cursor.subColumn = MIN(view->cursor.columnTracker,
				string_endpoint(cell));
		view->cursor.scroll = 0;
		break;
	case KEY_DOWN:
		if (view->cursor.row + 1 == table->numActiveRows)
			break;
		view->cursor.row++;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		view->cursor.subColumn = MIN(view->cursor.columnTracker,
				string_endpoint(cell));
		view->cursor.scroll = 0;
		break;

	case KEY_LEFT:
		if (view->cursor.subColumn == 0) {
			if (view->cursor.column == 0)
				break;
			view->cursor.column--;
			cell = table->cells[table->activeRows[view->cursor.row]]
				[table->activeColumns[view->cursor.column]];
			view->cursor.subColumn = string_endpoint(cell);
			view->cursor.scroll = 0;
		} else {
			cell = table->cells[table->activeRows[view->cursor.row]]
				[table->activeColumns[view->cursor.column]];
			/* this assures correct handling of utf8 */
			do
				view->cursor.subColumn--;
			while ((cell[view->cursor.subColumn] & 0xc0) == 0x80);
		}
		view->cursor.columnTracker = view->cursor.subColumn;
		break;
	case KEY_RIGHT:
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		if (view->cursor.subColumn >= string_endpoint(cell)) {
			if (view->cursor.column + 1 == table->numActiveColumns)
				break;
			view->cursor.column++;
			view->cursor.scroll = 0;
			view->cursor.subColumn = 0;
		} else {
			do
				view->cursor.subColumn++;
			while ((cell[view->cursor.subColumn] & 0xc0) == 0x80);
		}
		view->cursor.columnTracker = view->cursor.subColumn;
		break;

	case '\t':
		if (view->cursor.column + 1 == table->numActiveColumns) {
			if (view->cursor.row + 1 == table->numActiveRows)
				break;
			view->cursor.row++;
			view->cursor.column = 0;
		} else {
			view->cursor.column++;
		}
		view->cursor.subColumn = 0;
		view->cursor.columnTracker = 0;
		view->cursor.scroll = 0;
		break;
	case KEY_BTAB:
		if (view->cursor.column == 0) {
			if (view->cursor.row == 0)
				break;
			view->cursor.row--;
			view->cursor.column = table->numActiveColumns - 1;
		} else {
			view->cursor.column--;
		}
		view->cursor.subColumn = 0;
		view->cursor.columnTracker = 0;
		view->cursor.scroll = 0;
		break;
	case '+':
		view->cursor.scroll++;
		return;
	case '-':
		view->cursor.scroll--;
		return;
	case '=':
		view->cursor.scroll = 0;
		return;
	default:
		return;
	}
	if (view->cursor.row < view->scroll.y)
		view->scroll.y = view->cursor.row;
	else if (view->cursor.row > view->scroll.y + LINES - 2)
		view->scroll.y = view->cursor.row - (LINES - 2);

	cell = table->cells[table->activeRows[view->cursor.row]]
		[table->activeColumns[view->cursor.column]];
	delwin(view->cursor.cell);
	view->cursor.cell = newpad(1, strlen(cell));
	waddstr(view->cursor.cell, cell);

	maxx = getmaxx(view->pads[view->cursor.column]);
	if (view->cursor.subColumn < view->cursor.scroll)
		view->cursor.scroll = view->cursor.subColumn;
	else if (view->cursor.subColumn > view->cursor.scroll + maxx - 1)
		view->cursor.scroll = view->cursor.subColumn - (maxx - 1);

	begx = 0;
	for (size_t i = 0; i < view->cursor.column; i++) {
		maxx = getmaxx(view->pads[i]);
		begx += maxx + 1;
	}
	maxx = getmaxx(view->pads[view->cursor.column]);

	x = begx + view->cursor.subColumn - view->cursor.scroll;
	if (x < view->scroll.x)
		view->scroll.x = x;
	else if (x > view->scroll.x + COLS - 1)
		view->scroll.x = x - (COLS - 1);
}

static void table_view_scroll(TableView *view, int c)
{
	size_t x;
	int maxx;
	size_t w;
	size_t i;

	Table *const table = view->table;
	switch (c) {
	case 'h':
		if (view->scroll.x == 0)
			break;
		view->scroll.x--;
		break;
	case 'H':
		x = 0;
		for (i = 0; i < table->numActiveColumns; i++) {
			WINDOW *const pad = view->pads[i];
			maxx = getmaxx(pad);
			if (x + maxx + 1 >= view->scroll.x)
				break;
			x += maxx + 1;
		}
		view->scroll.x = x;
		break;

	case 'j':
		if (view->scroll.y + (size_t) LINES >= table->numRows)
			break;
		view->scroll.y++;
		break;
	case 'k':
		if (view->scroll.y == 0)
			break;
		view->scroll.y--;
		break;

	case 'l':
		w = 0;
		for (size_t i = 0; i < table->numActiveColumns; i++) {
			WINDOW *const pad = view->pads[i];
			w += getmaxx(pad) + 1;
		}
		if (view->scroll.x + (size_t) COLS >= w)
			break;
		view->scroll.x++;
		break;
	case 'L':
		x = 0;
		for (i = 0; i < table->numActiveColumns; i++) {
			WINDOW *const pad = view->pads[i];
			maxx = getmaxx(pad);
			if (x > view->scroll.x)
				break;
			x += maxx + 1;
		}
		w = 0;
		for (size_t i = 0; i < table->numActiveColumns; i++) {
			WINDOW *const pad = view->pads[i];
			w += getmaxx(pad) + 1;
		}
		if (x + (size_t) COLS >= w)
			x = w - COLS;
		view->scroll.x = x;
		break;
	}
}

int table_view_show(TableView *view)
{
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, true);

	if (table_view_regeneratepads(view) < 0) {
		endwin();
		return -1;
	}

	refresh();
	while (1) {
		table_view_render(view);
		const int c = getch();
		if (c == 'q')
			break;

		table_view_movecursor(view, c);
		table_view_scroll(view, c);
	}

	endwin();
	return 0;
}

void table_view_uninit(TableView *view)
{
	if (view->pads != NULL) {
		for (size_t i = 0; i < view->table->numActiveColumns; i++)
			delwin(view->pads[i]);
		free(view->pads);
	}
}
