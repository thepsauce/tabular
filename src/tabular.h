#ifndef TABULAR_H
#define TABULAR_H

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

struct table {
	char **columnNames;
	char ***cells;
	size_t numRows;
	size_t numColumns;
	size_t *activeRows;
	size_t numActiveRows;
	size_t *activeColumns;
	size_t numActiveColumns;
};

int table_init(struct table *table);
int table_parseline(struct table *table, const char *line);
void table_filterrows(struct table *table, const char *filter);
void table_filtercolumns(struct table *table, const char *filter);
int table_foreach(struct table *table,
		int (*proc)(struct table *table, const char *cell, void *arg),
		void *arg);
void table_uninit(struct table *table);

void table_printactivecells(struct table *table);
void table_dumpinfo(struct table *table);

#endif
