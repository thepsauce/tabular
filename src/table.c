#include "tabular.h"

int table_init(Table *table)
{
	memset(table, 0, sizeof(*table));
	return 0;
}

static char *parse_string(const char *text, const char **pText)
{
	char *str, *newStr;
	size_t n;

	if (*text != '\"')
		return NULL;
	str = malloc(16);
	n = 0;
	for (text++; *text != '\"'; text++) {
		if (*text == '\0')
			goto err;
		newStr = realloc(str, n + 1);
		if (newStr == NULL)
			goto err;
		str = newStr;
		str[n++] = *text;
	}
	newStr = realloc(str, n + 1);
	if (newStr == NULL)
		goto err;
	str = newStr;
	str[n] = '\0';
	*pText = text + 1;
	return str;

err:
	free(str);
	return NULL;
}

static char **parse_row(const char *text, size_t *pNumColumns)
{
	char *str;
	char **row;
	char **newRow;
	size_t numColumns;

	row = NULL;
	numColumns = 0;
	while (1) {
		str = parse_string(text, &text);
		if (str == NULL)
			goto err;
		newRow = realloc(row, sizeof(*row) * (numColumns + 1));
		if (newRow == NULL)
			goto err;
		row = newRow;
		row[numColumns++] = str;
		if (*text == '\n')
			break;
		if (*text != ';')
			goto err;
		text++;
	}
	*pNumColumns = numColumns;
	return row;

err:
	for (size_t x = 0; x < numColumns; x++)
		free(row[x]);
	free(row);
	return NULL;
}

int table_parseline(Table *table, const char *line)
{
	char **row;
	size_t numColumns;
	char ***newCells;

	row = parse_row(line, &numColumns);
	if (row == NULL)
		return -1;
	if (table->columnNames == NULL) {
		table->columnNames = row;
		table->numColumns = numColumns;
		return 0;
	}
	if (numColumns != table->numColumns)
		goto err;
	newCells = realloc(table->cells, sizeof(*table->cells) *
			(table->numRows + 1));
	if (newCells == NULL)
		goto err;
	table->cells = newCells;
	table->cells[table->numRows++] = row;
	return 0;

err:
	for (size_t x = 0; x < numColumns; x++)
		free(row[x]);
	free(row);
	return -1;
}

void table_uninit(Table *table)
{
	for (size_t x = 0; x < table->numColumns; x++)
		free(table->columnNames[x]);
	for (size_t y = 0; y < table->numRows; y++) {
		for (size_t x = 0; x < table->numColumns; x++)
			free(table->cells[y][x]);
		free(table->cells[y]);
	}
	free(table->columnNames);
	free(table->cells);
	free(table->activeRows);
	free(table->activeColumns);
}

