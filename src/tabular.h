#ifndef TABULAR_H
#define TABULAR_H

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <locale.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

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

typedef struct table {
	const char *atText;
	char **columnNames;
	char ***cells;
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
	TABLE_OPERATION_ALL,
	TABLE_OPERATION_INFO,
	TABLE_OPERATION_ROW,
	TABLE_OPERATION_COLUMN,
	TABLE_OPERATION_NO_ROWS,
	TABLE_OPERATION_NO_COLUMNS,
	TABLE_OPERATION_SET_ROW,
	TABLE_OPERATION_SET_COLUMN,
	TABLE_OPERATION_SUM,
	TABLE_OPERATION_VIEW,
	TABLE_OPERATION_PRINT,
};

void table_dooperation(Table *table, enum table_operation operation, void *arg);
void table_activateall(Table *table);
void table_filterrows(Table *table, const char *filter);
void table_filtercolumns(Table *table, const char *filter);
int table_foreach(Table *table,
		int (*proc)(Table *table, const char *cell, void *arg),
		void *arg);
void table_printactivecells(Table *table);
int table_printbeautiful(Table *table);
void table_dumpinfo(Table *table);

typedef struct table_view {
	Table *table;
	/* pad regions for columns */
	WINDOW **pads;
	size_t verticalColumnTracking;
	struct {
		size_t row;
		size_t column;
		size_t subColumn;
	} cursor;
	struct {
		size_t x;
		size_t y;
	} scroll;

} TableView;

int table_view_init(TableView *view, Table *table);
int table_view_show(TableView *view);
void table_view_uninit(TableView *view);

#endif
