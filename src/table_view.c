#include "tabular.h"

static void table_view_showerror(TableView *view, const char *fmt, ...)
{
	va_list l;

	(void) view;

	endwin();

	fprintf(stderr, "An error occured!!\n");

	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);

	fprintf(stderr, "\n\nPress enter to continue...\n");
	while (getchar() != '\n');
}

static void *table_view_realloc(TableView *view, void *ptr, size_t newSize,
		const char *file, int line)
{
	char choice;

	(void) view;

	if (newSize == 0)
		return NULL;
retry:
	ptr = realloc(ptr, newSize);
	if (ptr != NULL)
		return ptr;
	endwin();
	fprintf(stderr, ">>> failed allocating '%zu' bytes at %s:%d<<<\n",
			newSize, file, line);
	/* RB80 */
	fprintf(stderr, "note: continuing should not crash or break the program but impact its usability\n");
	do {
		/* RB80 */
		fprintf(stderr, "should the program continue (yn) or retry (r)? [ynr]\n");
		choice = getchar();
		if (choice == 'y')
			return NULL;
		if (choice == 'r')
			goto retry;
		if (choice == 'n')
			exit(0);
		while (choice != '\n')
			choice = getchar();
	} while (choice != 'y' && choice != 'n' && choice != 'r');

	return NULL;
}

#define table_view_realloc(view, ptr, newSize) table_view_realloc(view, ptr, newSize, __FILE__, __LINE__);

static int table_view_updatecolumns(TableView *view)
{
	size_t *newColumnWidths;
	struct fitting fit;

	const int cols = COLS < 28 ? 28 : COLS;
	const size_t mostMaxWidth = cols / 4 + ((cols / 4) & 1);

	Table *const table = view->table;

	newColumnWidths = table_view_realloc(view, view->columnWidths,
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
		view->columnWidths[i] = MAX(maxWidth, (size_t) 1);
	}
	return 0;
}

static void table_view_updatecursor(TableView *view)
{
	size_t begx, maxx;

	if (view->mode != TABLE_VIEW_NORMAL) {
		struct fitting fit;
		int d;

		d = view->mode == TABLE_VIEW_COMMAND;
		if (utf8_getnfitting(view->cursor.text, view->cursor.index,
					&fit) < 0)
			fit.width = 0;
		if (fit.width < view->cursor.scroll + d)
			view->cursor.scroll = fit.width;
		else if (fit.width > view->cursor.scroll + COLS - d - 1)
			view->cursor.scroll = fit.width - (COLS - d - 1);
		return;
	}

	if (view->table->numActiveColumns == 0) {
		view->scroll.x = 0;
		view->scroll.y = 0;
		return;
	}

	begx = 0;
	for (size_t i = 0; i < view->cursor.column; i++) {
		maxx = view->columnWidths[i];
		begx += maxx + 1;
	}
	maxx = view->columnWidths[view->cursor.column];

	if (begx + maxx < view->scroll.x)
		view->scroll.x = begx + maxx;
	else if (begx > view->scroll.x + (COLS - 1))
		view->scroll.x = begx - (COLS - 1);

	if (view->cursor.row < view->scroll.y)
		view->scroll.y = view->cursor.row;
	else if (view->cursor.row > view->scroll.y + LINES - 3)
		view->scroll.y = view->cursor.row - (LINES - 3);
}

static void table_view_updatetext(TableView *view)
{
	const Utf8 *cell;

	Table *const table = view->table;
	if (table->numActiveRows == 0 ||
			table->numActiveColumns == 0) {
		cell = "";
	} else {
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
	}
	view->cursor.lenText = strlen(cell);
	if (view->cursor.capText < view->cursor.lenText + 1) {
		Utf8 *newText;

		newText = table_view_realloc(view, view->cursor.text,
				view->cursor.lenText + 1);
		if (newText != NULL) {
			view->cursor.text = newText;
			memcpy(view->cursor.text, cell, view->cursor.lenText);
			view->cursor.text[view->cursor.lenText] = '\0';
			view->cursor.capText = view->cursor.lenText;
		}
	} else {
		memcpy(view->cursor.text, cell, view->cursor.lenText);
		view->cursor.text[view->cursor.lenText] = '\0';
	}
	view->cursor.index = view->cursor.lenText;
	table_view_updatecursor(view);
}

int table_view_init(TableView *view, Table *table)
{
	initscr();
	endwin();

	memset(view, 0, sizeof(*view));
	view->table = table;
	table_view_updatecolumns(view);
	table_view_updatetext(view);
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
		move(0, sx - view->scroll.x);
		if (utf8_getfitting(title, width, &fit) != 0) {
			move(0, sx - view->scroll.x);
			addnstr(fit.start, fit.end - fit.start);
			mvaddstr(0, sx - view->scroll.x + fit.width - 1, "…");
		} else {
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
				for (size_t i = fit.width; i < width; i++)
					addch(' ');
			}
		}

		if (sx + width >= view->scroll.x + COLS) {
			curs_set(0);
			attr_off(A_REVERSE, NULL);
			continue;
		}
		attr_off(A_REVERSE, NULL);
		mvaddch(0, sx + width - view->scroll.x, '|' | A_REVERSE);
		for (y = 0; y < MIN((size_t) LINES, table->numActiveRows);
				y++)
			mvaddch(y + 1, sx + width - view->scroll.x, '|');
	}

	move(LINES - 1, 0);
	if (view->mode == TABLE_VIEW_COMMAND)
		addch(':' | A_REVERSE);
	if (utf8_getfitting(view->cursor.text, view->cursor.scroll, &fit) < 0)
		addstr(view->cursor.text);
	else
		addstr(fit.end);
	clrtoeol();

	if (view->mode == TABLE_VIEW_NORMAL) {
		curs_set(0);
	} else {
		curs_set(1);
		if (utf8_getfitting(view->cursor.text, view->cursor.index,
					&fit) < 0)
			fit.width = 0;
		move(LINES - 1, fit.width - view->cursor.scroll +
				(view->mode == TABLE_VIEW_COMMAND));
	}
}

static void table_view_interruptcommand(TableView *view)
{
	table_view_updatetext(view);
	view->mode = TABLE_VIEW_NORMAL;
}

static void table_view_updatecell(TableView *view, size_t row, size_t column)
{
	Utf8 **pOldCell;
	Utf8 *newCell;

	Table *const table = view->table;
	if (table->numActiveRows == 0 || table->numActiveColumns == 0)
		return;
	pOldCell = &table->cells[table->activeRows[row]]
		[table->activeColumns[column]];
	newCell = table_view_realloc(view, *pOldCell, view->cursor.lenText + 1);
	if (newCell != NULL) {
		*pOldCell = newCell;
		memcpy(newCell, view->cursor.text, view->cursor.lenText + 1);
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
		if (view->mode != TABLE_VIEW_NORMAL || view->cursor.row == 0)
			break;
		view->cursor.row--;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		end = strlen(cell);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;
	case KEY_DOWN:
		if (view->mode != TABLE_VIEW_NORMAL ||
				view->cursor.row + 1 >= table->numActiveRows)
			break;
		view->cursor.row++;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeColumns[view->cursor.column]];
		end = strlen(cell);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;

	case KEY_LEFT:
		if (view->mode == TABLE_VIEW_NORMAL) {
			if (view->cursor.column == 0)
				break;
			view->cursor.column--;
			view->cursor.index = 0;
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

	case KEY_HOME:
		if (view->mode == TABLE_VIEW_NORMAL)
			view->cursor.column = 0;
		else
			view->cursor.index = 0;
		break;
	case KEY_END:
		if (view->mode == TABLE_VIEW_NORMAL)
			view->cursor.column =
				table->numActiveColumns == 0 ? 0 :
					table->numActiveColumns - 1;
		else
			view->cursor.index = view->cursor.lenText;
		break;
	case '\t':
		if (view->mode != TABLE_VIEW_NORMAL)
			return;
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
		if (view->mode != TABLE_VIEW_NORMAL)
			return;
		if (view->cursor.column == 0) {
			if (view->cursor.row == 0)
				break;
			view->cursor.row--;
			if (table->numActiveColumns == 0)
				view->cursor.column = 0;
			else
				view->cursor.column =
					table->numActiveColumns - 1;
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

	if (oldRow != view->cursor.row || oldColumn != view->cursor.column) {
		if (view->mode == TABLE_VIEW_COMMAND) {
			table_view_interruptcommand(view);
		} else {
			table_view_updatecell(view, oldRow, oldColumn);
			table_view_updatetext(view);
		}
	}
	table_view_updatecursor(view);
}

static inline int
table_view_executecommand(TableView *view, const char *cmd)
{
	static const struct command {
		const char *name;
		int arg;
	} commands[] = {
		/* 0x1: Needs an argument
		 * 0x2: Optional argument
		 * 0x4: Prints to stdin/stderr
		 */
		[TABLE_OPERATION_INFO] = { "info", 4 },
		[TABLE_OPERATION_PRINT] = { "print", 4 },
		[TABLE_OPERATION_OUTPUT] = { "output", 6 },
		[TABLE_OPERATION_INPUT] = { "input", 5 },

		[TABLE_OPERATION_ALL] = { "all", 0 },
		[TABLE_OPERATION_INVERT] = { "invert", 0 },
		[TABLE_OPERATION_ROW] = { "row", 1 },
		[TABLE_OPERATION_COLUMN] = { "column", 1 },
		[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0 },
		[TABLE_OPERATION_NO_COLUMNS] = { "no-columns", 0 },
		[TABLE_OPERATION_NONE] = { "none", 0 },
		[TABLE_OPERATION_SET_ROW] = { "set-row", 1 },
		[TABLE_OPERATION_SET_COLUMN] = { "set-column", 1 },

		[TABLE_OPERATION_APPEND] = { "append", 2 },
		[TABLE_OPERATION_APPEND_COLUMN] = { "append-column", 1 },

		{ "quit", 0 },
	};

	const struct command *command = NULL;
	const char *name, *arg, *end;

	while (*cmd == ' ')
		cmd++;
	name = cmd;
	if (*cmd != '\0') {
		end = strchr(cmd, ' ');
		if (end == NULL)
			end = cmd + strlen(cmd);
		for (size_t i = 0; i < ARRLEN(commands); i++) {
			if (commands[i].name == NULL)
				continue;
			if (!strncmp(commands[i].name,
						cmd, end - cmd) &&
					commands[i].name[end - cmd]
						== '\0') {
				command = &commands[i];
				break;
			}
		}
	}
	if (command == NULL) {
		table_view_showerror(view, "command '%s' does not exist", name);
		return -1;
	}

	if (!strcmp(command->name, "quit")) {
		view->quit = true;
		return 0;
	}

	arg = end;
	while (*arg == ' ')
		arg++;

	if ((command->arg & 0x1) && *arg == '\0') {
		table_view_showerror(view, "command '%s' expected argument",
				command->name);
		return -1;
	}
	if (!(command->arg & 0x3) && *arg != '\0') {
		table_view_showerror(view,
				"command '%s' does not expect an argument",
				command->name);
		return -1;
	}
	if (command->arg & 0x4)
		endwin();
	table_dooperation(view->table, (enum table_operation)
			(command - commands), arg);
	if (command->arg & 0x4) {
		fprintf(stderr, "\nPress enter to continue...\n");
		while (getchar() != '\n');
	}
	view->cursor.row = MIN(view->cursor.row, view->table->numActiveRows);
	view->cursor.column = MIN(view->cursor.column,
		view->table->numActiveColumns);
	table_view_updatecolumns(view);
	return 0;
}

static inline void table_view_inserttext(TableView *view, char *buf, int count)
{
	if (view->cursor.capText < view->cursor.lenText + count + 1) {
		Utf8 *newText;

		view->cursor.capText += count;
		newText = table_view_realloc(view, view->cursor.text, view->cursor.capText + 1);
		if (newText == NULL)
			return;
		view->cursor.text = newText;
	}
	memmove(&view->cursor.text[view->cursor.index + count],
		&view->cursor.text[view->cursor.index],
		view->cursor.lenText - view->cursor.index);
	memcpy(&view->cursor.text[view->cursor.index], buf, count);
	view->cursor.index += count;
	view->cursor.lenText += count;
	view->cursor.text[view->cursor.lenText] = '\0';
	table_view_updatecursor(view);
}

static inline void table_view_typehotkey(TableView *view, int c)
{
	switch (c) {
		size_t count;

	case '\n':
		if (view->mode == TABLE_VIEW_COMMAND) {
			table_view_executecommand(view, view->cursor.text);
			table_view_interruptcommand(view);
		} else {
			view->mode = TABLE_VIEW_NORMAL;
			table_view_updatecell(view, view->cursor.row,
					view->cursor.column);
		}
		return;

	/* Ctrl + U */
	case 'U' - 'A' + 1:
		memmove(&view->cursor.text[0],
			&view->cursor.text[view->cursor.index],
			view->cursor.lenText + 1 - view->cursor.index);
		view->cursor.lenText -= view->cursor.index;
		view->cursor.index = 0;
		table_view_updatecursor(view);
		return;

	case 0x7f:
	case KEY_BACKSPACE:
		if (view->cursor.index == 0) {
			if (view->cursor.lenText == 0 &&
					view->mode == TABLE_VIEW_COMMAND) {
				view->mode = TABLE_VIEW_NORMAL;
				table_view_updatetext(view);
			}
			return;
		}
		count = utf8_previouslength(
				&view->cursor.text[view->cursor.index]);
		view->cursor.index -= count;
		memmove(&view->cursor.text[view->cursor.index],
			&view->cursor.text[view->cursor.index + count],
			view->cursor.lenText + 1 - count -
				view->cursor.index);
		view->cursor.lenText -= count;
		table_view_updatecursor(view);
		break;
	case KEY_DC:
		if (view->cursor.index == view->cursor.lenText)
			return;
		count = utf8_nextlength(
				&view->cursor.text[view->cursor.index]);
		memmove(&view->cursor.text[view->cursor.index],
			&view->cursor.text[view->cursor.index + count],
			view->cursor.lenText + 1 - view->cursor.index);
		view->cursor.lenText -= count;
		table_view_updatecursor(view);
		break;
	}
}

static void table_view_type(TableView *view, int c, char *buf)
{
	if (buf != NULL)
		table_view_inserttext(view, buf, c);
	else if (c >= 0x20 && c < 0x7f)
		table_view_inserttext(view, &(char) { c }, 1);
	else
		table_view_typehotkey(view, c);
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

	case 'g':
		view->scroll.y = 0;
		break;
	case 'G':
		if (table->numActiveRows < (size_t) LINES)
			break;
		view->scroll.y = table->numActiveRows - (LINES - 1);
		break;
	}
}

static void table_view_hotkey(TableView *view, int c)
{
	Table *const table = view->table;
	switch (c) {
	case 'd':
		if (table->numActiveRows == 0)
			break;
		table->numActiveRows--;
		memmove(&table->activeRows[view->cursor.row],
			&table->activeRows[view->cursor.row + 1],
			sizeof(*table->activeRows) *
			(table->numActiveRows - view->cursor.row));
		if (view->cursor.row > 0 &&
				view->cursor.row == table->numActiveRows)
			view->cursor.row--;
		break;
	case 'x':
		if (table->numActiveColumns == 0)
			break;
		table->numActiveColumns--;
		memmove(&table->activeColumns[view->cursor.column],
			&table->activeColumns[view->cursor.column + 1],
			sizeof(*table->activeColumns) *
			(table->numActiveColumns - view->cursor.column));
		if (view->cursor.column > 0 &&
				view->cursor.column ==
					table->numActiveColumns)
			view->cursor.column--;
		break;

	case 'A':
		table_parseline(table, "");
		view->cursor.column = 0;
		table->activeRows[table->numActiveRows++] =
			table->numRows - 1;
		view->cursor.row = table->numActiveRows - 1;
		table_view_updatetext(view);
		break;

	case '>':
		if (table->numActiveColumns == 0)
			break;
		if (view->columnWidths[view->cursor.column] == (size_t) COLS)
			break;
		view->columnWidths[view->cursor.column]++;
		break;
	case '<':
		if (table->numActiveColumns == 0)
			break;
		if (view->columnWidths[view->cursor.column] == 1)
			break;
		view->columnWidths[view->cursor.column]--;
		break;

	case 'I':
	case 'i':
		view->mode = TABLE_VIEW_INSERT;
		break;

	case ':':
		view->mode = TABLE_VIEW_COMMAND;
		if (view->cursor.capText == 0) {
			view->cursor.text = malloc(1);
			if (view->cursor.text != NULL) {
				view->cursor.capText = 1;
				view->cursor.text[0] = '\0';
			}
		} else {
			view->cursor.text[0] = '\0';
		}
		view->cursor.lenText = 0;
		view->cursor.index = 0;
		view->cursor.scroll = 0;
		break;

	case 'Q':
		view->quit = true;
		break;
	}
}

int table_view_show(TableView *view)
{
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, true);

	refresh();
	while (!view->quit) {
		int c;

		table_view_render(view);
		c = getch();
		if (c >= 0x7f && c <= 0xff) {
			char buf[8];
			int det;

			buf[0] = c;
			det = (int) utf8_determinate(c);
			for (int i = 1; i < det; i++)
				buf[i] = getch();
			table_view_type(view, det, buf);
			continue;
		}
		if (c == 0x1b) {
			view->mode = TABLE_VIEW_NORMAL;
			table_view_updatetext(view);
			continue;
		}

		table_view_movecursor(view, c);
		switch (view->mode) {
		case TABLE_VIEW_INSERT:
		case TABLE_VIEW_COMMAND:
			table_view_type(view, c, NULL);
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
	if (view->cursor.capText > 0)
		free(view->cursor.text);
	free(view->columnWidths);
}
