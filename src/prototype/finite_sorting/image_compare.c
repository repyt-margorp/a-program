#include "source_io.h"

static size_t image_limit;

static struct pg_program *bounded_read(FILE *file, size_t limit, size_t *count,
	struct pg_synthesis_job *const **roots)
{
	return pg_sources_read(file, image_limit ? image_limit : limit, count, roots);
}

/* Reuse the accepted typed comparison and its chunk-1/chunk-64 checks.
 * Only the caller-selected resource limit differs; admission is unchanged. */
#define pg_sources_read bounded_read
#define main program_test_main
#include "../../../tests/program.c"
#undef main
#undef pg_sources_read

int main(int argc, char **argv)
{
	if (argc != 9 || strcmp(argv[1], "--image-limit") ||
		strcmp(argv[3], "--steps") || strcmp(argv[5], "--equal-image")) return 2;
	char *end;
	errno = 0;
	uintmax_t limit = strtoumax(argv[2], &end, 10);
	if (errno || end == argv[2] || *end || argv[2][0] == '-' || !limit || limit > SIZE_MAX) return 2;
	image_limit = (size_t)limit;
	return program_test_main(argc - 2, argv + 2);
}
