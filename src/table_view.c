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

/*static void smth(void) {
	if (sx < 0)
		leftScreenDelta = sx < 0 ? sx : -xMax;
	else if (rightScreenDelta == 0)
		rightScreenDelta = xMax;
	if (sx <= COLS + 1)
		rightScreenDelta = ex - COLS + 1;
}*/

static void table_view_render(TableView *view)
{
	int x, y;
	int sx, ex;
	int xColumn;
	int xMax, yMax;

	Table *const table = view->table;

	x = -view->scroll.x;
	for (size_t i = 0; i < table->numActiveColumns; i++, x += xMax + 1) {
		WINDOW *const pad = view->pads[i];
		getmaxyx(pad, yMax, xMax);
		if (yMax > LINES)
			yMax = LINES;
		if (i == view->cursor.column)
			xColumn = x;
		sx = x;
		ex = x + xMax - 1;
		if (ex < 0)
			continue;
		if (sx < 0)
			sx = 0;
		if (ex >= COLS)
			ex = COLS - 1;
		pnoutrefresh(pad, 0, 0, 0, sx, 0, ex);
		pnoutrefresh(pad, (int) view->scroll.y + 1, 0, 1, sx, yMax - 1, ex);
	}
	doupdate();

	x = xColumn + view->cursor.subColumn;
	y = view->cursor.row - view->scroll.y + 1;
	if (x < 0 || x >= COLS || y < 1 || y >= LINES) {
		curs_set(0);
	} else {
		move(y, x);
		curs_set(1);
	}
}

static void table_view_movecursor(TableView *view, int c)
{
	char *cell;

	Table *const table = view->table;
	switch (c) {
	case KEY_UP:
		if (view->cursor.row == 0)
			break;
		view->cursor.row--;
		view->cursor.subColumn = 0;
		break;
	case KEY_DOWN:
		if (view->cursor.row + 1 == table->numActiveRows)
			break;
		view->cursor.row++;
		view->cursor.subColumn = 0;
		break;
	case KEY_LEFT:
		if (view->cursor.subColumn == 0) {
			if (view->cursor.column == 0)
				break;
			view->cursor.column--;
			cell = table->cells[table->activeRows[view->cursor.row]]
				[table->activeColumns[view->cursor.column]];
			view->cursor.subColumn = strlen(cell) - 1;
		} else {
			view->cursor.subColumn--;
		}
		break;
	case KEY_RIGHT:
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		if (view->cursor.subColumn + 1 == strlen(cell)) {
			if (view->cursor.column + 1 == table->numActiveColumns)
				break;
			view->cursor.column++;
			view->cursor.subColumn = 0;
		} else {
			view->cursor.subColumn++;
		}
		break;
	default:
		return;
	}
	if (view->cursor.row < view->scroll.y)
		view->scroll.y = view->cursor.row;
	else if (view->cursor.row > view->scroll.y + LINES - 2)
		view->scroll.y = view->cursor.row - (LINES - 2); 
}

static void table_view_scroll(TableView *view, int c)
{
	Table *const table = view->table;
	switch (c) {
	case 'h':
		if (view->scroll.x == 0)
			break;
		view->scroll.x--;
		break;
	case 'H':
		view->scroll.x += 0;//leftScreenDelta;
		break;
	case 'j':
		if (view->scroll.y + (size_t) LINES == table->numRows)
			break;
		view->scroll.y++;
		break;
	case 'k':
		if (view->scroll.y == 0)
			break;
		view->scroll.y--;
		break;
	case 'l':
		view->scroll.x++;
		break;
	case 'L':
		view->scroll.x += 0;//rightScreenDelta;
		break;
	}
}

int table_view_show(TableView *view)
{
	initscr();
	keypad(stdscr, true);

	if (table_view_regeneratepads(view) < 0) {
		endwin();
		return -1;
	}

	refresh();
	while (1) {
		clear();
		refresh();
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
