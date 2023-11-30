#include "tabular.h"

void usage(int argc, char **argv)
{
	(void) argc;

	fprintf(stderr, "usage: %s [file name] [options]\n",
			argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "1. Status:\n");
	fprintf(stderr, "--info		Show table information (size and column names)\n");
	fprintf(stderr, "--view -v	View all selected cells in a terminal user interface\n");
	fprintf(stderr, "--print -p	Print all selected cells\n");
	fprintf(stderr, "--output -o	Output to a file or stdout\n");
	fprintf(stderr, "--input -i	Input from a file\n");

	fprintf(stderr, "\n2. Selecting:\n");
	fprintf(stderr, "Note: At the beginning, nothing is selected.\n");
	fprintf(stderr, "--col \"*\" --row \"*\"\n");
	fprintf(stderr, "--all -a	Select all rows and columns\n");
	fprintf(stderr, "--all-rows	Select all rows and columns\n");
	fprintf(stderr, "--all-cols	Select all rows and columns\n");
	fprintf(stderr, "--invert	Inverts the selection\n");
	fprintf(stderr, "--invert-row	Inverts the selected rows\n");
	fprintf(stderr, "--invert-cols	Inverts the selected columns\n");
	fprintf(stderr, "--none		Deselect everything\n");
	fprintf(stderr, "--no-rows	Deselect all rows\n");
	fprintf(stderr, "--no-cols	Deselect all columns\n\n");

	fprintf(stderr, "--row -r	Select a row\n");
	fprintf(stderr, "--column -c	Select a column\n");
	fprintf(stderr, "--set-row	Combination of --no-rows and --row\n");
	fprintf(stderr, "--set-col	Combination of --no-columns and --column\n");
	fprintf(stderr, "--undo		Undo a selection\n");
	fprintf(stderr, "--redo		Redo a selection\n");

	fprintf(stderr, "\n3. Modifying:\n");
	fprintf(stderr, "--append	Append a row\n");
	fprintf(stderr, "--append-col	Append a column\n");
}

int main(int argc, char **argv)
{
	static struct option longOptions[] = {
		[TABLE_OPERATION_INFO] = { "info", 0, 0, 0 },
		[TABLE_OPERATION_VIEW] = { "view", 0, 0, 'v' },
		[TABLE_OPERATION_PRINT] = { "print", 0, 0, 'p' },
		[TABLE_OPERATION_OUTPUT] = { "output", 2, 0, 'o' },
		[TABLE_OPERATION_INPUT] = { "input", 1, 0, 'i' },

		[TABLE_OPERATION_ALL] = { "all", 0, 0, 'a' },
		[TABLE_OPERATION_ALL_ROWS] = { "all-rows", 0, 0, 0 },
		[TABLE_OPERATION_ALL_COLS] = { "all-columns", 0, 0, 0 },
		[TABLE_OPERATION_INVERT] = { "invert", 0, 0, 0 },
		[TABLE_OPERATION_INVERT_ROWS] = { "invert-rows", 0, 0, 0 },
		[TABLE_OPERATION_INVERT_COLS] = { "invert-cols", 0, 0, 0 },
		[TABLE_OPERATION_NONE] = { "none", 0, 0, 0 },
		[TABLE_OPERATION_NO_ROWS] = { "no-rows", 0, 0, 0 },
		[TABLE_OPERATION_NO_COLS] = { "no-cols", 0, 0, 0 },

		[TABLE_OPERATION_ROW] = { "row", 1, 0, 'r' },
		[TABLE_OPERATION_COL] = { "col", 1, 0, 'c' },
		[TABLE_OPERATION_SET_ROW] = { "set-row", 1, 0, 0 },
		[TABLE_OPERATION_SET_COL] = { "set-col", 1, 0, 0 },

		[TABLE_OPERATION_APPEND] = { "append", 2, 0, 'd' },
		[TABLE_OPERATION_APPEND_COL] = { "append-col", 2, 0, 'n' },

		[TABLE_OPERATION_UNDO] = { "undo", 0, 0, 0 },
		[TABLE_OPERATION_REDO] = { "redo", 0, 0, 0 },
		{ 0, 0, 0, 0 }
	};
	Table table;
	char opt;
	int optionIndex;

	setlocale(LC_ALL, "");

	if (argc == 1) {
		usage(argc, argv);
		return 0;
	}

	table_init(&table);

	if (argv[1][0] != '-') {
		/* explicit --input */
		table_dooperation(&table, TABLE_OPERATION_INPUT, argv[1]);
		optind = 2;
	}
	while ((opt = getopt_long(argc, argv, "ac:d::n::o::i:pr:v",
			longOptions, &optionIndex)) >= 0) {
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
}
