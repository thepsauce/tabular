#include "tabular.h"

void table_dooperation(Table *table, enum table_operation operation, const void *arg)
{
	switch (operation) {
	case TABLE_OPERATION_INFO:
		table_printinfo(table);
		break;
	case TABLE_OPERATION_VIEW:
		table_printbeautiful(table);
		break;
	case TABLE_OPERATION_PRINT:
		table_printactivecells(table);
		break;
	case TABLE_OPERATION_OUTPUT:
		table_writeout(table, arg);
		break;
	case TABLE_OPERATION_INPUT:
		table_readin(table, arg);
		break;

	case TABLE_OPERATION_ALL:
		table_activateall(table);
		break;
	case TABLE_OPERATION_INVERT:
		table_invert(table);
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
	case TABLE_OPERATION_NONE:
		table->numActiveRows = 0;
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

	case TABLE_OPERATION_APPEND:
		table_parseline(table, arg == NULL ? "" : arg);
		break;
	case TABLE_OPERATION_APPEND_COLUMN:
		table_appendcolumn(table, arg == NULL ? "" : arg);
		break;
	}
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

void table_printinfo(Table *table)
{
	printf("%zu %zu\n", table->numActiveColumns, table->numActiveRows);
	for (size_t i = 0; i < table->numActiveColumns; i++) {
		const size_t column = table->activeColumns[i];
		printf("%s\n", table->columnNames[column]);
	}
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

int table_writeout(Table *table, const char *path)
{
	FILE *fp;

	if (path != NULL && *path != '\0') {
		fp = fopen(path, "w");
		if (fp == NULL) {
			fprintf(stderr, "unable to open '%s': %s\n",
					path, strerror(errno));
			return -1;
		}
	} else {
		fp = stdout;
	}

	for (size_t i = 0; i < table->numActiveColumns; i++) {
		const size_t column = table->activeColumns[i];
		if (i > 0)
			fputc(';', fp);
		fprintf(fp, "\"%s\"", table->columnNames[column]);
	}
	fputc('\n', fp);
	for (size_t i = 0; i < table->numActiveRows; i++) {
		const size_t row = table->activeRows[i];
		for (size_t j = 0; j < table->numActiveColumns; j++) {
			const size_t column = table->activeColumns[j];
			if (j > 0)
				fputc(';', fp);
			fprintf(fp, "\"%s\"", table->cells[row][column]);
		}
		fputc('\n', fp);
	}
	if (fp != stdout)
		fclose(fp);
	return 0;
}

int table_readin(Table *table, const char *path)
{
	FILE *fp;
	char *line = NULL;
	size_t capacity;
	ssize_t count;
	size_t lineIndex;

	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "unable to open '%s': %s\n",
				path, strerror(errno));
		return -1;
	}
	lineIndex = 0;
	while ((count = getline(&line, &capacity, fp)) >= 0) {
		if (count <= 1)
			continue;
		line[count - 1] = '\0';
		if (table_parseline(table, line) < 0) {
			fprintf(stderr, " at line no. %zu\n%s\n",
					lineIndex + 1, line);
			if (table->atText != NULL) {
				for (char *s = line; s != table->atText; s++)
					fprintf(stderr, "~");
				fprintf(stderr, "^\n");
			}
			free(line);
			fclose(fp);
			return -1;
		}
		lineIndex++;
	}
	free(line);
	fclose(fp);
	return 0;
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

void table_invert(Table *table)
{
	for (size_t i = 0, j = table->numActiveRows; i < table->numRows; i++) {
		size_t k;

		for (k = 0; k < table->numActiveRows; k++)
			if (table->activeRows[k] == i)
				break;
		if (k != table->numActiveRows)
			continue;
		table->activeRows[j++] = i;
	}
	memmove(&table->activeRows[0],
		&table->activeRows[table->numActiveRows],
		sizeof(*table->activeRows) *
			(table->numRows - table->numActiveRows));
	table->numActiveRows = table->numRows - table->numActiveRows;

	for (size_t i = 0, j = table->numActiveColumns; i < table->numColumns; i++) {
		size_t k;

		for (k = 0; k < table->numActiveColumns; k++)
			if (table->activeColumns[k] == i)
				break;
		if (k != table->numActiveColumns)
			continue;
		table->activeColumns[j++] = i;
	}
	memmove(&table->activeColumns[0],
		&table->activeColumns[table->numActiveColumns],
		sizeof(*table->activeColumns) *
			(table->numColumns - table->numActiveColumns));
	table->numActiveColumns = table->numColumns - table->numActiveColumns;
}

static inline void table_activaterow(Table *table, size_t row)
{
	for (size_t i = 0; i < table->numActiveRows; i++)
		if (table->activeRows[i] == row)
			return;
	table->activeRows[table->numActiveRows++] = row;
}

void table_filterrows(Table *table, const Utf8 *filter)
{
	for (size_t row = 0; row < table->numRows; row++)
		for (size_t i = 0; i < table->numActiveColumns; i++) {
			const size_t column = table->activeColumns[i];
			if (!utf8_match(filter, table->cells[row][column]))
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

void table_filtercolumns(Table *table, const Utf8 *filter)
{
	for (size_t i = 0; i < table->numColumns; i++)
		if (utf8_match(filter, table->columnNames[i]))
			table_activatecolumn(table, i);
}

int table_appendcolumn(Table *table, const char *name)
{
	char **newColumnNames;
	size_t *newActiveColumns;

	newColumnNames = realloc(table->columnNames,
			sizeof(*table->columnNames) * (table->numColumns + 1));
	if (newColumnNames == NULL)
		return -1;
	table->columnNames = newColumnNames;
	table->columnNames[table->numColumns] = strdup(name);
	if (table->columnNames[table->numColumns] == NULL)
		return -1;
	for (size_t i = 0; i < table->numRows; i++) {
		char **newCells;

		newCells = realloc(table->cells[i], sizeof(*table->cells[i]) *
				(table->numColumns + 1));
		if (newCells == NULL)
			return -1;
		table->cells[i] = newCells;
		table->cells[i][table->numColumns] = strdup("");
		if (table->cells[i][table->numColumns] == NULL) {
			while (i > 0) {
				i--;
				free(table->cells[i][table->numColumns]);
			}
			return -1;
		}
	}
	newActiveColumns = realloc(table->activeColumns,
			sizeof(*table->activeColumns) *
				(table->numColumns + 1));
	if (newActiveColumns == NULL)
		return -1;
	table->activeColumns = newActiveColumns;
	table->numColumns++;
	return 0;
}
