#include "tabular.h"

void usage(int argc, char **argv)
{
	(void) argc;

	fprintf(stderr, "usage: %s <file name or '-' for stdin> [options]\n",
			argv[0]);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-d\n");
	fprintf(stderr, "-c <column name>\n");
}

int cell_add(struct table *table, const char *cell, void *arg)
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

int main(int argc, char **argv)
{
	enum {
		OPT_INFO,
		OPT_ROW,
		OPT_COLUMN,
		OPT_NO_ROWS,
		OPT_NO_COLUMNS,
		OPT_SET_ROW,
		OPT_SET_COLUMN,
		OPT_SUM,
		OPT_PRINT,
	};
	static struct option longOptions[] = {
		{ "info",	0, 0, 'i' },
		{ "row", 	1, 0, 'r' },
		{ "column", 	1, 0, 'c' },
		{ "no-rows",	0, 0, 0 },
		{ "no-columns", 0, 0, 0 },
		{ "set-row", 1, 0, 0 },
		{ "set-column",	1, 0, 0 },
		{ "sum",	0, 0, 0 },
		{ "print",	0, 0, 'p' },
		{ 0, 0, 0, 0 }
	};
	FILE *fp;
	char *line = NULL;
	size_t capacity = 0;
	ssize_t count;
	size_t lineIndex;
	struct table table;
	char opt;
	int option;

	if (argc == 1) {
		usage(argc, argv);
		return 0;
	}

	if (argv[1][0] == '-' && argv[1][1] == '\0') {
		fp = stdin;
	} else {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			return 1;
		}
	}

	table_init(&table);
	lineIndex = 0;
	while ((count = getline(&line, &capacity, fp)) >= 0) {
		if (count == 0)
			continue;
		if (table_parseline(&table, line) < 0) {
			fprintf(stderr, "error at line no. %zu: %s\n",
					lineIndex + 1, line);
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
	while ((opt = getopt_long(argc, argv, "c:dpr:s", longOptions, &option))
			>= 0) {
		switch (opt) {
		case 0:
			switch (option) {
			case OPT_NO_ROWS:
				table.numActiveRows = 0;
				break;
			case OPT_NO_COLUMNS:
				table.numActiveColumns = 0;
				break;
			case OPT_SET_ROW:
				table.numActiveRows = 0;
				table_filterrows(&table, optarg);
				break;
			case OPT_SET_COLUMN:
				table.numActiveColumns = 0;
				table_filtercolumns(&table, optarg);
				break;
			}
			break;
		case 'c':
			table_filtercolumns(&table, optarg);
			break;
		case 'i':
			table_dumpinfo(&table);
			break;
		case 'p':
			table_printactivecells(&table);
			break;
		case 'r':
			table_filterrows(&table, optarg);
			break;
		case 's': {
			int64_t sums[2] = { 0, 0 };

			table_foreach(&table, cell_add, sums);
			sums[0] += sums[1] / 100;
			sums[1] %= 100;
			printf("%" PRId64 ",%" PRId64 "\n", sums[0], sums[1]);
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
