#include "eval_internal.h"
#include "eval_io.h"

#include <assert.h>
#include <stdio.h>

static size_t finish(struct materialization *work, struct pg_graph *graph,
	struct pg_eval_configuration input)
{
	size_t steps = 0;
	int status;
	do {
		status = pg_materialize_step(work, graph, input.head, input.arguments);
		assert(status >= 0 && ++steps < 10000);
	} while (!status);
	return steps;
}

int main(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *x = pg_binder(&graph), *z = pg_binder(&graph);
	const struct pg_term *identity = pg_lambda(&graph, x, pg_reference(&graph, x));
	const struct pg_term *dag = identity;
	for (size_t i = 0; i < 8; ++i)
		dag = pg_lambda(&graph, pg_binder(&graph), pg_application(&graph, dag, dag));
	struct pg_environment environment = {z, {identity, NULL}, NULL};
	struct pg_eval_configuration original = {
		{pg_application(&graph, dag, pg_reference(&graph, z)), &environment}, NULL};
	struct materialization baseline = {0};
	size_t total = finish(&baseline, &graph, original);
	for (size_t cut = 0; cut <= total; ++cut) {
		struct materialization work = {0};
		for (size_t i = 0; i < cut; ++i)
			assert(pg_materialize_step(&work, &graph, original.head, NULL) == (i + 1 == total));
		FILE *saved = tmpfile(), *copy = tmpfile();
		assert(saved && copy && !pg_materialization_write(saved, &work, &original, NULL, NULL));
		pg_materialize_destroy(&work);
		rewind(saved);
		struct pg_graph restored;
		assert(!pg_graph_init(&restored));
		struct pg_eval_configuration input;
		assert(!pg_materialization_read(saved, &restored, 10000, 100, NULL, NULL, &work, &input));
		assert(!pg_materialization_write(copy, &work, &input, NULL, NULL));
		rewind(saved);
		rewind(copy);
		int a, b;
		do {
			a = fgetc(saved);
			b = fgetc(copy);
			assert(a == b);
		} while (a != EOF);
		if (cut < total) assert(finish(&work, &restored, input) == total - cut);
		assert(work.done && work.partial->kind == PG_APPLICATION);
		const struct pg_term *result = work.partial;
		assert(pg_alpha_equal(result->as.application.function, input.head.term->as.application.function) == 1);
		assert(result->as.application.argument == input.head.environment->value.term);
#ifdef PG_SUPPORT_CANDIDATE
		assert(result->as.application.function == input.head.term->as.application.function);
#endif
		pg_materialize_destroy(&work);
		pg_graph_destroy(&restored);
		fclose(copy);
		fclose(saved);
	}
	printf("closed/open readback: %zu save/resume boundaries passed\n", total + 1);
	pg_materialize_destroy(&baseline);
	pg_graph_destroy(&graph);
	return 0;
}
