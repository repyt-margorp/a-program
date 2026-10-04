/* Compile against the exact checkpoint fixture under review. Reuse its jobs,
 * charged advance wrapper and byte comparison instead of a shadow scheduler. */
#define main normalization_checkpoint_main
#include "normalization_checkpoint_test.c"
#undef main

static struct pg_graph *denied_graph;
static size_t denied_allocations;

void *__real_pg_alloc(struct pg_graph *, size_t);
void *__wrap_pg_alloc(struct pg_graph *graph, size_t bytes)
{
	if (graph == denied_graph) {
		++denied_allocations;
		return NULL;
	}
	return __real_pg_alloc(graph, bytes);
}

int main(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *jobs[4];
	schedule_jobs(&p->synthesis, jobs);
	for (size_t i = 1; i < 4; ++i)
		pg_synthesis_subscribe(&p->synthesis, jobs[i], jobs[0], i != 2);
	FILE *saved = tmpfile();
	assert(saved && !pg_artifact_schedule_write(saved, &p->synthesis, 4, jobs));
	pg_program_destroy(p);
	p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	schedule_jobs(&p->synthesis, jobs);
	rewind(saved);
	const struct pg_artifact_schedule *state = pg_artifact_schedule_read(saved, &p->graph, 4);
	assert(state && !p->synthesis.free_waiters);
	FILE *before = tmpfile(), *after = tmpfile();
	assert(before && after && !pg_artifact_schedule_write(before, &p->synthesis, 4, jobs));
	unsigned char original[4][sizeof(*jobs[0])];
	for (size_t i = 0; i < 4; ++i) memcpy(original[i], jobs[i], sizeof(original[i]));
	struct pg_synthesis_job *ready = p->synthesis.ready, *tail = p->synthesis.ready_tail;
	size_t proofs = p->typing.proofs.count, requests = p->synthesis.jobs.count;
	const struct pg_block *blocks = p->graph.blocks;
	denied_graph = &p->graph;
	assert(pg_artifact_schedule_attach(&p->synthesis, state, 4, jobs));
	denied_graph = NULL;
	assert(denied_allocations && !p->synthesis.free_waiters);
	assert(p->graph.blocks == blocks && !p->synthesis.steps);
	assert(p->synthesis.ready == ready && p->synthesis.ready_tail == tail);
	assert(p->typing.proofs.count == proofs && p->synthesis.jobs.count == requests);
	for (size_t i = 0; i < 4; ++i) assert(!memcmp(original[i], jobs[i], sizeof(original[i])));
	assert(!pg_artifact_schedule_write(after, &p->synthesis, 4, jobs));
	equal_files(before, after);
	assert(!fclose(before) && !fclose(after));
	assert(!pg_artifact_schedule_attach(&p->synthesis, state, 4, jobs));
	denied_allocations = 0;
	denied_graph = &p->graph;
	for (size_t i = 0; i < 256; ++i)
		assert(!pg_artifact_schedule_attach(&p->synthesis, state, 4, jobs));
	denied_graph = NULL;
	assert(!denied_allocations && !p->synthesis.steps);
	FILE *copy = tmpfile();
	assert(copy && !pg_artifact_schedule_write(copy, &p->synthesis, 4, jobs));
	equal_files(saved, copy);
	assert(!fclose(copy) && !fclose(saved));
	trace_count = 0;
	advance(&p->synthesis, 0);
	assert(!trace_count && !p->synthesis.steps);
	advance(&p->synthesis, 100);
	const unsigned expected[] = {0, 0, 3, 1, 2};
	assert(trace_count == 5 && !memcmp(trace, expected, sizeof(expected)));
	assert(p->synthesis.steps == 5 && p->typing.proofs.count == proofs);
	pg_program_destroy(p);
	puts("scheduler: allocation failure preserves old schedule; existing waits reuse without allocation");
	return 0;
}
