#include "identity_internal.h"
#include "wire.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *name(void *unused, const struct pg_object *object)
{
	(void)unused;
	return pg_identity_name(object);
}
static const struct pg_object *resolve(void *unused, const char *text)
{
	(void)unused;
	return pg_identity_resolve(text);
}
static const struct pg_graph_codec codec = {.name = name, .resolve = resolve};

static struct action_body_work *fixture(struct pg_graph *graph, struct pg_graph *arena, int different,
	const struct pg_term **request, const struct pg_term **expected)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *body = pg_application(graph, vx, pg_lambda(graph, a, pg_reference(graph, a)));
	const struct pg_term *source = pg_lambda(graph, x, pg_lambda(graph, y, pg_lambda(graph, z, body)));
	struct action_body_work *work = pg_alloc(arena, sizeof(*work));
	assert(work);
	work->scope = (struct action_scope){source, body, 3, NULL};
	work->answer = different ? vx : pg_application(graph, vx, pg_lambda(graph, b, pg_reference(graph, b)));
	work->arena = arena;
	work->output = graph;
	assert(!pg_comparison_init(&work->comparison, body, work->answer, NULL, NULL));
	*request = pg_identity_action(graph, source);
	*expected = different ? pg_identity_action(graph, pg_lambda(graph, x,
		pg_lambda(graph, y, pg_lambda(graph, z, vx)))) : *request;
	return work;
}

static size_t finish(struct action_body_work *work)
{
	size_t count = 0;
	int status;
	do {
		status = pg_action_body_operation.poll(work);
		assert(status >= 0 && ++count < 10000);
	} while (!status);
	return count;
}

static void invalid_headers(FILE *file, struct pg_graph *arena, struct pg_graph *output)
{
	const long offsets[] = {8, 16, 24, 32};
	const uint64_t values[] = {4, 10001, 4, 2};
	for (size_t i = 0; i < 4; ++i) {
		uint64_t prior;
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_read_u64(file, &prior));
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, values[i]));
		rewind(file);
		struct action_body_work *rejected;
		size_t count = 1;
		const struct pg_term *const *roots;
		assert(pg_action_body_read(file, arena, output, 10000, 100, &codec, NULL, &rejected, &count, &roots));
		assert(!rejected && !count && !roots);
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, prior));
	}
}

static void check_resume(struct action_body_work *work, const struct pg_term *request, const struct pg_term *expected)
{
	struct pg_eval machine;
	pg_eval_init(&machine, request);
	machine.output = work->output;
	/* Ordinary APP establishes the caller and its source argument. */
	assert(pg_eval_advance(&machine, 1) == PG_EVAL_PENDING);
	assert(!pg_eval_defer(&machine, &pg_action_body_operation, work));
	assert(pg_eval_advance(&machine, 10) == PG_EVAL_WHNF);
	const struct pg_term *answer = pg_eval_readback(&machine, work->output);
	assert(pg_alpha_equal(answer, expected) == 1);
	pg_eval_destroy(&machine);
}

static void all_cuts(void)
{
	for (int different = 0; different < 2; ++different) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *request, *expected;
		struct action_body_work *baseline = fixture(&graph, &arena, different, &request, &expected);
		size_t total = finish(baseline);
		pg_action_body_operation.destroy(baseline);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		unsigned phases = 0;
		for (size_t cut = 0; cut <= total; ++cut) {
			assert(!pg_graph_init(&graph));
			struct action_body_work *work = fixture(&graph, &arena, different, &request, &expected);
			for (size_t i = 0; i < cut; ++i) assert(pg_action_body_operation.poll(work) == (i + 1 == total));
			phases |= 1u << work->phase;
			const struct pg_term *initial[] = {request, expected};
			const struct pg_term *const *roots = initial;
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file && !pg_action_body_write(file, work, 2, roots, &codec, NULL));
				invalid_headers(file, &arena, &graph);
				pg_action_body_operation.destroy(work);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t count;
				assert(!pg_action_body_read(file, &arena, &graph, 10000, 100, &codec, NULL, &work, &count, &roots));
				assert(count == 2);
				assert(roots[0]->as.application.argument == work->scope.source);
				assert(!fclose(file));
			}
			for (size_t i = cut; i < total; ++i) assert(pg_action_body_operation.poll(work) == (i + 1 == total));
			assert(work->phase == BODY_READY);
			check_resume(work, roots[0], roots[1]);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
		}
		assert(phases == (different ? 15u : (1u << BODY_COMPARE) | (1u << BODY_READY)));
	}
}

int main(int argc, char **argv)
{
	if (argc == 3) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		if (!strcmp(argv[1], "write")) {
			const struct pg_term *roots[2];
			struct action_body_work *work = fixture(&graph, &arena, 1, &roots[0], &roots[1]);
			for (size_t i = 0; i < 5; ++i) assert(!pg_action_body_operation.poll(work));
			FILE *file = fopen(argv[2], "wb");
			assert(file && !pg_action_body_write(file, work, 2, roots, &codec, NULL));
			assert(!fclose(file));
			pg_action_body_operation.destroy(work);
		} else {
			assert(!strcmp(argv[1], "read"));
			FILE *file = fopen(argv[2], "rb");
			struct action_body_work *work;
			size_t count;
			const struct pg_term *const *roots;
			assert(file && !pg_action_body_read(file, &arena, &graph, 10000, 100, &codec, NULL, &work, &count, &roots));
			assert(!fclose(file) && count == 2);
			finish(work);
			check_resume(work, roots[0], roots[1]);
		}
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		return 0;
	}
	assert(argc == 1);
	all_cuts();
	puts("Identity body: compare/collect/wrap/ready preserve owner sharing and resume through the original operation");
	return 0;
}
