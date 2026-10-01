#include "source_io.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static int measuring;
static size_t calls, aligned_bytes;

void *__real_pg_alloc(struct pg_graph *, size_t);

/* External arena requests during writing, not total or peak live memory. */
void *__wrap_pg_alloc(struct pg_graph *graph, size_t bytes)
{
	void *result = __real_pg_alloc(graph, bytes);
	if (result && measuring) {
		size_t alignment = _Alignof(max_align_t);
		++calls;
		aligned_bytes += bytes ? (bytes + alignment - 1) / alignment * alignment : alignment;
	}
	return result;
}

int main(int argc, char **argv)
{
	if (argc != 4 && argc != 5) return 2;
	char *end;
	errno = 0;
	uintmax_t fuel = strtoumax(argv[2], &end, 10);
	if (errno || !*argv[2] || *argv[2] == '-' || *end || fuel > UINT64_MAX) return 2;
	FILE *file = fopen(argv[1], "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long length = ftell(file);
	assert(length >= 0 && (uintmax_t)length < SIZE_MAX && !fseek(file, 0, SEEK_SET));
	char *source = malloc((size_t)length + 1);
	assert(source && fread(source, 1, (size_t)length, file) == (size_t)length && !ferror(file));
	source[length] = 0;
	assert(!fclose(file));
	struct pg_program *program = pg_program_create(source, (size_t)length, PG_DEFINITION_IMPLICIT_THUNK);
	free(source);
	assert(program && program->root);
	struct pg_synthesis_job *root = program->root;
	if (argc == 5) root = pg_program_select_name(program, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = argv[4], .length = strlen(argv[4])});
	assert(root);
	pg_synthesis_advance(&program->synthesis, (uint64_t)fuel);
	uint64_t steps = program->synthesis.steps;
	size_t proofs = program->typing.proofs.count, jobs = program->synthesis.jobs.count;
	size_t terms = program->graph.terms.count, occurrences = program->typing.occurrences.count;
	file = fopen(argv[3], "wb");
	assert(file);
	const struct pg_evidence *checked = argc == 5 ? pg_synthesis_result(root) : NULL;
	struct pg_synthesis_input input = checked ? (struct pg_synthesis_input){.checked = checked}
		: (struct pg_synthesis_input){.pending = pg_synthesis_pending(root)};
	measuring = 1;
	int status = pg_sources_write(file, &program->synthesis, 1, &input);
	measuring = 0;
	long bytes = ftell(file);
	assert(bytes >= 0 && !fclose(file));
	assert(steps == program->synthesis.steps && proofs == program->typing.proofs.count);
	assert(jobs == program->synthesis.jobs.count && terms == program->graph.terms.count);
	assert(occurrences == program->typing.occurrences.count);
	printf("%" PRIuMAX "\t%" PRIu64 "\t%d\t%d\t%ld\t%zu\t%zu\n", fuel, steps,
		pg_synthesis_status(root), status, bytes, calls, aligned_bytes);
	pg_program_destroy(program);
	return 0;
}
