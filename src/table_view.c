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

static int table_view_updatecols(TableView *view)
{
	size_t *newColumnWidths;
	struct fitting fit;

	const int cols = COLS < 28 ? 28 : COLS;
	const size_t mostMaxWidth = cols / 4 + ((cols / 4) & 1);

	Table *const table = view->table;

	newColumnWidths = table_view_realloc(view, view->colWidths,
			sizeof(*view->colWidths) *
				table->numActiveCols);
	if (newColumnWidths == NULL)
		return -1;
	view->colWidths = newColumnWidths;

	for (size_t i = 0; i < table->numActiveCols; i++) {
		size_t maxWidth;

		const size_t col = table->activeCols[i];
		const char *const name = table->colNames[col];
		utf8_getfitting(name, mostMaxWidth, &fit);
		maxWidth = fit.width;

		for (size_t j = 0; j < table->numActiveRows; j++) {
			const size_t row = table->activeRows[j];
			const char *cell = table->cells[row][col];
			utf8_getfitting(cell, mostMaxWidth, &fit);
			if (fit.width > maxWidth)
				maxWidth = fit.width;
			if (maxWidth == mostMaxWidth)
				break;
		}
		view->colWidths[i] = MAX(maxWidth, (size_t) 1);
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

	if (view->table->numActiveCols == 0) {
		view->scroll.x = 0;
		view->scroll.y = 0;
		return;
	}

	begx = 0;
	for (size_t i = 0; i < view->cursor.col; i++) {
		maxx = view->colWidths[i];
		begx += maxx + 1;
	}
	maxx = view->colWidths[view->cursor.col];

	if (begx < view->scroll.x)
		view->scroll.x = begx;
	else if (begx + maxx > view->scroll.x + (COLS - 1))
		view->scroll.x = begx + maxx - (COLS - 1);

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
			table->numActiveCols == 0) {
		cell = "";
	} else {
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeCols[view->cursor.col]];
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
	table_view_updatecols(view);
	table_view_updatetext(view);
	return 0;
}

static void table_view_render(TableView *view)
{
	struct fitting fit;
	size_t x, y;
	size_t sx;
	size_t colsx, colex;
	size_t width;
	(void) colsx;
	(void) colex;

	Table *const table = view->table;

	erase();
	x = 0;
	for (size_t i = 0; i < table->numActiveCols;
			x += view->colWidths[i] + 1, i++) {
		const size_t col = table->activeCols[i];
		const char *const title = table->colNames[col];

		width = view->colWidths[i];
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
		if (i == view->cursor.col) {
			colsx = sx;
			colex = sx + width;
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
			const char *cell = table->cells[row][col];
			if (i == view->cursor.col && j == view->cursor.row) {
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

static void table_view_updatecell(TableView *view, size_t row, size_t col)
{
	Utf8 **pOldCell;
	Utf8 *newCell;

	Table *const table = view->table;
	if (table->numActiveRows == 0 || table->numActiveCols == 0)
		return;
	pOldCell = &table->cells[table->activeRows[row]]
		[table->activeCols[col]];
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
	const size_t oldColumn = view->cursor.col;
	switch (c) {
	case 'k':
	case KEY_UP:
		if (view->cursor.row == 0)
			break;
		view->cursor.row--;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeCols[view->cursor.col]];
		end = strlen(cell);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;
	case 'j':
	case KEY_DOWN:
		if (view->cursor.row + 1 >= table->numActiveRows)
			break;
		view->cursor.row++;
		cell = table->cells[table->activeRows[view->cursor.row]]
			[table->activeCols[view->cursor.col]];
		end = strlen(cell);
		view->cursor.index = MIN(view->cursor.indexTracker, end);
		view->cursor.scroll = 0;
		break;

	case 'h':
	case KEY_LEFT:
		if (view->cursor.col == 0)
			break;
		view->cursor.col--;
		view->cursor.index = 0;
		view->cursor.scroll = 0;
		view->cursor.indexTracker = view->cursor.index;
		break;
	case 'l':
	case KEY_RIGHT:
		if (view->cursor.col + 1 >= table->numActiveCols)
			break;
		view->cursor.col++;
		view->cursor.scroll = 0;
		view->cursor.index = 0;
		view->cursor.indexTracker = view->cursor.index;
		break;

	case 'g':
		view->cursor.row = 0;
		break;
	case 'G':
		if (table->numActiveRows > 0)
			view->cursor.row = table->numActiveRows - 1;
		break;

	case '0':
	case KEY_HOME:
		view->cursor.col = 0;
		break;
	case '$':
	case KEY_END:
		view->cursor.col = table->numActiveCols == 0 ? 0 :
				table->numActiveCols - 1;
		break;
	case '\t':
	case ' ':
		if (view->cursor.col + 1 >= table->numActiveCols) {
			if (view->cursor.row + 1 >= table->numActiveRows)
				break;
			view->cursor.row++;
			view->cursor.col = 0;
		} else {
			view->cursor.col++;
		}
		view->cursor.index = 0;
		view->cursor.indexTracker = 0;
		view->cursor.scroll = 0;
		break;
	case KEY_BACKSPACE:
	case KEY_BTAB:
		if (view->cursor.col == 0) {
			if (view->cursor.row == 0)
				break;
			view->cursor.row--;
			if (table->numActiveCols == 0)
				view->cursor.col = 0;
			else
				view->cursor.col =
					table->numActiveCols - 1;
		} else {
			view->cursor.col--;
		}
		view->cursor.index = 0;
		view->cursor.indexTracker = 0;
		view->cursor.scroll = 0;
		break;
	default:
		return;
	}

	if (oldRow != view->cursor.row || oldColumn != view->cursor.col) {
		table_view_updatecell(view, oldRow, oldColumn);
		table_view_updatetext(view);
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
		[TABLE_OPERATION_OUTPUT] = { "write", 6 },
		[TABLE_OPERATION_INPUT] = { "read", 5 },

		[TABLE_OPERATION_ALL] = { "all", 0 },
		[TABLE_OPERATION_ALL_ROWS] = { "all-rows", 0 },
		[TABLE_OPERATION_ALL_COLS] = { "all-cols", 0 },
		[TABLE_OPERATION_INVERT] = { "invert", 0 },
		[TABLE_OPERATION_INVERT_ROWS] = { "invert-rows", 0 },
		[TABLE_OPERATION_INVERT_COLS] = { "invert-cols", 0 },
		[TABLE_OPERATION_NONE] = { "none", 0 },
		[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0 },
		[TABLE_OPERATION_NO_COLS] = { "no-cols", 0 },

		[TABLE_OPERATION_ROW] = { "row", 1 },
		[TABLE_OPERATION_COL] = { "col", 1 },
		[TABLE_OPERATION_SET_ROW] = { "set-row", 1 },
		[TABLE_OPERATION_SET_COL] = { "set-col", 1 },

		[TABLE_OPERATION_APPEND] = { "append", 2 },
		[TABLE_OPERATION_APPEND_COL] = { "append-col", 1 },

		[TABLE_OPERATION_UNDO] = { "undo", 0 },
		[TABLE_OPERATION_REDO] = { "redo", 0 },

		{ "quit", 0 },
	};
	static const struct abbreviation {
		const char *word;
		const char *abbreviation;
	} abbreviations[] = {
		{ "info", "i", },
		{ "print", "p", },
		{ "write", "w", },
		{ "read", "r", },

		{ "all", "A" },
		{ "all-rows", "Ar" },
		{ "all-cols", "Ac" },
		{ "invert", "I" },
		{ "invert-rows", "Ir" },
		{ "invert-cols", "Ic" },
		{ "none", "N" },
		{ "no-rows", "Nr" },
		{ "no-cols", "Nc" },

		{ "row", "r" },
		{ "col", "c" },
		{ "set-row", "sr" },
		{ "set-col", "sc" },

		{ "append", "a" },
		{ "append-col", "ac" },

		{ "undo", "U" },
		{ "redo", "R" },

		{ "quit", "q" },
	};

	const struct command *command = NULL;
	const char *arg, *end;
	size_t len;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0')
		return 0;

	end = strchr(cmd, ' ');
	if (end == NULL)
		end = cmd + strlen(cmd);
	len = end - cmd;

	for (size_t i = 0; i < ARRLEN(abbreviations); i++) {
		const char *const abbr = abbreviations[i].abbreviation;
		if (!strncmp(abbr, cmd, len) && abbr[len] == '\0') {
			cmd = abbreviations[i].word;
			len = strlen(cmd);
			break;
		}
	}
	for (size_t i = 0; i < ARRLEN(commands); i++) {
		const char *const name = commands[i].name;
		if (name == NULL)
			continue;
		if (!strncmp(name, cmd, len) && name[len] == '\0') {
			command = &commands[i];
			break;
		}
	}
	if (command == NULL) {
		table_view_showerror(view, "command '%.*s' does not exist", (int) len, cmd);
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
	view->cursor.col = MIN(view->cursor.col,
		view->table->numActiveCols);
	table_view_updatecols(view);
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
					view->cursor.col);
		}
		return;

	case KEY_LEFT:
		view->cursor.index -= utf8_previouslength(
			&view->cursor.text[view->cursor.index]);
		view->cursor.indexTracker = view->cursor.index;
		table_view_updatecursor(view);
		break;
	case KEY_RIGHT:
		view->cursor.index += utf8_nextlength(
			&view->cursor.text[view->cursor.index]);
		view->cursor.indexTracker = view->cursor.index;
		table_view_updatecursor(view);
		break;

	case KEY_HOME:
		view->cursor.index = 0;
		table_view_updatecursor(view);
		break;
	case KEY_END:
		view->cursor.index = view->cursor.lenText;
		table_view_updatecursor(view);
		break;

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

static void table_view_hotkey(TableView *view, int c)
{
	struct table_diff diff;
	enum table_operation opr;
	size_t prevColumns;

	Table *const table = view->table;
	switch (c) {
	case 'd':
		if (table->numActiveRows == 0)
			break;
		diff.rowDiff = malloc(sizeof(*diff.rowDiff));
		if (diff.rowDiff == NULL)
			break;
		diff.rowDiff[0].index = view->cursor.row;
		diff.numChangedRows = 1;
		diff.colDiff = 0;
		diff.numChangedCols = 0;
		table_validatediff(table, &diff);
		table_applydiff(table, &diff);
		table_appendhistory(table, &diff);

		if (view->cursor.row > 0 &&
				view->cursor.row == table->numActiveRows)
			view->cursor.row--;
		break;
	case 'x':
		if (table->numActiveCols == 0)
			break;

		diff.rowDiff = NULL;
		diff.numChangedRows = 0;
		diff.colDiff = malloc(sizeof(*diff.colDiff));
		if (diff.colDiff == NULL)
			break;
		diff.colDiff[0].index = view->cursor.col;
		diff.numChangedCols = 1;
		table_validatediff(table, &diff);
		table_applydiff(table, &diff);
		table_appendhistory(table, &diff);

		if (view->cursor.col > 0 &&
				view->cursor.col == table->numActiveCols)
			view->cursor.col--;
		break;

	case 'u':
	/* CTRL + R */
	case 'R' - 'A' + 1:
		opr = c == 'u' ? TABLE_OPERATION_UNDO :
			TABLE_OPERATION_REDO;
		prevColumns = table->numActiveCols;
		table_dooperation(table, opr, NULL);
		if (prevColumns < table->numActiveCols)
			table_view_updatecols(view);
		break;

	case 'A':
		table_parseline(table, "");
		view->cursor.col = 0;
		table->activeRows[table->numActiveRows++] =
			table->numRows - 1;
		view->cursor.row = table->numActiveRows - 1;
		table_view_updatetext(view);
		break;

	case '>':
		if (table->numActiveCols == 0)
			break;
		if (view->colWidths[view->cursor.col] == (size_t) COLS)
			break;
		view->colWidths[view->cursor.col]++;
		break;
	case '<':
		if (table->numActiveCols == 0)
			break;
		if (view->colWidths[view->cursor.col] == 1)
			break;
		view->colWidths[view->cursor.col]--;
		break;

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

	case 'r':
		table_view_updatecols(view);
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

		switch (view->mode) {
		case TABLE_VIEW_INSERT:
		case TABLE_VIEW_COMMAND:
			table_view_type(view, c, NULL);
			break;
		case TABLE_VIEW_NORMAL:
			table_view_movecursor(view, c);
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
	free(view->colWidths);
}
