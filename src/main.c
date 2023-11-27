#include "tabular.h"

void usage(int argc, char **argv)
{
	(void) argc;

	fprintf(stderr, "usage: %s <file name or '-' for stdin> [options]\n",
			argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "1. Status:\n");
	fprintf(stderr, "--info -i		Show table information (size and column names)\n");
	fprintf(stderr, "--view -v		View all selected cells in a terminal user interface\n");
	fprintf(stderr, "--print -p		Print all selected cells\n");
	fprintf(stderr, "--output -o		Output to a file or stdout\n");

	fprintf(stderr, "\n2. Selecting:\n");
	fprintf(stderr, "Note: At the beginning, nothing is selected.\n");
	fprintf(stderr, "--column \"*\" --row \"*\"\n");
	fprintf(stderr, "--all			Select all rows and columns\n");
	fprintf(stderr, "--invert		Inverts the selection\n");
	fprintf(stderr, "--row -r		Select a row\n");
	fprintf(stderr, "--column -c		Select a column\n");
	fprintf(stderr, "--no-rows		Deselect all rows\n");
	fprintf(stderr, "--no-columns		Deselect all columns\n");
	fprintf(stderr, "--none			Deselect everything\n");
	fprintf(stderr, "--set-row		Combination of --no-rows and --row\n");
	fprintf(stderr, "--set-column		Combination of --no-columns and --column\n");

	fprintf(stderr, "\n3. Modifying:\n");
	fprintf(stderr, "--append -a		Append a row\n");
	fprintf(stderr, "--delete -d		Delete all selected rows and columns\n");
	fprintf(stderr, "--undo			Undo the last operation\n");
	fprintf(stderr, "--redo			Redo the last operation\n");
}

int main(int argc, char **argv)
{
	static struct option longOptions[] = {
		[TABLE_OPERATION_INFO] = { "info", 0, 0, 'i' },
		[TABLE_OPERATION_VIEW] = { "view", 0, 0, 'v' },
		[TABLE_OPERATION_PRINT] = { "print", 0, 0, 'p' },
		[TABLE_OPERATION_OUTPUT] = { "output", 2, 0, 'o' },

		[TABLE_OPERATION_ALL] = { "all", 0, 0, 0 },
		[TABLE_OPERATION_INVERT] = { "invert", 0, 0, 0 },
		[TABLE_OPERATION_ROW] = { "row", 1, 0, 'r' },
		[TABLE_OPERATION_COLUMN] = { "column", 1, 0, 'c' },
		[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0, 0, 0 },
		[TABLE_OPERATION_NO_COLUMNS] = { "no-columns", 0, 0, 0 },
		[TABLE_OPERATION_NONE] = { "none", 0, 0, 0 },
		[TABLE_OPERATION_SET_ROW] = { "set-row", 1, 0, 0 },
		[TABLE_OPERATION_SET_COLUMN] = { "set-column", 1, 0, 0 },

		[TABLE_OPERATION_APPEND] = { "append", 1, 0, 'a' },
		[TABLE_OPERATION_UNDO] = { "undo", 0, 0, 0 },
		[TABLE_OPERATION_REDO] = { "redo", 0, 0, 0 },
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
