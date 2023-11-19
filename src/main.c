#include "tabular.h"

void usage(int argc, char **argv)
{
	(void) argc;

	fprintf(stderr, "usage: %s <file name or '-' for stdin> [options]\n",
			argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "--info -i		Show table information (size and column names)\n");
	fprintf(stderr, "--column \"*\" --row \"*\"\n");
	fprintf(stderr, "--all -a		Select all rows and columns\n");
	fprintf(stderr, "--row -r		Select a row\n");
	fprintf(stderr, "--column -c		Select a column\n");
	fprintf(stderr, "--no-rows		Deselect all rows\n");
	fprintf(stderr, "--no-columns		Deselect all columns\n");
	fprintf(stderr, "--set-row		Select a single row\n");
	fprintf(stderr, "--set-column		Select a single column\n");
	fprintf(stderr, "--sum			Interpret all cells as numbers and take the sum\n");
	fprintf(stderr, "--view -v		View all selected cells in a terminal user interface\n");
	fprintf(stderr, "--print -p		Print all selected cells\n");
}

int main(int argc, char **argv)
{
	static struct option longOptions[] = {
		[TABLE_OPERATION_ALL] = { "all", 0, 0, 'a' },
		[TABLE_OPERATION_INFO] = { "info", 0, 0, 'i' },
		[TABLE_OPERATION_ROW] = { "row", 1, 0, 'r' },
		[TABLE_OPERATION_COLUMN] = { "column", 1, 0, 'c' },
		[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0, 0, 0 },
		[TABLE_OPERATION_NO_COLUMNS] = { "no-columns", 0, 0, 0 },
		[TABLE_OPERATION_SET_ROW] = { "set-row", 1, 0, 0 },
		[TABLE_OPERATION_SET_COLUMN] = { "set-column", 1, 0, 0 },
		[TABLE_OPERATION_SUM] = { "sum", 0, 0, 0 },
		[TABLE_OPERATION_VIEW] = { "view", 0, 0, 'v' },
		[TABLE_OPERATION_PRINT] = { "print", 0, 0, 'p' },
		{ 0, 0, 0, 0 }
	};
	FILE *fp;
	char *line = NULL;
	size_t capacity = 0;
	ssize_t count;
	size_t lineIndex;
	Table table;
	char opt;
	int optionIndex;

	setlocale(LC_ALL, "");

	if (argc == 1) {
		usage(argc, argv);
		return 0;
	}

	if (argv[1][0] == '-' && argv[1][1] == '\0') {
		fp = stdin;
	} else {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			fprintf(stderr, "error opening '%s': %s\n",
					argv[1], strerror(errno));
			return 1;
		}
	}

	table_init(&table);
	lineIndex = 0;
	while ((count = getline(&line, &capacity, fp)) >= 0) {
		if (count <= 1)
			continue;
		line[count - 1] = '\0';
		if (table_parseline(&table, line) < 0) {
			fprintf(stderr, " at line no. %zu\n%s\n",
					lineIndex + 1, line);
			if (table.atText != NULL) {
				for (char *s = line; s != table.atText; s++)
					fprintf(stderr, "~");
				fprintf(stderr, "^\n");
			}
			goto err;
		}
		lineIndex++;
	}
	if (fp != stdin)
		fclose(fp);
	free(line);
	table.activeRows = malloc(sizeof(*table.activeRows) * table.numRows);
	table.activeColumns = malloc(sizeof(*table.activeColumns) *
			table.numColumns);
	if (table.activeRows == NULL || table.activeColumns == NULL)
		goto err;

	optind = 2;
	while ((opt = getopt_long(argc, argv, "ac:ipr:s", longOptions, &optionIndex))
			>= 0) {
		switch (opt) {
		case 0:
			table_dooperation(&table, optionIndex, optarg);
			break;
		default:
			for (size_t i = 0; i < ARRLEN(longOptions); i++)
				if (longOptions[i].val == opt) {
					table_dooperation(&table, i, optarg);
					break;
				}
		}
	}

	table_uninit(&table);
	return 0;

err:
	table_uninit(&table);
	return 1;
}
