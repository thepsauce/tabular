#ifndef TABULAR_H
#define TABULAR_H

#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <wchar.h>

#define ARRLEN(a) (sizeof(a)/sizeof*(a))

#define MAX(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _a : _b; \
})
#define MIN(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _a : _b; \
})

#include "utf8.h"

typedef struct table {
	const Utf8 *atText;
	Utf8 **colNames;
	Utf8 ***cells;
	size_t numRows;
	size_t numCols;

	size_t *activeRows;
	size_t numActiveRows;
	size_t *activeCols;
	size_t numActiveCols;

	size_t *newActiveRows;
	size_t newNumActiveRows;
	size_t *newActiveCols;
	size_t newNumActiveCols;

	struct table_diff {
		struct table_diff_row {
			size_t index;
			size_t row;
		} *rowDiff;
		size_t numChangedRows;
		struct table_diff_column {
			size_t index;
			size_t col;
		} *colDiff;
		size_t numChangedCols;
	} *history;
	size_t indexHistory;
	size_t numHistory;
} Table;

int table_init(Table *table);
const char *table_strerror(Table *table);
int table_parseline(Table *table, const char *line);
void table_uninit(Table *table);

enum table_operation {
	TABLE_OPERATION_INFO,
	TABLE_OPERATION_VIEW,
	TABLE_OPERATION_PRINT,
	TABLE_OPERATION_OUTPUT,
	TABLE_OPERATION_INPUT,

	TABLE_OPERATION_ALL,
	TABLE_OPERATION_ALL_ROWS,
	TABLE_OPERATION_ALL_COLS,
	TABLE_OPERATION_INVERT,
	TABLE_OPERATION_INVERT_ROWS,
	TABLE_OPERATION_INVERT_COLS,
	TABLE_OPERATION_NONE,
	TABLE_OPERATION_NO_ROWS,
	TABLE_OPERATION_NO_COLS,

	TABLE_OPERATION_ROW,
	TABLE_OPERATION_COL,
	TABLE_OPERATION_SET_ROW,
	TABLE_OPERATION_SET_COL,

	TABLE_OPERATION_APPEND,
	TABLE_OPERATION_APPEND_COL,

	TABLE_OPERATION_UNDO,
	TABLE_OPERATION_REDO,
};

void table_dooperation(Table *table, enum table_operation operation, const void *arg);
/* If functions outside of table_operations.c want to make
 * undoable changes to the table, they need to use
 * on of these functions.
 * Note: table_generatediff calls table_appendhistory
 * Note 2: Passing a diff argument into table_appendhistory
 * will make you lose ownership of the memory, all memory is
 * handled by Table from that moment on.
 */
void table_validatediff(Table *table, struct table_diff *diff);
int table_generatediff(Table *table);
int table_appendhistory(Table *table, const struct table_diff *diff);
void table_applydiff(Table *table, const struct table_diff *diff);

enum table_view_mode {
	TABLE_VIEW_NORMAL,
	TABLE_VIEW_INSERT,
	TABLE_VIEW_COMMAND,
};

typedef struct table_view {
	Table *table;
	enum table_view_mode mode;
	size_t *colWidths;
	struct {
		size_t row;
		size_t col;

		Utf8 *text;
		size_t index;
		size_t indexTracker;
		size_t scroll;
		size_t lenText;
		size_t capText;
	} cursor;
	struct {
		size_t x;
		size_t y;
	} scroll;
	bool quit;
} TableView;

int table_view_init(TableView *view, Table *table);
int table_view_show(TableView *view);
void table_view_uninit(TableView *view);

#endif
