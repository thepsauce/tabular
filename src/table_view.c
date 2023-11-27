#include "tabular.h"

int table_view_init(TableView *view, Table *table)
{
	memset(view, 0, sizeof(*view));
	view->table = table;
	if (table->numActiveRows > 0 && table->numActiveColumns > 0) {
		char *const cell = table->cells[table->activeRows[0]]
			[table->activeColumns[0]];
		view->cursor.lenText = strlen(cell);
		view->cursor.capText = MAX((size_t) 512, view->cursor.lenText);
		view->cursor.text = malloc(view->cursor.capText);
		if (view->cursor.text == NULL) {
			view->cursor.capText = 0;
			view->cursor.text = cell;
		} else {
			memcpy(view->cursor.text, cell, view->cursor.lenText);
		}
	}
	return 0;
}

static int table_view_updatecolumns(TableView *view)
{
	size_t *newColumnWidths;
	struct fitting fit;

	const int cols = COLS < 28 ? 28 : COLS;
	const size_t mostMaxWidth = cols / 4 + ((cols / 4) & 1);

	Table *const table = view->table;

	newColumnWidths = realloc(view->columnWidths,
			sizeof(*view->columnWidths) *
				table->numActiveColumns);
	if (newColumnWidths == NULL)
		return -1;
	view->columnWidths = newColumnWidths;

	for (size_t i = 0; i < table->numActiveColumns; i++) {
		size_t maxWidth;

		const size_t column = table->activeColumns[i];
		const char *const name = table->columnNames[column];
		utf8_getfitting(name, mostMaxWidth, &fit);
		maxWidth = fit.width;

		for (size_t j = 0; j < table->numActiveRows; j++) {
			const size_t row = table->activeRows[j];
			const char *cell = table->cells[row][column];
			utf8_getfitting(cell, mostMaxWidth, &fit);
			if (fit.width > maxWidth)
				maxWidth = fit.width;
			if (maxWidth == mostMaxWidth)
				break;
		}
		view->columnWidths[i] = maxWidth;
	}
	return 0;
}

static void table_view_render(TableView *view)
{
	struct fitting fit;
	size_t x, y;
	size_t sx;
	size_t columnsx, columnex;
	size_t width;
	(void) columnsx;
	(void) columnex;

	Table *const table = view->table;

	erase();
	x = 0;
	for (size_t i = 0; i < table->numActiveColumns;
			x += view->columnWidths[i] + 1, i++) {
		const size_t column = table->activeColumns[i];
		const char *const title = table->columnNames[column];

		width = view->columnWidths[i];
		if (x + width < view->scroll.x) {
			curs_set(0);
			continue;
		}

		if (x < view->scroll.x) {
			width -= view->scroll.x - sx;
			sx = view->scroll.x;
		} else {
			sx = x;
		}
		if (i == view->cursor.column) {
			columnsx = sx;
			columnex = sx + width;
		}
		if (sx >= view->scroll.x + COLS)
			continue;
		if (sx + width > COLS + view->scroll.x) {
			width = COLS + view->scroll.x - sx;
		}

		attr_on(A_REVERSE, NULL);
		for (size_t i = 0; i < width; i++)
			mvaddch(0, (int) (i + sx - view->scroll.x), ' ');
		if (utf8_getfitting(title, width, &fit) != 0) {
			move(0, sx - view->scroll.x);
			addnstr(fit.start, fit.end - fit.start);
			mvaddstr(0, sx - view->scroll.x + fit.width - 1, "…");
		} else {
			move(0, sx - view->scroll.x + (width - fit.width) / 2);
			addstr(title);
		}

		for (size_t j = view->scroll.y;
				j < MIN(table->numActiveRows,
					LINES - 2 + view->scroll.y); j++) {
			const size_t row = table->activeRows[j];
			const char *cell = table->cells[row][column];
			if (i == view->cursor.column && j == view->cursor.row) {
				attr_on(A_REVERSE, NULL);
			} else {
				attr_off(A_REVERSE, NULL);
			}
			move(j - view->scroll.y + 1, sx - view->scroll.x);
			if (utf8_getfitting(cell, width, &fit) != 0) {
				addnstr(fit.start, fit.end - fit.start);
				mvaddstr(j - view->scroll.y + 1,
					sx - view->scroll.x + fit.width - 1,
					"…");
			} else {
				addstr(cell);
			}
		}

		if (sx + width >= view->scroll.x + COLS) {
			curs_set(0);
			continue;
		}
		attr_off(A_REVERSE, NULL);
		mvaddch(0, sx + width - view->scroll.x, '|' | A_REVERSE);
		for (y = 0; y < MIN((size_t) LINES, table->numActiveRows);
				y++)
			mvaddch(y + 1, sx + width - view->scroll.x, '|');
	}

	mvaddnstr(LINES - 1, 0, view->cursor.text, view->cursor.index);
	getyx(stdscr, y, x);
	addnstr(&view->cursor.text[view->cursor.index],
			view->cursor.lenText - view->cursor.index);
	clrtoeol();

	if (view->mode == TABLE_VIEW_NORMAL) {
		curs_set(0);
	} else {
		curs_set(1);
		move(y, x);
	}
}

static void table_view_updatecursor(TableView *view)
{
	size_t x, begx, maxx;

	if (view->cursor.row < view->scroll.y)
		view->scroll.y = view->cursor.row;
	else if (view->cursor.row > view->scroll.y + LINES - 3)
		view->scroll.y = view->cursor.row - (LINES - 3);

	maxx = view->columnWidths[view->cursor.column];
	if (view->cursor.index < view->cursor.scroll)
		view->cursor.scroll = view->cursor.index;
	else if (view->cursor.index > view->cursor.scroll + maxx - 1)
		view->cursor.scroll = view->cursor.index - (maxx - 1);

	begx = 0;
	for (size_t i = 0; i < view->cursor.column; i++) {
		maxx = view->columnWidths[i];
		begx += maxx + 1;
	}
	maxx = view->columnWidths[view->cursor.column];

	x = begx + view->cursor.index - view->cursor.scroll;
	if (x < view->scroll.x)
		view->scroll.x = x;
	else if (x > view->scroll.x + COLS - 1)
		view->scroll.x = x - (COLS - 1);
}

static void table_view_updatecell(TableView *view, size_t row, size_t column)
{
	Utf8 **pOldCell;
	Utf8 *newCell;
	Utf8 *cell;

	Table *const table = view->table;
	pOldCell = &table->cells[table->activeRows[row]]
		[table->activeColumns[column]];
	newCell = realloc(*pOldCell, view->cursor.lenText + 1);
	if (newCell != NULL) {
		*pOldCell = newCell;
		memcpy(newCell, view->cursor.text, view->cursor.lenText);
		newCell[view->cursor.lenText] = '\0';
	}

	if (row == view->cursor.row && column == view->cursor.column)
		return;
	cell = table->cells[table->activeRows[view->cursor.row]]
		[table->activeColumns[view->cursor.column]];
	view->cursor.lenText = strlen(cell);
	if (view->cursor.capText < view->cursor.lenText) {
		Utf8 *newText;

		newText = realloc(view->cursor.text,
				view->cursor.lenText);
		if (newText == NULL) {
			free(view->cursor.text);
			view->cursor.text = cell;
			view->cursor.capText = 0;
		} else {
			view->cursor.text = newText;
			memcpy(view->cursor.text, cell, view->cursor.lenText);
		}
	} else {
		memcpy(view->cursor.text, cell, view->cursor.lenText);
	}
}

static void table_view_movecursor(TableView *view, int c)
{
	Utf8 *cell;
	size_t end;

	Table *const table = view->table;
	const size_t oldRow = view->cursor.row;
	const size_t oldColumn = view->cursor.column;
	switch (c) {
	case KEY_UP:
		if (view->cursor.row == 0)
			break;
		view->cursor.row--;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		end = strlen(cell);
		if (view->mode == TABLE_VIEW_NORMAL)
			end -= utf8_previouslength(&cell[end]);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;
	case KEY_DOWN:
		if (view->cursor.row + 1 == table->numActiveRows)
			break;
		view->cursor.row++;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		end = strlen(cell);
		if (view->mode == TABLE_VIEW_NORMAL)
			end -= utf8_previouslength(&cell[end]);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;

	case KEY_LEFT:
		if (view->mode == TABLE_VIEW_NORMAL) {
			if (view->cursor.column == 0)
				break;
			view->cursor.column--;
			cell = table->cells[table->activeRows[view->cursor.row]]
				[table->activeColumns[view->cursor.column]];
			end = strlen(cell);
			if (view->mode == TABLE_VIEW_NORMAL)
				end -= utf8_previouslength(&cell[end]);
			view->cursor.index = end;
			view->cursor.scroll = 0;
		} else if (view->cursor.index > 0) {
			view->cursor.index -= utf8_previouslength(
				&view->cursor.text[view->cursor.index]);
		}
		view->cursor.indexTracker = view->cursor.index;
		break;
	case KEY_RIGHT:
		end = view->cursor.lenText - utf8_previouslength(
				&view->cursor.text[view->cursor.lenText]);
		if (view->mode == TABLE_VIEW_NORMAL) {
			if (view->cursor.column + 1 == table->numActiveColumns)
				break;
			view->cursor.column++;
			view->cursor.scroll = 0;
			view->cursor.index = 0;
		} else if (view->cursor.index <= end) {
			view->cursor.index += utf8_nextlength(
				&view->cursor.text[view->cursor.index]);
		}
		view->cursor.indexTracker = view->cursor.index;
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
		view->cursor.index = 0;
		view->cursor.indexTracker = 0;
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
		view->cursor.index = 0;
		view->cursor.indexTracker = 0;
		view->cursor.scroll = 0;
		break;
	default:
		return;
	}

	if (oldRow != view->cursor.row || oldColumn != view->cursor.column)
		table_view_updatecell(view, oldRow, oldColumn);
	table_view_updatecursor(view);
}

static void table_view_type(TableView *view, int c)
{
	if (view->cursor.capText == 0)
		return;
	switch (c) {
		size_t count;

	case '\n':
		table_view_updatecell(view, view->cursor.row,
				view->cursor.column);
		return;
	case 0x7f:
	case KEY_BACKSPACE:
		if (view->cursor.index == 0)
			return;
		count = utf8_previouslength(
				&view->cursor.text[view->cursor.index]);
		view->cursor.index -= count;
		memmove(&view->cursor.text[view->cursor.index],
			&view->cursor.text[view->cursor.index + count],
			view->cursor.lenText - count -
				view->cursor.index);
		view->cursor.lenText--;
		goto update;
	case KEY_DC:
		if (view->cursor.index == view->cursor.lenText)
			return;
		count = utf8_nextlength(
				&view->cursor.text[view->cursor.index]);
		memmove(&view->cursor.text[view->cursor.index],
			&view->cursor.text[view->cursor.index + count],
			view->cursor.lenText - view->cursor.index);
		view->cursor.lenText--;
		goto update;
	}
	if (c < 0x20 || c >= 0xff)
		return;
	if (view->cursor.capText < view->cursor.lenText + 1) {
		Utf8 *newText;

		view->cursor.capText++;
		newText = realloc(view->cursor.text, view->cursor.capText);
		if (newText == NULL)
			return;
		view->cursor.text = newText;
	}
	memmove(&view->cursor.text[view->cursor.index + 1],
		&view->cursor.text[view->cursor.index],
		view->cursor.lenText - view->cursor.index);
	view->cursor.text[view->cursor.index++] = c;
	view->cursor.lenText++;

	if (c & 0x80) {
		size_t start;
		size_t end;

		/* make sure the utf8 sequence is complete */
		for (start = end = view->cursor.index; start > 0; ) {
			start--;
			if (view->cursor.text[start] & 0xc0)
				break;
		}
		if (end - start != utf8_determinate(view->cursor.text[start]))
			return;
	}

update:
	table_view_updatecursor(view);
}

static void table_view_scroll(TableView *view, int c)
{
	size_t x;
	size_t maxx;
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
			maxx = view->columnWidths[i];
			if (x + maxx + 1 >= view->scroll.x)
				break;
			x += maxx + 1;
		}
		view->scroll.x = x;
		break;

	case 'j':
		if (view->scroll.y + LINES - 1 >= table->numRows)
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
		for (size_t i = 0; i < table->numActiveColumns; i++)
			w += view->columnWidths[i] + 1;
		if (view->scroll.x + COLS >= w)
			break;
		view->scroll.x++;
		break;
	case 'L':
		x = 0;
		for (i = 0; i < table->numActiveColumns; i++) {
			maxx = view->columnWidths[i];
			if (x > view->scroll.x)
				break;
			x += maxx + 1;
		}
		w = 0;
		for (size_t i = 0; i < table->numActiveColumns; i++)
			w += view->columnWidths[i] + 1;
		if (x + COLS >= w)
			x = w - COLS;
		view->scroll.x = x;
		break;
	}
}

static void table_view_hotkey(TableView *view, int c)
{
	switch (c) {
	case 'A':
		table_parseline(view->table, "");
		view->cursor.column = 0;
		view->table->activeRows[view->table->numActiveRows++] =
			view->table->numRows - 1;
		view->cursor.row = view->table->numActiveRows - 1;
		break;

	case 'I':
	case 'i':
		view->mode = TABLE_VIEW_INSERT;
		break;
	}
}

int table_view_show(TableView *view)
{
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, true);

	if (table_view_updatecolumns(view) < 0) {
		endwin();
		return -1;
	}

	refresh();
	while (1) {
		table_view_render(view);
		const int c = getch();
		if (view->mode == TABLE_VIEW_NORMAL && c == 'q')
			break;

		table_view_movecursor(view, c);
		switch (view->mode) {
		case TABLE_VIEW_INSERT:
			table_view_type(view, c);
			break;
		case TABLE_VIEW_NORMAL:
			table_view_scroll(view, c);
			table_view_hotkey(view, c);
			break;
		}
	}

	endwin();
	return 0;
}

void table_view_uninit(TableView *view)
{
	(void) view;
}
