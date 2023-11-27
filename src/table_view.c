#include "tabular.h"

static void table_view_showerror(TableView *view, const char *fmt, ...)
{
	va_list l;

	(void) view;

	endwin();

	printf("An error occured!!\n");

	va_start(l, fmt);
	vprintf(fmt, l);
	va_end(l);

	printf("\n\nPress enter to continue...\n");
	while (getchar() != '\n');
}


static void *table_view_realloc(TableView *view, void *ptr, size_t newSize)
{
	char choice;

	(void) view;
retry:
	ptr = realloc(ptr, newSize);
	if (ptr != NULL)
		return ptr;
	endwin();
	printf(">>> failed allocating '%zu' bytes <<<\n", newSize);
	printf("note: continuing should not crash or break the program but impact its usability\n");
	do {
		printf("should the program continue (yn) or retry (r)? [ynr]\n");
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

int table_view_init(TableView *view, Table *table)
{
	memset(view, 0, sizeof(*view));
	view->table = table;
	if (table->numActiveRows > 0 && table->numActiveColumns > 0) {
		char *const cell = table->cells[table->activeRows[0]]
			[table->activeColumns[0]];
		view->cursor.lenText = strlen(cell);
		view->cursor.capText = MAX((size_t) 512, view->cursor.lenText);
		view->cursor.text = table_view_realloc(view, NULL,
				view->cursor.capText);
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
	addnstr(view->cursor.text, view->cursor.index);
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

static void table_view_updatetext(TableView *view)
{
	Utf8 *cell;

	Table *const table = view->table;
	cell = table->cells[table->activeRows[view->cursor.row]]
		[table->activeColumns[view->cursor.column]];
	view->cursor.lenText = strlen(cell);
	if (view->cursor.capText < view->cursor.lenText) {
		Utf8 *newText;

		newText = table_view_realloc(view, view->cursor.text,
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
	pOldCell = &table->cells[table->activeRows[row]]
		[table->activeColumns[column]];
	newCell = table_view_realloc(view, *pOldCell, view->cursor.lenText + 1);
	if (newCell != NULL) {
		*pOldCell = newCell;
		memcpy(newCell, view->cursor.text, view->cursor.lenText);
		newCell[view->cursor.lenText] = '\0';
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
			view->cursor.column = table->numActiveColumns - 1;
		else
			view->cursor.index = view->cursor.lenText;
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

static const struct table_view_command {
	const char *name;
	int arg;
} table_view_commands[] = {
	[TABLE_OPERATION_OUTPUT] = { "output", 2 },

	[TABLE_OPERATION_ALL] = { "all", 0 },
	[TABLE_OPERATION_INVERT] = { "invert", 0 },
	[TABLE_OPERATION_ROW] = { "row", 1 },
	[TABLE_OPERATION_COLUMN] = { "column", 1 },
	[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0 },
	[TABLE_OPERATION_NO_COLUMNS] = { "no-columns", 0 },
	[TABLE_OPERATION_NONE] = { "none", 0 },
	[TABLE_OPERATION_SET_ROW] = { "set-row", 1 },
	[TABLE_OPERATION_SET_COLUMN] = { "set-column", 1 },

	[TABLE_OPERATION_APPEND] = { "append", 1 },

	[TABLE_OPERATION_QUIT] = { "quit", 0 },
};

static void table_view_execute(TableView *view,
	       const struct table_view_command *command,
	       const char *arg)
{
	if (command->arg == 1 && *arg == '\0') {
		table_view_showerror(view, "command '%s' expected argument",
				command->name);
		return;
	}
	if (command->arg == 2)
		endwin();
	table_dooperation(view->table, (enum table_operation)
			(command - table_view_commands), arg);
	if (command->arg == 2) {
		printf("\nPress enter to continue...\n");
		while (getchar() != '\n');
	}
	view->cursor.row = MIN(view->cursor.row, view->table->numActiveRows);
	view->cursor.column = MIN(view->cursor.column,
		view->table->numActiveColumns);
}

static void table_view_type(TableView *view, int c)
{
	if (view->cursor.capText == 0)
		return;
	switch (c) {
		size_t count;

	case '\n':
		if (view->mode == TABLE_VIEW_COMMAND) {
			Utf8 *cmd;
			Utf8 *end;
			const struct table_view_command *command = NULL;

			/* execute command */
			if (view->cursor.capText == view->cursor.lenText) {
				Utf8 *newText;

				view->cursor.capText++;
				newText = table_view_realloc(view, view->cursor.text,
						view->cursor.capText);
				if (newText == NULL) {
					table_view_showerror(view, "failed allocating: %s",
							strerror(errno));
					return;
				}
				view->cursor.text = newText;
			}
			view->cursor.text[view->cursor.lenText] = '\0';
			cmd = view->cursor.text;
			while (*cmd == ' ')
				cmd++;
			if (*cmd != '\0') {
				end = strchr(cmd, ' ');
				if (end == NULL)
					end = cmd + strlen(cmd);
				/* RB 80 */
				for (size_t i = 0; i < ARRLEN(table_view_commands); i++) {
					if (table_view_commands[i].name == NULL)
						continue;
					if (!strncmp(table_view_commands[i].name, cmd, end - cmd) &&
							table_view_commands[i].name[end - cmd] == '\0') {
						command = &table_view_commands[i];
						break;
					}
				}
			}
			if (command != NULL) {
				while (*end == ' ')
					end++;
				table_view_execute(view, command, end);
			} else {
				*end = '\0';
				table_view_showerror(view,
						"command '%s' does not exist",
						cmd);
			}

			table_view_interruptcommand(view);
		} else {
			table_view_updatecell(view, view->cursor.row,
					view->cursor.column);
		}
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
		newText = table_view_realloc(view, view->cursor.text, view->cursor.capText);
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
		table_view_updatetext(view);
		break;

	case '>':
		if (view->columnWidths[view->cursor.column] == (size_t) COLS)
			break;
		view->columnWidths[view->cursor.column]++;
		break;
	case '<':
		if (view->columnWidths[view->cursor.column] == 1)
			break;
		view->columnWidths[view->cursor.column]--;
		break;

	case ':':
		view->mode = TABLE_VIEW_COMMAND;
		view->cursor.lenText = 0;
		view->cursor.index = 0;
		view->cursor.scroll = 0;
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
		if (c == 0x1b) {
			view->mode = TABLE_VIEW_NORMAL;
			continue;
		}

		table_view_movecursor(view, c);
		switch (view->mode) {
		case TABLE_VIEW_INSERT:
		case TABLE_VIEW_COMMAND:
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
