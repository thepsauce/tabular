#include "tabular.h"

static void table_printinfo(Table *table);
static int table_printbeautiful(Table *table);
static void table_printactivecells(Table *table);
static int table_writeout(Table *table, const char *path);
static int table_readin(Table *table, const char *path);

static void table_allrows(Table *table);
static void table_allcols(Table *table);
static void table_invertrows(Table *table);
static void table_invertcols(Table *table);

static void table_filterrows(Table *table, const Utf8 *filter);
static void table_filtercols(Table *table, const Utf8 *filter);
static void table_selectrows(Table *table, const Utf8 *filter);
static void table_selectcols(Table *table, const Utf8 *filter);

static int table_appendcol(Table *table, const char *name);

static void table_redo(Table *table);
static void table_undo(Table *table);

void table_validatediff(Table *table, struct table_diff *diff)
{
	for (size_t i = 0; i < diff->numChangedCols; i++)
		diff->colDiff[i].col =
			table->activeCols[diff->colDiff[i].index];
	for (size_t i = 0; i < diff->numChangedRows; i++)
		diff->rowDiff[i].row =
			table->activeRows[diff->rowDiff[i].index];
}

struct table_diff_part {
	size_t index;
	size_t value;
};

static struct table_diff_part *
getdiff(size_t *a, size_t na, size_t *b, size_t nb,
		struct table_diff_part **pc, size_t *pnc)
{
	struct table_diff_part *c, *sc;
	size_t nc;

	c = malloc(sizeof(*c) * na);
	if (c == NULL)
		return NULL;
	nc = 0;
	for (size_t i = 0, j; i < na; i++) {
		for (j = 0; j < nb; j++)
			if (a[i] == b[i])
				break;
		if (j == nb) {
			c[nc].index = i;
			c[nc].value = a[i];
			nc++;
		}
	}
	sc = realloc(c, sizeof(*c) * nc);
	if (sc == NULL)
		return NULL;
	*pc = sc;
	*pnc = nc;
	return sc;
}

void table_applydiff(Table *table, const struct table_diff *diff)
{
	bool found;

	for (size_t i = 0, j; i < diff->numChangedRows; i++) {
		found = false;
		for (j = 0; j < table->numActiveRows; j++)
			if (diff->rowDiff[i].row == table->activeRows[j]) {
				table->numActiveRows--;
				memmove(&table->activeRows[j],
					&table->activeRows[j + 1],
					sizeof(*table->activeRows) *
						(table->numActiveRows - j));
				found = true;
				break;
			}
		if (!found) {
			const size_t index = diff->rowDiff[i].index;
			memmove(&table->activeRows[index + 1],
				&table->activeRows[index],
				sizeof(*table->activeRows) *
					(table->numActiveRows - index));
			table->activeRows[index] = diff->rowDiff[i].row;
			table->numActiveRows++;
		}
	}

	for (size_t i = 0, j; i < diff->numChangedCols; i++) {
		found = false;
		for (j = 0; j < table->numActiveCols; j++)
			if (diff->colDiff[i].col == table->activeCols[j]) {
				table->numActiveCols--;
				memmove(&table->activeCols[j],
					&table->activeCols[j + 1],
					sizeof(*table->activeCols) *
						(table->numActiveCols - j));
				found = true;
				break;
			}
		if (!found) {
			const size_t index = diff->colDiff[i].index;
			memmove(&table->activeCols[index + 1],
				&table->activeCols[index],
				sizeof(*table->activeCols) *
					(table->numActiveCols - index));
			table->activeCols[index] = diff->colDiff[i].col;
			table->numActiveCols++;
		}
	}
}

int table_appendhistory(Table *table, const struct table_diff *diff)
{
	struct table_diff *newHistory;

	for (size_t i = table->indexHistory; i < table->numHistory; i++) {
		free(table->history[i].rowDiff);
		free(table->history[i].colDiff);
	}
	table->numHistory = table->indexHistory;
	newHistory = realloc(table->history, sizeof(*table->history) *
			(table->numHistory + 1));
	if (newHistory == NULL) {
		free(diff->rowDiff);
		free(diff->colDiff);
		return -1;
	}
	table->history = newHistory;
	table->history[table->numHistory++] = *diff;
	table->indexHistory++;
	return 0;
}

int table_generatediff(Table *table)
{
	struct table_diff diff;

	if (getdiff(table->newActiveRows, table->newNumActiveRows,
			table->activeRows, table->numActiveRows,
			(struct table_diff_part**) &diff.rowDiff,
			&diff.numChangedRows) == NULL)
		return -1;
	if (getdiff(table->newActiveCols, table->newNumActiveCols,
			table->activeCols, table->numActiveCols,
			(struct table_diff_part**) &diff.colDiff,
			&diff.numChangedCols) == NULL) {
		free(diff.rowDiff);
		return -1;
	}

	memcpy(table->activeRows, table->newActiveRows,
		sizeof(*table->newActiveRows) * table->newNumActiveRows);
	table->numActiveRows = table->newNumActiveRows;
	memcpy(table->activeCols, table->newActiveCols,
		sizeof(*table->newActiveCols) * table->newNumActiveCols);
	table->numActiveCols = table->newNumActiveCols;

	if (table_appendhistory(table, &diff) < 0)
		return -1;
	return 0;
}

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
		table_allrows(table);
		table_allcols(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_ALL_ROWS:
		table_allrows(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_ALL_COLS:
		table_allcols(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_INVERT:
		table_invertrows(table);
		table_invertcols(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_INVERT_ROWS:
		table_invertrows(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_INVERT_COLS:
		table_invertcols(table);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_NONE:
		table->newNumActiveRows = 0;
		table->newNumActiveCols = 0;
		table_generatediff(table);
		break;
	case TABLE_OPERATION_NO_ROWS:
		table->newNumActiveRows = 0;
		table->newNumActiveCols = table->numActiveCols;
		memcpy(table->newActiveCols, table->activeCols,
				sizeof(*table->activeCols) *
					table->numActiveCols);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_NO_COLS:
		table->newNumActiveRows = table->numActiveRows;
		memcpy(table->newActiveRows, table->activeRows,
				sizeof(*table->activeRows) *
					table->numActiveRows);
		table->numActiveCols = 0;
		table_generatediff(table);
		break;

	case TABLE_OPERATION_ROW:
		table_filterrows(table, arg);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_COL:
		table_filtercols(table, arg);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_SET_ROW:
		table_selectrows(table, arg);
		table_generatediff(table);
		break;
	case TABLE_OPERATION_SET_COL:
		table_selectcols(table, arg);
		table_generatediff(table);
		break;

	case TABLE_OPERATION_APPEND:
		table_parseline(table, arg == NULL ? "" : arg);
		break;
	case TABLE_OPERATION_APPEND_COL:
		table_appendcol(table, arg == NULL ? "" : arg);
		break;

	case TABLE_OPERATION_UNDO:
		table_undo(table);
		break;
	case TABLE_OPERATION_REDO:
		table_redo(table);
		break;
	}
}

static void table_printactivecells(Table *table)
{
	if (table->numActiveCols == 1) {
		for (size_t i = 0; i < table->numActiveRows; i++) {
			const size_t row = table->activeRows[i];
			const size_t col = table->activeCols[0];
			printf("%s\n", table->cells[row][col]);
		}
		return;
	}
	for (size_t i = 0; i < table->numActiveCols; i++) {
		const size_t col = table->activeCols[i];
		printf("%s\n", table->colNames[col]);
		for (size_t j = 0; j < table->numActiveRows; j++) {
			const size_t row = table->activeRows[j];
			printf("\t%s\n", table->cells[row][col]);
		}
	}
}

static void table_printinfo(Table *table)
{
	printf("%zu %zu\n", table->numActiveCols, table->numActiveRows);
	for (size_t i = 0; i < table->numActiveCols; i++) {
		const size_t col = table->activeCols[i];
		printf("%s\n", table->colNames[col]);
	}
}

static int table_printbeautiful(Table *table)
{
	TableView view;

	if (table_view_init(&view, table) != 0)
		return -1;
	if (table_view_show(&view) != 0)
		return -1;
	table_view_uninit(&view);
	return 0;
}

static int table_writeout(Table *table, const char *path)
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

	for (size_t i = 0; i < table->numActiveCols; i++) {
		const size_t col = table->activeCols[i];
		if (i > 0)
			fputc(';', fp);
		fprintf(fp, "\"%s\"", table->colNames[col]);
	}
	fputc('\n', fp);
	for (size_t i = 0; i < table->numActiveRows; i++) {
		const size_t row = table->activeRows[i];
		for (size_t j = 0; j < table->numActiveCols; j++) {
			const size_t col = table->activeCols[j];
			if (j > 0)
				fputc(';', fp);
			fprintf(fp, "\"%s\"", table->cells[row][col]);
		}
		fputc('\n', fp);
	}
	if (fp != stdout)
		fclose(fp);
	return 0;
}

static int table_readin(Table *table, const char *path)
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

static void table_allrows(Table *table)
{
	for (size_t i = 0; i < table->numRows; i++)
		table->newActiveRows[i] = i;
	table->newNumActiveRows = table->numRows;
}

static void table_allcols(Table *table)
{
	for (size_t i = 0; i < table->numCols; i++)
		table->newActiveCols[i] = i;
	table->newNumActiveCols = table->numCols;
}

static void table_invertrows(Table *table)
{
	for (size_t i = 0, j = 0; i < table->numRows; i++) {
		size_t k;

		for (k = 0; k < table->numActiveRows; k++)
			if (table->activeRows[k] == i)
				break;
		if (k != table->numActiveRows)
			continue;
		table->newActiveRows[j++] = i;
	}
	table->newNumActiveRows = table->numRows - table->numActiveRows;
}

static void table_invertcols(Table *table)
{
	for (size_t i = 0, j = 0; i < table->numCols; i++) {
		size_t k;

		for (k = 0; k < table->numActiveCols; k++)
			if (table->activeCols[k] == i)
				break;
		if (k != table->numActiveCols)
			continue;
		table->newActiveCols[j++] = i;
	}
	table->newNumActiveCols = table->numCols - table->numActiveCols;
}

static void table_filterrows(Table *table, const Utf8 *filter)
{
	if (table->numActiveRows > 0) {
		table->newNumActiveRows = 0;
		for (size_t i = 0; i < table->numActiveRows; i++) {
			const size_t row = table->activeRows[i];
			for (size_t j = 0; j < table->numActiveCols; j++) {
				const size_t col = table->activeCols[j];
				if (!utf8_match(filter,
						table->cells[row][col]))
					continue;
				table->newActiveRows
					[table->newNumActiveRows++] = row;
				break;
			}
		}
	} else {
		table_selectrows(table, filter);
	}
}

static void table_selectrows(Table *table, const Utf8 *filter)
{
	table->newNumActiveRows = 0;
	for (size_t row = 0; row < table->numRows; row++) {
		for (size_t j = 0; j < table->numActiveCols; j++) {
			const size_t col = table->activeCols[j];
			if (!utf8_match(filter,
					table->cells[row][col]))
				continue;
			table->newActiveRows[table->newNumActiveRows++] = row;
			break;
		}
	}
}

static void table_filtercols(Table *table, const Utf8 *filter)
{
	if (table->numActiveCols > 0) {
		table->newNumActiveCols = 0;
		for (size_t i = 0; i < table->numActiveCols; i++) {
			const size_t col = table->activeCols[i];
			if (utf8_match(filter, table->colNames[col]))
				table->newActiveCols
					[table->newNumActiveCols++] = col;
		}
	} else {
		table_selectcols(table, filter);
	}
}

static void table_selectcols(Table *table, const Utf8 *filter)
{
	table->newNumActiveCols = 0;
	for (size_t col = 0; col < table->numCols; col++)
		if (utf8_match(filter, table->colNames[col]))
			table->newActiveCols[table->newNumActiveCols++] = col;
}

static int table_appendcol(Table *table, const Utf8 *name)
{
	Utf8 **newColumnNames;
	size_t *newActiveCols;

	newColumnNames = realloc(table->colNames,
			sizeof(*table->colNames) * (table->numCols + 1));
	if (newColumnNames == NULL)
		return -1;
	table->colNames = newColumnNames;
	table->colNames[table->numCols] = strdup(name);
	if (table->colNames[table->numCols] == NULL)
		return -1;
	for (size_t i = 0; i < table->numRows; i++) {
		char **newCells;

		newCells = realloc(table->cells[i], sizeof(*table->cells[i]) *
				(table->numCols + 1));
		if (newCells == NULL)
			return -1;
		table->cells[i] = newCells;
		table->cells[i][table->numCols] = strdup("");
		if (table->cells[i][table->numCols] == NULL) {
			while (i > 0) {
				i--;
				free(table->cells[i][table->numCols]);
			}
			return -1;
		}
	}
	newActiveCols = realloc(table->activeCols,
			sizeof(*table->activeCols) *
				(table->numCols + 1));
	if (newActiveCols == NULL)
		return -1;
	table->activeCols = newActiveCols;

	newActiveCols = realloc(table->newActiveCols,
			sizeof(*table->newActiveCols) *
				(table->numCols + 1));
	if (newActiveCols == NULL)
		return -1;
	table->newActiveCols = newActiveCols;
	table->numCols++;
	return 0;
}

static void table_undo(Table *table)
{
	struct table_diff *diff;

	if (table->indexHistory == 0)
		return;
	diff = &table->history[--table->indexHistory];
	table_applydiff(table, diff);
}

static void table_redo(Table *table)
{
	struct table_diff *diff;

	if (table->indexHistory == table->numHistory)
		return;
	diff = &table->history[table->indexHistory++];
	table_applydiff(table, diff);
}
