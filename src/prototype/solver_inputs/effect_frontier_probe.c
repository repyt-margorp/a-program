#include "synthesis_work.h"
#include "effect_inference.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Same public request on each revision; inspect retained scheduling storage,
 * not RSS, transient allocations or semantic acceptance. */
int main(int argc, char **argv)
{
	unsigned depth = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 512;
	assert(depth <= 4096);
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work normalization;
	struct pg_synthesis synthesis;
	struct pg_effect_inference effects;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&normalization, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &normalization, PG_DEFINITION_IMPLICIT_THUNK));
	assert(!pg_effect_inference_init(&effects, &graph));
	struct pg_object labels[2] = {{.kind = PG_SEMANTIC_OBJECT}, {.kind = PG_SEMANTIC_OBJECT}};
	const struct pg_effect_row *empty = pg_effect_row(&graph, 0, NULL), *rows[2];
	for (size_t i = 0; i < 2; ++i) rows[i] = pg_effect_row(&graph, 1, (const struct pg_object *[]){&labels[i]});
	struct pg_effect_equation *source = pg_effect_equation(&effects, rows[0]);
	struct pg_effect_equation *target = pg_effect_equation(&effects, empty);
	const struct pg_term *root = pg_effect_join_term(&graph,
		pg_reference(&graph, pg_effect_equation_parameter(&effects, source)), pg_effect_reference(&graph, rows[1]));
	for (unsigned i = 0; i < depth; ++i) root = pg_effect_join_term(&graph, root, root);
	struct pg_synthesis_job *job = pg_synthesis_row_contribution(&synthesis, &effects, target, rows[0], root);
	assert(job);
	pg_synthesis_advance(&synthesis, 0);
	assert(!synthesis.steps && synthesis.jobs.count == 1 && !effects.dependencies.count);
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(synthesis.steps < 100000);
		pg_synthesis_advance(&synthesis, 1);
	}
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE && !pg_synthesis_result(job));
	assert(effects.dependencies.count == 2);
	pg_effect_inference_seal(&effects);
	while (!pg_effect_inference_advance(&effects, 1)) {}
	assert(pg_effect_inference_result(&effects, target) == rows[1]);
	size_t bytes = 0;
	for (size_t i = 0; i < synthesis.jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis.jobs.buckets[i]; entry; entry = entry->next) {
			const struct pg_synthesis_job *work = (const void *)entry;
			size_t align = _Alignof(struct pg_synthesis_job);
			bytes += sizeof(*work) + work->input_count * sizeof(*work->inputs)
				+ (pg_synthesis_work_role(work)->size + align - 1) / align * align;
		}
	printf("%u\t%zu\t%zu\t%llu\t%zu\t%zu\n", depth, synthesis.jobs.count, bytes,
		(unsigned long long)synthesis.steps, graph.terms.count, typing.proofs.count);
	pg_synthesis_destroy(&synthesis);
	pg_effect_inference_destroy(&effects);
	pg_whnf_work_destroy(&normalization);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}
