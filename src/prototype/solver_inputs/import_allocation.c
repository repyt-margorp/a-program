#include "artifact/file.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

/* Read-only census of external pg_alloc requests into the returned Program's
 * persistent arena. Initialization, graph.c internals and discarded scratch
 * are excluded. Requested bytes are not RSS, block capacity or peak memory. */
static struct pg_graph *read_graph;
static size_t requests, requested_bytes, initial_proofs;

void *__real_pg_alloc(struct pg_graph *, size_t);
struct pg_program *__real_pg_program_allocate_empty(enum pg_definition_policy);

struct pg_program *__wrap_pg_program_allocate_empty(enum pg_definition_policy policy)
{
	struct pg_program *program = __real_pg_program_allocate_empty(policy);
	read_graph = program ? &program->graph : NULL;
	initial_proofs = program ? program->typing.proofs.count : 0;
	return program;
}

void *__wrap_pg_alloc(struct pg_graph *graph, size_t bytes)
{
	void *result = __real_pg_alloc(graph, bytes);
	if (result && graph == read_graph) {
		assert(requests < SIZE_MAX && bytes <= SIZE_MAX - requested_bytes);
		++requests;
		requested_bytes += bytes;
	}
	return result;
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	FILE *file = fopen(argv[1], "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long bytes = ftell(file);
	assert(bytes >= 0 && !fseek(file, 0, SEEK_SET));
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(program && !fclose(file));
	assert(!program->synthesis.steps && program->typing.proofs.count == initial_proofs);
	printf("image_bytes\troots\tsteps\tterms\toccurrences\tjobs\tpersistent_requests\tpersistent_requested_bytes\n");
	printf("%ld\t%zu\t%" PRIu64 "\t%zu\t%zu\t%zu\t%zu\t%zu\n",
		bytes, count, program->synthesis.steps, program->graph.terms.count,
		program->typing.occurrences.count, program->synthesis.jobs.count,
		requests, requested_bytes);
	pg_program_destroy(program);
	return 0;
}
