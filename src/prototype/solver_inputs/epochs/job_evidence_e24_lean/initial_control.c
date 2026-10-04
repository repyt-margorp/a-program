/* Instrument only the schema producer's allocator. The checker keeps its own
 * ordinary allocation and admission paths. */
#define main synthesis_full_main
#include "synthesis.c"
#undef main

static struct pg_graph *persistent_graph;
static size_t denied_allocations;
static int deny_scratch;

void *pg_schema_control_alloc(struct pg_graph *graph, size_t bytes)
{
	if (deny_scratch && graph != persistent_graph) {
		++denied_allocations;
		return NULL;
	}
	return pg_alloc(graph, bytes);
}

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work reduction;
	struct pg_synthesis synthesis;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&reduction, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &reduction, PG_DEFINITION_EXPLICIT_THUNK));
	persistent_graph = &graph;
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	struct pg_synthesis_job *job = pg_synthesis_data_schema(&synthesis, root,
		expression_syntax(&graph, "Flag:=@{off:*; on:*;};"));
	assert(job);
	uint64_t steps = synthesis.steps;
	size_t proofs = typing.proofs.count;
	pg_synthesis_advance(&synthesis, 0);
	assert(synthesis.steps == steps && typing.proofs.count == proofs);
	deny_scratch = 1;
	complete(&synthesis, job, PG_SYNTHESIS_DONE);
	assert(!denied_allocations);
	const struct pg_data_schema *schema = pg_synthesis_schema_result(job);
	const struct source_constructor *members = pg_synthesis_schema_members(job);
	assert(schema && members && pg_data_constructor_count(schema) == 2);
	for (size_t i = 0; i < 2; ++i)
		assert(pg_synthesis_result(members[i].producer) == pg_data_schema_result(schema,
			pg_data_constructor(pg_data_schema_layout(schema), i)));
	const struct pg_data_declaration *allocation = pg_data_schema_declaration(schema);
	struct pg_synthesis_job *denied = pg_synthesis_data_schema_at(&synthesis, root,
		expression_syntax(&graph, "Restored:=@{off:*; on:*;};"), allocation);
	assert(denied);
	complete(&synthesis, denied, PG_SYNTHESIS_ERROR);
	assert(denied_allocations == 1 && !pg_synthesis_schema_result(denied));
	deny_scratch = 0;
	struct pg_synthesis_job *restored = pg_synthesis_data_schema_at(&synthesis, root,
		expression_syntax(&graph, "Rechecked:=@{off:*; on:*;};"), allocation);
	complete(&synthesis, restored, PG_SYNTHESIS_DONE);
	assert(pg_data_schema_declaration(pg_synthesis_schema_result(restored)) == allocation);
	struct pg_synthesis_job *changed = pg_synthesis_data_schema_at(&synthesis, root,
		expression_syntax(&graph, "Changed:=@{off:@->*; on:*;};"), allocation);
	complete(&synthesis, changed, PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_schema_result(changed));
	pg_synthesis_destroy(&synthesis);
	assert(pg_data_constructor_count(schema) == 2);
	pg_whnf_work_destroy(&reduction);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("schema: existing checked inputs need no producer scratch array; nominal conversion and rejection remain checked");
	return 0;
}
