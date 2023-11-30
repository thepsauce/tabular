#include "tabular.h"

int table_init(Table *table)
{
	memset(table, 0, sizeof(*table));
	return 0;
}

static void *table_realloc(Table *table, void *ptr, size_t newSize)
{
	ptr = realloc(ptr, newSize);
	if (ptr != NULL)
		return ptr;
	fprintf(stderr, "error: could not allocate %zu bytes: %s",
			newSize, strerror(errno));
	table->atText = NULL;
	return NULL;
}

static char *table_parse_string(Table *table, const char *text, const char **pText)
{
	char *str, *newStr;
	size_t n;

	str = malloc(16);
	n = 0;
	if (*text == '\"') {
		for (text++; *text != '\"'; text++) {
			if (*text == '\0') {
				fprintf(stderr, "error: missing closing double"
						"quotes");
				table->atText = text;
				goto err;
			}
			newStr = table_realloc(table, str, n + 1);
			if (newStr == NULL)
				goto err;
			str = newStr;
			str[n++] = *text;
		}
		text++;
	} else {
		for (; *text != '\0' && *text != '\t' &&
				*text != ',' && *text != ';'; text++) {
			newStr = table_realloc(table, str, n + 1);
			if (newStr == NULL)
				goto err;
			str = newStr;
			str[n++] = *text;
		}
	}

	if (n > 0) {
		size_t lead, trail;

		for (lead = 0; isblank(str[lead]); lead++);
		for (trail = 0; isblank(str[n - 1 - trail]); trail++);
		n -= lead + trail;
		memmove(str, str + lead, n);
	}
	newStr = table_realloc(table, str, n + 1);
	if (newStr == NULL)
		goto err;
	str = newStr;
	str[n] = '\0';
	*pText = text;
	return str;

err:
	free(str);
	return NULL;
}

static char **table_parse_row(Table *table, const char *text, size_t *pNumCols)
{
	char *str;
	char **row;
	char **newRow;
	size_t numCols;

	row = NULL;
	numCols = 0;
	while (1) {
		str = table_parse_string(table, text, &text);
		if (str == NULL)
			goto err;
		newRow = table_realloc(table, row, sizeof(*row) *
				(numCols + 1));
		if (newRow == NULL)
			goto err;
		row = newRow;
		row[numCols++] = str;
		if (*text == '\0')
			break;
		if (*text != ';' && *text != ',' && *text != '\t') {
			fprintf(stderr, "error: missing separator");
			table->atText = text;
			goto err;
		}
		text++;
	}
	*pNumCols = numCols;
	return row;

err:
	for (size_t x = 0; x < numCols; x++)
		free(row[x]);
	free(row);
	return NULL;
}

int table_parseline(Table *table, const char *text)
{
	char **row;
	size_t numCols;
	char ***newCells;
	size_t *newActiveRows;

	row = table_parse_row(table, text, &numCols);
	if (row == NULL)
		return -1;
	if (table->colNames == NULL) {
		table->colNames = row;
		table->numCols = numCols;
		table->activeCols = malloc(sizeof(*table->activeCols) *
				table->numCols);
		if (table->activeCols == NULL)
			goto err;
		table->newActiveCols = malloc(sizeof(*table->newActiveCols) *
				table->numCols);
		if (table->newActiveCols == NULL)
			goto err;
		return 0;
	}
	if (numCols > table->numCols) {
		fprintf(stderr, "error: too many cols (%zu vs %zu)",
			numCols, table->numCols);
		goto err;
	}
	if (numCols < table->numCols) {
		char **paddedRow;

		paddedRow = realloc(row, sizeof(*row) * table->numCols);
		if (paddedRow == NULL)
			goto err;
		row = paddedRow;
		/* setting all to NULL to never get free(trashpointer) */
		for (size_t i = numCols; i < table->numCols; i++)
			row[i] = NULL;
		for (size_t i = numCols; i < table->numCols; i++) {
			row[i] = malloc(1);
			if (row[i] == NULL)
				goto err;
			row[i][0] = '\0';
		}
	}
	newCells = table_realloc(table, table->cells, sizeof(*table->cells) *
			(table->numRows + 1));
	if (newCells == NULL)
		goto err;
	table->cells = newCells;
	table->cells[table->numRows++] = row;

	newActiveRows = realloc(table->activeRows,
			sizeof(*table->activeRows) * table->numRows);
	if (newActiveRows == NULL)
		goto err;
	table->activeRows = newActiveRows;

	newActiveRows = realloc(table->newActiveRows,
			sizeof(*table->newActiveRows) * table->numRows);
	if (newActiveRows == NULL)
		goto err;
	table->newActiveRows = newActiveRows;
	return 0;

err:
	table_uninit(table);
	return -1;
}

void table_uninit(Table *table)
{
	for (size_t x = 0; x < table->numCols; x++)
		free(table->colNames[x]);
	for (size_t y = 0; y < table->numRows; y++) {
		for (size_t x = 0; x < table->numCols; x++)
			free(table->cells[y][x]);
		free(table->cells[y]);
	}
	free(table->colNames);
	free(table->cells);
	free(table->activeRows);
	free(table->activeCols);
	free(table->newActiveRows);
	free(table->newActiveCols);
}

