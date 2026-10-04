/* Observe the existing allocator/index calls only during Source capture.
 * These counts exclude bucket allocations and are not peak-memory or timing. */
#define main source_checkpoint_main
#include "source_checkpoint_test.c"
#undef main

static const struct pg_graph *persistent_graph;
static size_t local_indexes, scratch_calls, scratch_requested_bytes;
static int observing;

void *__real_pg_alloc(struct pg_graph *, size_t);
void *__wrap_pg_alloc(struct pg_graph *graph, size_t bytes)
{
	if (observing && graph != persistent_graph) {
		++scratch_calls;
		scratch_requested_bytes += bytes;
	}
	return __real_pg_alloc(graph, bytes);
}

int __real_pg_index_init(struct pg_index *);
int __wrap_pg_index_init(struct pg_index *index)
{
	if (observing) ++local_indexes;
	return __real_pg_index_init(index);
}

int main(int argc, char **argv)
{
	assert(argc == 1 || argc == 2);
	struct pg_program *p = start("{{ first := @; second := #1; main := second; }}.main");
	advance(&p->synthesis, 12);
	struct checkpoint c = collect(p);
	assert(c.source_count);
	FILE *before = tmpfile(), *after = tmpfile();
	assert(before && after && !pg_artifact_source_write(before, c.source));
	size_t proofs = p->typing.proofs.count, requests = p->synthesis.jobs.count;
	uint64_t steps = p->synthesis.steps;
	persistent_graph = &p->graph;
	observing = 1;
	const struct pg_artifact_source *captured = pg_artifact_source_capture(&p->graph,
		&p->synthesis, c.count, c.jobs, c.source_count, c.sources);
	observing = 0;
	assert(captured && !pg_artifact_source_write(after, captured));
	equal(before, after);
	assert(p->synthesis.steps == steps && p->typing.proofs.count == proofs
		&& p->synthesis.jobs.count == requests);
	printf("jobs\t%zu\nowners\t%zu\nlocal_indexes\t%zu\nscratch_calls\t%zu\nscratch_requested_bytes\t%zu\nsource_bytes_equal\t1\n",
		c.count, c.source_count, local_indexes, scratch_calls, scratch_requested_bytes);
	if (argc == 2) {
		FILE *image = fopen(argv[1], "wb");
		assert(image && !pg_artifact_source_write(image, captured) && !fclose(image));
	}
	assert(!fclose(before) && !fclose(after));
	pg_program_destroy(p);
	return 0;
}
