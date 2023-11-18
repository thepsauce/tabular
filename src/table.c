# include "tabular.h"

bool string_match(const char *pattern, const char *text)
{
	while (*pattern != '\0' || *text != '\0') {
		if (*pattern != *text) {
			if (*pattern != '*')
				return false;
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
		} else {
			pattern++;
			text++;
		}
	}
	return true;
}

int table_init(struct table *table)
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

int table_parseline(struct table *table, const char *line)
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

static inline void table_activaterow(struct table *table, size_t row)
{
	for (size_t i = 0; i < table->numActiveRows; i++)
		if (table->activeRows[i] == row)
			return;
	table->activeRows[table->numActiveRows++] = row;
}

void table_filterrows(struct table *table, const char *filter)
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

static inline void table_activatecolumn(struct table *table, size_t column)
{
	for (size_t i = 0; i < table->numActiveColumns; i++)
		if (table->activeColumns[i] == column)
			return;
	table->activeColumns[table->numActiveColumns++] = column;
}

void table_filtercolumns(struct table *table, const char *filter)
{
	for (size_t i = 0; i < table->numColumns; i++)
		if (string_match(filter, table->columnNames[i]))
			table_activatecolumn(table, i);
}

int table_foreach(struct table *table,
		int (*proc)(struct table *table, const char *cell, void *arg),
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

void table_uninit(struct table *table)
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

void table_printactivecells(struct table *table)
{
	for (size_t i = 0; i < table->numActiveRows; i++) {
		const size_t row = table->activeRows[i];
		for (size_t j = 0; j < table->numActiveColumns; j++) {
			const size_t column = table->activeColumns[j];
			printf("%s\n", table->cells[row][column]);
		}
	}
}

void table_dumpinfo(struct table *table)
{
	printf("%zu %zu\n", table->numColumns, table->numRows);
	for (size_t x = 0; x < table->numColumns; x++)
		printf("%s\n", table->columnNames[x]);
}

