#include "computation.h"
#include "computation_io.h"
#include "descriptor_io.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct pg_term *force_input(struct pg_graph *graph,
	const struct pg_term **expected)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *deep = vx, *body = vy;
	for (size_t i = 0; i < 1000; ++i) {
		deep = pg_application(graph, deep, deep);
		body = pg_application(graph, body, body);
	}
	/* Substitution would capture free y unless the returned lambda is freshened. */
	*expected = pg_lambda(graph, pg_binder(graph), body);
	const struct pg_term *quoted = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
		pg_lambda(graph, y, deep));
	return pg_application(graph, pg_lambda(graph, x,
		pg_application(graph, pg_reference(graph, &pg_force_operation), quoted)), vy);
}

static void capture_result(const struct pg_term *result, const struct pg_term *expected)
{
	assert(result && result->kind == PG_LAMBDA);
	assert(pg_alpha_equal(result, expected) == 1);
	const struct pg_term *free = expected->as.lambda.body;
	while (free->kind == PG_APPLICATION) free = free->as.application.function;
	assert(free->kind == PG_REFERENCE && result->as.lambda.binder != free->as.reference);
}

static void head_and_effort(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *expected;
	const struct pg_term *input = force_input(&graph, &expected);
	struct pg_eval whole;
	pg_computation_eval_init(&whole, &graph, input);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	uint64_t raw_steps = whole.steps;
	assert(raw_steps < 100);
	capture_result(pg_eval_readback(&whole, &graph), expected);
	pg_eval_destroy(&whole);
	for (uint64_t cut = 0; cut <= raw_steps; ++cut) {
		struct pg_eval split;
		pg_computation_eval_init(&split, &graph, input);
		pg_eval_advance(&split, cut);
		while (pg_eval_advance(&split, 1) == PG_EVAL_PENDING) assert(split.steps < raw_steps);
		assert(split.status == PG_EVAL_WHNF && split.steps == raw_steps);
		capture_result(pg_eval_readback(&split, &graph), expected);
		pg_eval_destroy(&split);
	}
	struct pg_whnf_work work;
	assert(!pg_whnf_work_init(&work, &graph));
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input);
	assert(pg_whnf_advance(job, 100) == PG_EVAL_PENDING);
	assert(pg_whnf_steps(job) == 100 && !pg_whnf_result(job) && !pg_whnf_certificate(job));
	assert(pg_whnf_advance(job, UINT64_MAX) == PG_EVAL_WHNF);
	uint64_t charged_steps = pg_whnf_steps(job);
	assert(charged_steps > 1000 && charged_steps > raw_steps);
	capture_result(pg_whnf_result(job), expected);
	assert(pg_reduction_source(pg_whnf_certificate(job)) == input);
	assert(pg_reduction_target(pg_whnf_certificate(job)) == pg_whnf_result(job));
	pg_whnf_work_destroy(&work);
	assert(!pg_whnf_work_init(&work, &graph));
	job = pg_whnf_request(&work, &pg_pure_policy, input);
	while (pg_whnf_advance(job, 7) == PG_EVAL_PENDING) {
		assert(!pg_whnf_result(job) && !pg_whnf_certificate(job));
		assert(pg_whnf_steps(job) < charged_steps);
	}
	assert(pg_whnf_status(job) == PG_EVAL_WHNF && pg_whnf_steps(job) == charged_steps);
	capture_result(pg_whnf_result(job), expected);
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
	printf("head/capture: all %" PRIu64 " raw cuts; WHNF charges %" PRIu64 " transitions\n",
		raw_steps + 1, charged_steps);
}

static void write_machine(const char *path, uint64_t cut)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *expected;
	const struct pg_term *input = force_input(&graph, &expected);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, input);
	pg_eval_advance(&machine, cut);
	struct pg_eval_configuration roots[] = {{.head = {expected, NULL}}};
	FILE *file = fopen(path, "wb");
	assert(file && !pg_computation_machine_write(file, &machine, &pg_pure_policy,
		1, roots, &pg_builtin_graph_codec, &graph));
	assert(!fclose(file));
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

static void read_machine(const char *path)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval machine;
	const struct pg_eval_policy *policy;
	size_t count;
	const struct pg_eval_configuration *roots;
	FILE *file = fopen(path, "rb");
	assert(file && !pg_computation_machine_read(file, &machine, &graph, 100000, 100,
		&pg_builtin_graph_codec, &graph, &policy, &count, &roots));
	assert(!fclose(file) && policy == &pg_pure_policy && count == 1);
	uint64_t before = machine.steps;
	assert(pg_eval_advance(&machine, 0) == machine.status && machine.steps == before);
	while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < 100);
	assert(machine.status == PG_EVAL_WHNF);
	capture_result(pg_eval_readback(&machine, &graph), roots[0].head.term);
	printf("fresh-process head resume: total raw steps=%" PRIu64 "\n", machine.steps);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	if (argc == 1) { head_and_effort(); return 0; }
	if (argc == 4 && !strcmp(argv[1], "write")) {
		char *end;
		uint64_t cut = strtoull(argv[3], &end, 10);
		assert(!*end && cut <= 8);
		write_machine(argv[2], cut);
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "read")) { read_machine(argv[2]); return 0; }
	return 2;
}
