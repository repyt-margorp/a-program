#include "computation.h"
#include "computation_io.h"
#include "descriptor_io.h"
#include "iadt.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum materialized_case { CAPTURED, TRAILING, ZERO, NEUTRAL, UNDER, OVER, FOREIGN, CASE_COUNT };

static const struct pg_term *fixture(struct pg_graph *graph, enum materialized_case which,
	const struct pg_term **expected)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_object *z = pg_binder(graph), *unused = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *va = pg_reference(graph, pg_binder(graph));
	const struct pg_term *vb = pg_reference(graph, pg_binder(graph));
	const struct pg_term *self = pg_lambda(graph, z,
		pg_application(graph, pg_reference(graph, z), pg_reference(graph, z)));
	const struct pg_term *omega = pg_application(graph, self, self);
	size_t arities[] = {0, 2};
	const struct pg_data_layout *layout = pg_data_layout(graph, 2, arities);
	const struct pg_object *leaf = pg_data_constructor(layout, 0);
	const struct pg_object *node = pg_data_constructor(layout, 1);
	const struct pg_term *source = pg_application(graph,
		pg_application(graph, pg_reference(graph, node), vx), omega);
	source = pg_application(graph, pg_lambda(graph, x, source), vb);
	const struct pg_term *body = pg_application(graph,
		pg_application(graph, pg_reference(graph, &pg_return_operation), vx), vy);
	const struct pg_term *branch = pg_lambda(graph, y, pg_lambda(graph, unused, body));
	if (which == TRAILING)
		branch = pg_lambda(graph, y, pg_lambda(graph, unused,
			pg_lambda(graph, z, pg_reference(graph, z))));
	if (which == ZERO) source = pg_reference(graph, leaf);
	if (which == NEUTRAL) source = va;
	if (which == UNDER) source = pg_application(graph, pg_reference(graph, node), va);
	if (which == OVER) source = pg_application(graph,
		pg_application(graph, pg_application(graph, pg_reference(graph, node), va), vb), va);
	if (which == FOREIGN) {
		size_t arity = 2;
		const struct pg_data_layout *other = pg_data_layout(graph, 1, &arity);
		source = pg_application(graph,
			pg_application(graph, pg_reference(graph, pg_data_constructor(other, 0)), va), vb);
	}
	struct pg_match_clause clauses[] = {{leaf, which == ZERO ? vx : omega}, {node, branch}};
	const struct pg_term *input = pg_data_match(graph, layout, source, 2, clauses);
	assert(input);
	if (which >= NEUTRAL) { *expected = input; return input; }
	input = pg_application(graph, pg_lambda(graph, x, input), which == ZERO ? vb : va);
	*expected = which == ZERO ? vb : pg_application(graph,
		pg_application(graph, pg_reference(graph, &pg_return_operation), va), vb);
	if (which == TRAILING) {
		input = pg_application(graph, input, va);
		*expected = va;
	}
	return input;
}

static int legacy_dispatch(struct pg_eval *machine)
{
	if (pg_data_layout_view(machine->current.term->as.reference)) {
		/* Seed the existing named materialized continuation. Before any save,
		 * restore the ordinary portable policy; no new descriptor is registered. */
		machine->dispatch = pg_pure_policy.dispatch;
		return pg_eval_demand(machine, 0,
			pg_data_continuation_resolve("iadt/match_answer/v1"), NULL);
	}
	return pg_pure_policy.dispatch(machine);
}

static void seed(struct pg_eval *machine, struct pg_graph *graph, const struct pg_term *input)
{
	pg_computation_eval_init(machine, graph, input);
	machine->dispatch = legacy_dispatch;
	while (machine->dispatch == legacy_dispatch) {
		assert(pg_eval_advance(machine, 1) == PG_EVAL_PENDING);
		assert(machine->steps < 100);
	}
	assert(machine->frames && machine->dispatch == pg_pure_policy.dispatch);
}

static uint64_t finish(struct pg_eval *machine, struct pg_graph *graph, const struct pg_term *expected)
{
	while (pg_eval_advance(machine, 1) == PG_EVAL_PENDING) assert(machine->steps < 10000);
	assert(machine->status == PG_EVAL_WHNF);
	const struct pg_term *answer = pg_eval_readback(machine, graph);
	assert(answer && pg_alpha_equal(answer, expected) == 1);
	return machine->steps;
}

static uint64_t total_steps(struct pg_graph *graph, const struct pg_term *input,
	const struct pg_term *expected, uint64_t *base)
{
	struct pg_eval machine;
	seed(&machine, graph, input);
	*base = machine.steps;
	uint64_t total = finish(&machine, graph, expected);
	pg_eval_destroy(&machine);
	return total;
}

static void save(FILE *file, const struct pg_eval *machine,
	const struct pg_term *input, const struct pg_term *expected)
{
	struct pg_eval_configuration roots[] = {{.head = {input, NULL}}, {.head = {expected, NULL}}};
	assert(!pg_computation_machine_write(file, machine, &pg_pure_policy,
		2, roots, &pg_builtin_graph_codec, NULL));
}

static void load(FILE *file, struct pg_eval *machine, struct pg_graph *graph,
	const struct pg_term **input, const struct pg_term **expected)
{
	const struct pg_eval_policy *policy;
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(!pg_computation_machine_read(file, machine, graph, 10000, 100,
		&pg_builtin_graph_codec, NULL, &policy, &count, &roots));
	assert(policy == &pg_pure_policy && count == 2 && fgetc(file) == EOF);
	*input = roots[0].head.term;
	*expected = roots[1].head.term;
}

static void check_cuts(void)
{
	for (enum materialized_case which = CAPTURED; which < CASE_COUNT; ++which) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		const struct pg_term *expected, *input = fixture(&graph, which, &expected);
		uint64_t base, total = total_steps(&graph, input, expected, &base);
		pg_graph_destroy(&graph);
		for (uint64_t cut = 0; cut <= total - base; ++cut) {
			assert(!pg_graph_init(&graph));
			input = fixture(&graph, which, &expected);
			struct pg_eval machine;
			seed(&machine, &graph, input);
			pg_eval_advance(&machine, cut);
			uint64_t before = machine.steps;
			enum pg_eval_status status = machine.status;
			assert(pg_eval_readback(&machine, &graph));
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file);
				save(file, &machine, input, expected);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				load(file, &machine, &graph, &input, &expected);
				assert(!fclose(file) && machine.steps == before && machine.status == status);
			}
			assert(pg_eval_advance(&machine, 0) == status && machine.steps == before);
			assert(finish(&machine, &graph, expected) == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
		}
		printf("Materialized case=%d base=%" PRIu64 " steps=%" PRIu64 " cuts=%" PRIu64 " pass\n",
			which, base, total, total - base + 1);
	}
}

int main(int argc, char **argv)
{
	if (argc == 1) { check_cuts(); return 0; }
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval machine;
	const struct pg_term *input, *expected;
	if (argc == 5 && !strcmp(argv[1], "write")) {
		char *end;
		unsigned long choice = strtoul(argv[4], &end, 10);
		assert(!*end && choice < CASE_COUNT);
		uint64_t cut = strtoull(argv[3], &end, 10), base;
		assert(!*end);
		input = fixture(&graph, (enum materialized_case)choice, &expected);
		uint64_t total = total_steps(&graph, input, expected, &base);
		assert(cut <= total - base);
		seed(&machine, &graph, input);
		pg_eval_advance(&machine, cut);
		FILE *file = fopen(argv[2], "wb");
		assert(file);
		save(file, &machine, input, expected);
		assert(!fclose(file));
		printf("Materialized writer cut=%" PRIu64 " total=%" PRIu64 "\n", machine.steps, total);
	} else if (argc == 3 && !strcmp(argv[1], "read")) {
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		load(file, &machine, &graph, &input, &expected);
		assert(!fclose(file));
		uint64_t base, total = total_steps(&graph, input, expected, &base), before = machine.steps;
		assert(pg_eval_advance(&machine, 0) == machine.status && machine.steps == before);
		assert(finish(&machine, &graph, expected) == total);
		printf("Materialized reader cut=%" PRIu64 " total=%" PRIu64 " pass\n", before, total);
	} else return 2;
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
	return 0;
}
