#include "program.h"

#include <stdio.h>
#include <stdlib.h>

/* Calls through linked allocator symbols only; libc internals are not counted.
 * These are cumulative requests, including scratch arenas, not live RAM. */
static size_t calls, bytes, zeroed_calls, zeroed_bytes;

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_pg_program_destroy(struct pg_program *program);

void *__wrap_malloc(size_t size)
{
	void *result = __real_malloc(size);
	if (result) { ++calls; bytes += size; }
	return result;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *result = __real_calloc(count, size);
	if (result) { ++zeroed_calls; zeroed_bytes += count * size; }
	return result;
}

void __wrap_pg_program_destroy(struct pg_program *program)
{
	fprintf(stderr, "wrapped_malloc_calls\t%zu\nwrapped_malloc_bytes\t%zu\n", calls, bytes);
	fprintf(stderr, "wrapped_calloc_calls\t%zu\nwrapped_calloc_bytes\t%zu\n", zeroed_calls, zeroed_bytes);
	__real_pg_program_destroy(program);
}
