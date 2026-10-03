#include "computation.h"
#include "computation_io.h"
#include "descriptor_io.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum fold_case { CAPTURE, TRAILING, HANDLER, NEUTRAL, OVERAPPLIED, CASE_COUNT };

static const struct pg_term *fixture(struct pg_graph *graph, enum fold_case which,
	const struct pg_term **expected)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_object *z = pg_binder(graph), *k = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *value = pg_lambda(graph, z, vx);
	const struct pg_term *source = pg_application(graph,
		pg_reference(graph, &pg_return_operation), value);
	if (which == NEUTRAL)
		source = pg_application(graph, pg_reference(graph, &pg_force_operation), vx);
	if (which == OVERAPPLIED) source = pg_application(graph, source, vy);
	const struct pg_term *continuation = pg_lambda(graph, k, pg_reference(graph, k));
	/* A return must leave an unused handler clause suspended. */
	const struct pg_term *self = pg_lambda(graph, z,
		pg_application(graph, pg_reference(graph, z), pg_reference(graph, z)));
	struct pg_operation_clause clause = {&pg_request_operation, pg_application(graph, self, self)};
	const struct pg_term *input = pg_computation_fold(graph, source, continuation,
		which == HANDLER ? 1 : 0, which == HANDLER ? &clause : NULL);
	assert(input);
	if (which == NEUTRAL || which == OVERAPPLIED) {
		*expected = input;
		return input;
	}
	input = pg_application(graph, pg_lambda(graph, x, input), vy);
	*expected = pg_lambda(graph, z, vy);
	if (which == TRAILING) {
		input = pg_application(graph, input, pg_reference(graph, pg_binder(graph)));
		*expected = vy;
	}
	return input;
}

static uint64_t finish(struct pg_eval *machine, struct pg_graph *graph,
	const struct pg_term *expected)
{
	while (pg_eval_advance(machine, 1) == PG_EVAL_PENDING) assert(machine->steps < 1000);
	assert(machine->status == PG_EVAL_WHNF);
	const struct pg_term *answer = pg_eval_readback(machine, graph);
	assert(answer && pg_alpha_equal(answer, expected) == 1);
	return machine->steps;
}

static uint64_t total_steps(struct pg_graph *graph, const struct pg_term *input,
	const struct pg_term *expected)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, input);
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
	for (enum fold_case which = CAPTURE; which < CASE_COUNT; ++which) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		const struct pg_term *expected, *input = fixture(&graph, which, &expected);
		uint64_t total = total_steps(&graph, input, expected);
		pg_graph_destroy(&graph);
		for (uint64_t cut = 0; cut <= total; ++cut) {
			assert(!pg_graph_init(&graph));
			input = fixture(&graph, which, &expected);
			struct pg_eval machine;
			pg_computation_eval_init(&machine, &graph, input);
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
		printf("Fold case=%d steps=%" PRIu64 " cuts=%" PRIu64 " pass\n", which, total, total + 1);
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
		uint64_t cut = strtoull(argv[3], &end, 10);
		assert(!*end);
		input = fixture(&graph, (enum fold_case)choice, &expected);
		uint64_t total = total_steps(&graph, input, expected);
		assert(cut <= total);
		pg_computation_eval_init(&machine, &graph, input);
		pg_eval_advance(&machine, cut);
		FILE *file = fopen(argv[2], "wb");
		assert(file);
		save(file, &machine, input, expected);
		assert(!fclose(file));
		printf("Fold writer cut=%" PRIu64 " total=%" PRIu64 "\n", cut, total);
	} else if (argc == 3 && !strcmp(argv[1], "read")) {
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		load(file, &machine, &graph, &input, &expected);
		assert(!fclose(file));
		uint64_t total = total_steps(&graph, input, expected), before = machine.steps;
		assert(pg_eval_advance(&machine, 0) == machine.status && machine.steps == before);
		assert(finish(&machine, &graph, expected) == total);
		printf("Fold reader cut=%" PRIu64 " total=%" PRIu64 " pass\n", before, total);
	} else return 2;
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
	return 0;
}
