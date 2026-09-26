#include "syntax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_file(const char *path, int legacy_intrinsic_dot)
{
	FILE *file = fopen(path, "rb");
	if (!file) return -1;
	char *input = NULL;
	int result = -1;
	if (fseek(file, 0, SEEK_END) != 0) goto done;
	long size = ftell(file);
	if (size < 0) goto done;
	size_t length = (size_t)size;
	if ((long)length != size || length == SIZE_MAX) goto done;
	if (fseek(file, 0, SEEK_SET) != 0) goto done;
	input = malloc(length + 1);
	if (!input) goto done;
	if (fread(input, 1, length, file) != length) goto done;
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, input, length);
	parser.allow_legacy_intrinsic_dot = legacy_intrinsic_dot;
	int status;
	do status = pg_parser_next(&parser, &definition); while (status == 1);
	if (status == 0) printf("parsed\t%s\t%zu\t\n", path, parser.entries);
	else printf("syntax_error\t%s\t%zu:%zu\t%s\n", path,
		parser.error_token.line, parser.error_token.column, parser.error);
	pg_graph_destroy(&arena);
	result = status == 0 ? 0 : 1;
done:
	free(input);
	fclose(file);
	return result;
}

int main(int argc, char **argv)
{
	int legacy_intrinsic_dot = argc > 1 && !strcmp(argv[1], "--legacy-intrinsic-dot");
	if (legacy_intrinsic_dot) { ++argv; --argc; }
	if (argc < 2) {
		fprintf(stderr, "usage: parse_files [--legacy-intrinsic-dot] source.p ...\n");
		return 2;
	}
	int failed = 0;
	for (int i = 1; i < argc; ++i) {
		int result = parse_file(argv[i], legacy_intrinsic_dot);
		if (result < 0) printf("io_error\t%s\t\tunable to read source\n", argv[i]);
		if (result != 0) failed = 1;
	}
	return failed;
}
