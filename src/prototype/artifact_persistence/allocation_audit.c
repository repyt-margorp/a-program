#include "program.h"

#include <stdio.h>

/* Link-time census of arena requests from outside graph.c. Calls within that
 * module are not interposed. This is cumulative allocation, not live memory. */
static size_t calls, aligned_bytes;

void *__real_pg_alloc(struct pg_graph *graph, size_t bytes);
void __real_pg_program_destroy(struct pg_program *program);

void *__wrap_pg_alloc(struct pg_graph *graph, size_t bytes)
{
	void *result = __real_pg_alloc(graph, bytes);
	if (result) {
		size_t alignment = _Alignof(max_align_t);
		size_t aligned = (bytes + alignment - 1) / alignment * alignment;
		++calls;
		aligned_bytes += aligned ? aligned : alignment;
	}
	return result;
}

void __wrap_pg_program_destroy(struct pg_program *program)
{
	fprintf(stderr, "wrapped_arena_calls\t%zu\nwrapped_aligned_bytes\t%zu\n",
		calls, aligned_bytes);
	__real_pg_program_destroy(program);
}
