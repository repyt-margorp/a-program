#include "computation.h"
#include "computation_io.h"
#include "descriptor_io.h"
#include "eval_internal.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum result_case { CAPTURE, TRAILING, FOLD_FALLBACK, NEUTRAL, OVERAPPLIED, CASE_COUNT };

static const struct pg_term *fixture(struct pg_graph *graph, enum result_case which, size_t depth,
	const struct pg_term **expected)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *projection = pg_reference(graph, &pg_total_result_operation);
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_term *input;
	if (which == CAPTURE) {
		const struct pg_term *deep = vx, *body = vy;
		for (size_t i = 0; i < depth; ++i) {
			deep = pg_application(graph, deep, deep);
			body = pg_application(graph, body, body);
		}
		/* The returned binder y must not capture the free y supplied for x. */
		*expected = pg_lambda(graph, pg_binder(graph), body);
		const struct pg_term *value = pg_lambda(graph, y, deep);
		input = pg_application(graph, projection, pg_application(graph, ret, value));
		return pg_application(graph, pg_lambda(graph, x, input), vy);
	}
	if (which == TRAILING) {
		*expected = vy;
		const struct pg_term *value = pg_lambda(graph, y, vx);
		input = pg_application(graph, projection, pg_application(graph, ret, value));
		/* The trailing argument differs from the captured result value. */
		input = pg_application(graph, input, pg_reference(graph, pg_binder(graph)));
		return pg_application(graph, pg_lambda(graph, x, input), vy);
	}
	if (which == FOLD_FALLBACK) {
		*expected = vy;
		const struct pg_term *force = pg_reference(graph, &pg_force_operation);
		const struct pg_term *source = pg_application(graph, force, vx);
		const struct pg_term *branch = pg_lambda(graph, pg_binder(graph), pg_application(graph, ret, vy));
		return pg_application(graph, projection, pg_computation_fold(graph, source, branch, 0, NULL));
	}
	input = which == NEUTRAL ? vx : pg_application(graph, pg_application(graph, ret, vx), vy);
	input = pg_application(graph, projection, input);
	*expected = input;
	return input;
}

static void equal_result(struct pg_eval *machine, struct pg_graph *graph,
	const struct pg_term *expected)
{
	assert(machine->status == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(machine, graph);
	assert(result && pg_alpha_equal(result, expected) == 1);
}

static uint64_t raw_total(struct pg_graph *graph, const struct pg_term *input,
	const struct pg_term *expected)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, input);
	assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
	uint64_t total = machine.steps;
	equal_result(&machine, graph, expected);
	pg_eval_destroy(&machine);
	return total;
}

static void save_machine(FILE *file, struct pg_eval *machine,
	const struct pg_term *input, const struct pg_term *expected)
{
	struct pg_eval_configuration roots[] = {{.head = {input, NULL}}, {.head = {expected, NULL}}};
	assert(!pg_computation_machine_write(file, machine, &pg_pure_policy,
		2, roots, &pg_builtin_graph_codec, NULL));
}

static void load_machine(FILE *file, struct pg_eval *machine, struct pg_graph *graph,
	const struct pg_term **input, const struct pg_term **expected)
{
	const struct pg_eval_policy *policy;
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(!pg_computation_machine_read(file, machine, graph, 100000, 100,
		&pg_builtin_graph_codec, NULL, &policy, &count, &roots));
	assert(policy == &pg_pure_policy && count == 2);
	assert(!roots[0].head.environment && !roots[1].head.environment);
	*input = roots[0].head.term;
	*expected = roots[1].head.term;
}

static void semantic_cuts(void)
{
	for (enum result_case which = CAPTURE; which < CASE_COUNT; ++which) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		const struct pg_term *expected;
		const struct pg_term *input = fixture(&graph, which, 64, &expected);
		uint64_t total = raw_total(&graph, input, expected);
		pg_graph_destroy(&graph);
		unsigned projection_cuts = 0, head_cuts = 0, captured_cuts = 0, readback_cuts = 0;
		for (uint64_t cut = 0; cut <= total; ++cut) {
			assert(!pg_graph_init(&graph));
			input = fixture(&graph, which, 64, &expected);
			struct pg_eval machine;
			pg_computation_eval_init(&machine, &graph, input);
			pg_eval_advance(&machine, cut);
			for (const struct pg_eval_frame *frame = machine.frames; frame; frame = frame->parent) {
				const char *name = frame->continuation->name;
				projection_cuts += !strcmp(name, "computation/total_result/v1") ||
					!strcmp(name, "computation/total_result_head/v1");
				head_cuts += !strcmp(name, "computation/total_result_head/v1");
				readback_cuts += frame->answer.readback.output != NULL;
			}
			if (machine.current.term->kind == PG_REFERENCE &&
				machine.current.term->as.reference == &pg_return_operation && machine.arguments)
				captured_cuts += machine.arguments->value.environment != NULL;
			uint64_t before = machine.steps;
			enum pg_eval_status status = machine.status;
			int ready = machine.head_ready, framed = machine.frames != NULL;
			const struct pg_eval_work_operation *operation = machine.task ? machine.task->operation : NULL;
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file);
				save_machine(file, &machine, input, expected);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				load_machine(file, &machine, &graph, &input, &expected);
				assert(!fclose(file) && machine.steps == before && machine.status == status);
				assert(machine.head_ready == ready && (machine.frames != NULL) == framed);
				assert((machine.task ? machine.task->operation : NULL) == operation);
			}
			assert(pg_eval_advance(&machine, 0) == status && machine.steps == before);
			while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < total);
			assert(machine.steps == total);
			equal_result(&machine, &graph, expected);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
		}
		assert(projection_cuts);
		if (which == CAPTURE && pg_computation_continuation_resolve("computation/total_result_head/v1"))
			assert(head_cuts && captured_cuts);
		if (which == FOLD_FALLBACK) assert(readback_cuts);
		printf("TotalResult case=%d: all %" PRIu64 " raw reload cuts passed; projection=%u head=%u captured=%u readback=%u\n",
			which, total + 1, projection_cuts, head_cuts, captured_cuts, readback_cuts);
	}
}

static void charged_readback(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *expected;
	const struct pg_term *input = fixture(&graph, CAPTURE, 1000, &expected);
	struct pg_whnf_work work;
	assert(!pg_whnf_work_init(&work, &graph));
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input);
	assert(job && pg_whnf_advance(job, 0) == PG_EVAL_PENDING && !pg_whnf_steps(job));
	assert(pg_whnf_advance(job, 100) == PG_EVAL_PENDING && pg_whnf_steps(job) == 100);
	assert(!pg_whnf_result(job) && !pg_whnf_certificate(job));
	assert(pg_whnf_advance(job, UINT64_MAX) == PG_EVAL_WHNF);
	uint64_t total = pg_whnf_steps(job);
	assert(total > 1000 && pg_alpha_equal(pg_whnf_result(job), expected) == 1);
	assert(pg_reduction_source(pg_whnf_certificate(job)) == input);
	assert(pg_reduction_target(pg_whnf_certificate(job)) == pg_whnf_result(job));
	pg_whnf_work_destroy(&work);
	assert(!pg_whnf_work_init(&work, &graph));
	job = pg_whnf_request(&work, &pg_pure_policy, input);
	while (pg_whnf_advance(job, 7) == PG_EVAL_PENDING) {
		assert(!pg_whnf_result(job) && !pg_whnf_certificate(job));
		assert(pg_whnf_steps(job) < total);
	}
	assert(pg_whnf_status(job) == PG_EVAL_WHNF && pg_whnf_steps(job) == total);
	assert(pg_alpha_equal(pg_whnf_result(job), expected) == 1);
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
	printf("TotalResult WHNF: final readback charged, chunks agree at %" PRIu64 " steps\n", total);
}

int main(int argc, char **argv)
{
	if (argc == 1) { semantic_cuts(); charged_readback(); return 0; }
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval machine;
	const struct pg_term *input, *expected;
	if ((argc == 4 || argc == 5) && !strcmp(argv[1], "write")) {
		char *end;
		uint64_t cut = strtoull(argv[3], &end, 10);
		assert(!*end);
		enum result_case which = CAPTURE;
		if (argc == 5) {
			unsigned long choice = strtoul(argv[4], &end, 10);
			assert(!*end && choice < CASE_COUNT);
			which = (enum result_case)choice;
		}
		input = fixture(&graph, which, 64, &expected);
		uint64_t total = raw_total(&graph, input, expected);
		assert(cut <= total);
		pg_computation_eval_init(&machine, &graph, input);
		pg_eval_advance(&machine, cut);
		FILE *file = fopen(argv[2], "wb");
		assert(file);
		save_machine(file, &machine, input, expected);
		assert(!fclose(file));
		printf("TotalResult writer: cut=%" PRIu64 " total=%" PRIu64 "\n", cut, total);
	} else if (argc == 3 && !strcmp(argv[1], "read")) {
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		load_machine(file, &machine, &graph, &input, &expected);
		assert(!fclose(file));
		uint64_t total = raw_total(&graph, input, expected), before = machine.steps;
		assert(pg_eval_advance(&machine, 0) == machine.status && machine.steps == before);
		while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < total);
		assert(machine.steps == total);
		equal_result(&machine, &graph, expected);
		printf("TotalResult fresh reader: total=%" PRIu64 "\n", total);
	} else {
		pg_graph_destroy(&graph);
		return 2;
	}
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
	return 0;
}
