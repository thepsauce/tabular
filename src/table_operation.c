#include "tabular.h"

static bool string_match(const char *pattern, const char *text)
{
	while (*pattern != '\0' || *text != '\0') {
		if (*pattern != *text) {
			if (*pattern == '*') {
				do
					pattern++;
				while (*pattern == '*');
				if (*pattern == '\0')
					return true;
				while (*text != '\0') {
					if (string_match(pattern, text))
						return true;
					text++;
				}
			}
			return false;
		}
		pattern++;
		text++;
	}
	return true;
}

static int cell_add(Table *table, const char *cell, void *arg)
{
	int64_t n, m = 0, m2 = 0;

	(void) table;
	int64_t *const sums = (int64_t*) arg;
	sscanf(cell, "%" PRId64 ",%" PRId64, &n, &m);
	sscanf(cell, "%" PRId64 ".%" PRId64, &n, &m2);
	if (m != m2 && m == 0)
		m = m2;
	sums[0] += n;
	sums[1] += m;
	return 0;
}

void table_dooperation(Table *table, enum table_operation operation, void *arg)
{
	switch (operation) {
	case TABLE_OPERATION_ALL:
		table_activateall(table);
		break;
	case TABLE_OPERATION_INFO:
		table_dumpinfo(table);
		break;
	case TABLE_OPERATION_ROW:
		table_filterrows(table, arg);
		break;
	case TABLE_OPERATION_COLUMN:
		table_filtercolumns(table, arg);
		break;
	case TABLE_OPERATION_NO_ROWS:
		table->numActiveRows = 0;
		break;
	case TABLE_OPERATION_NO_COLUMNS:
		table->numActiveColumns = 0;
		break;
	case TABLE_OPERATION_SET_ROW:
		table->numActiveRows = 0;
		table_filterrows(table, arg);
		break;
	case TABLE_OPERATION_SET_COLUMN:
		table->numActiveColumns = 0;
		table_filtercolumns(table, arg);
		break;
	case TABLE_OPERATION_VIEW:
		table_printbeautiful(table);
		break;
	case TABLE_OPERATION_PRINT:
		table_printactivecells(table);
		break;
	case TABLE_OPERATION_SUM: {
		int64_t sums[2] = { 0, 0 };

		table_foreach(table, cell_add, sums);
		sums[0] += sums[1] / 100;
		sums[1] %= 100;
		printf("%" PRId64 ",%" PRId64 "\n", sums[0], sums[1]);
		break;
	}
	}
}

void table_activateall(Table *table)
{
	for (size_t i = 0; i < table->numRows; i++)
		table->activeRows[i] = i;
	table->numActiveRows = table->numRows;
	for (size_t i = 0; i < table->numColumns; i++)
		table->activeColumns[i] = i;
	table->numActiveColumns = table->numColumns;
}

static inline void table_activaterow(Table *table, size_t row)
{
	for (size_t i = 0; i < table->numActiveRows; i++)
		if (table->activeRows[i] == row)
			return;
	table->activeRows[table->numActiveRows++] = row;
}

void table_filterrows(Table *table, const char *filter)
{
	for (size_t row = 0; row < table->numRows; row++)
		for (size_t i = 0; i < table->numActiveColumns; i++) {
			const size_t column = table->activeColumns[i];
			if (!string_match(filter, table->cells[row][column]))
				continue;
			table_activaterow(table, row);
			break;
		}
}

static inline void table_activatecolumn(Table *table, size_t column)
{
	for (size_t i = 0; i < table->numActiveColumns; i++)
		if (table->activeColumns[i] == column)
			return;
	table->activeColumns[table->numActiveColumns++] = column;
}

void table_filtercolumns(Table *table, const char *filter)
{
	for (size_t i = 0; i < table->numColumns; i++)
		if (string_match(filter, table->columnNames[i]))
			table_activatecolumn(table, i);
}

int table_foreach(Table *table,
		int (*proc)(Table *table, const char *cell, void *arg),
		void *arg)
{
	for (size_t i = 0; i < table->numActiveRows; i++) {
		const size_t row = table->activeRows[i];
		for (size_t j = 0; j < table->numActiveColumns; j++) {
			const size_t column = table->activeColumns[j];
			if (proc(table, table->cells[row][column], arg) == 1)
				return 1;
		}
	}
	return 0;
}

void table_printactivecells(Table *table)
{
	if (table->numActiveColumns == 1) {
		for (size_t i = 0; i < table->numActiveRows; i++) {
			const size_t row = table->activeRows[i];
			const size_t column = table->activeColumns[0];
			printf("%s\n", table->cells[row][column]);
		}
		return;
	}
	for (size_t i = 0; i < table->numActiveColumns; i++) {
		const size_t column = table->activeColumns[i];
		printf("%s\n", table->columnNames[column]);
		for (size_t j = 0; j < table->numActiveRows; j++) {
			const size_t row = table->activeRows[j];
			printf("\t%s\n", table->cells[row][column]);
		}
	}
}

void table_dumpinfo(Table *table)
{
	printf("%zu %zu\n", table->numColumns, table->numRows);
	for (size_t x = 0; x < table->numColumns; x++)
		printf("%s\n", table->columnNames[x]);
}

int table_printbeautiful(Table *table)
{
	TableView view;

	if (table_view_init(&view, table) != 0)
		return -1;
	if (table_view_show(&view) != 0)
		return -1;
	table_view_uninit(&view);
	return 0;
}

