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
	Utf8 **columnNames;
	Utf8 ***cells;
	size_t numRows;
	size_t numColumns;
	size_t *activeRows;
	size_t numActiveRows;
	size_t *activeColumns;
	size_t numActiveColumns;
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
	TABLE_OPERATION_INVERT,
	TABLE_OPERATION_ROW,
	TABLE_OPERATION_COLUMN,
	TABLE_OPERATION_NO_ROWS,
	TABLE_OPERATION_NO_COLUMNS,
	TABLE_OPERATION_NONE,
	TABLE_OPERATION_SET_ROW,
	TABLE_OPERATION_SET_COLUMN,

	TABLE_OPERATION_APPEND,
	TABLE_OPERATION_APPEND_COLUMN,
};

void table_dooperation(Table *table, enum table_operation operation, const void *arg);

void table_printinfo(Table *table);
int table_printbeautiful(Table *table);
void table_printactivecells(Table *table);
int table_writeout(Table *table, const char *path);
int table_readin(Table *table, const char *path);

void table_activateall(Table *table);
void table_invert(Table *table);
void table_filterrows(Table *table, const Utf8 *filter);
void table_filtercolumns(Table *table, const Utf8 *filter);

int table_appendcolumn(Table *table, const char *name);

enum table_view_mode {
	TABLE_VIEW_NORMAL,
	TABLE_VIEW_INSERT,
	TABLE_VIEW_COMMAND,
};

typedef struct table_view {
	Table *table;
	enum table_view_mode mode;
	size_t *columnWidths;
	struct {
		size_t row;
		size_t column;

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
