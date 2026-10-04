#include "eval_io.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct pg_object value_a = {.kind = PG_SEMANTIC_OBJECT};
static const struct pg_object value_b = {.kind = PG_SEMANTIC_OBJECT};

struct binding_view {
	struct pg_binding_value values[4];
	size_t count, calls;
};

static struct pg_binding_value binding_at(const void *owner, size_t index)
{
	struct binding_view *view = (struct binding_view *)owner;
	++view->calls;
	return index < view->count ? view->values[index] : (struct pg_binding_value){0};
}

static const char *object_name(void *owner, const struct pg_object *object)
{
	(void)owner;
	return object == &value_a ? "value-a" : object == &value_b ? "value-b" : NULL;
}

static const struct pg_object *object_resolve(void *owner, const char *name)
{
	(void)owner;
	return !strcmp(name, "value-a") ? &value_a : !strcmp(name, "value-b") ? &value_b : NULL;
}

static const struct pg_graph_codec codec = {.name = object_name, .resolve = object_resolve};

static const struct pg_term *make_input(struct pg_graph *graph, unsigned which, struct binding_view *view)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_term *rx = pg_reference(graph, x), *ry = pg_reference(graph, y), *rz = pg_reference(graph, z);
	const struct pg_term *a = pg_reference(graph, &value_a), *b = pg_reference(graph, &value_b);
	assert(x && y && z && rx && ry && rz && a && b);
	switch (which) {
	case 0:
		*view = (struct binding_view){.values = {{x, rx}, {y, ry}, {z, rz}}, .count = 3};
		return pg_application(graph, rx, ry);
	case 1:
		/* The final identity shadows a nonidentity for the same binder. */
		*view = (struct binding_view){.values = {{z, rz}, {x, rx}, {x, a}, {x, rx}}, .count = 4};
		return rx;
	case 2:
		*view = (struct binding_view){.values = {{z, rz}, {x, b}, {y, a}}, .count = 3};
		return pg_application(graph, rx, ry);
	case 3:
		*view = (struct binding_view){.values = {{z, rz}, {y, a}, {x, b}}, .count = 3};
		return pg_lambda(graph, x, pg_application(graph, rx, ry));
	case 4:
		*view = (struct binding_view){.values = {{x, rx}, {y, b}}, .count = 2};
		return a;
	case 5:
		*view = (struct binding_view){0};
		return rx;
	default:
		return NULL;
	}
}

static void check_result(struct pg_graph *graph, unsigned which, const struct pg_substitution *work)
{
	const struct pg_closure *input = pg_substitution_input(work);
	const struct pg_term *result = pg_substitution_result(work);
	assert(input && result);
	const struct pg_term *expected = NULL;
	switch (which) {
	case 0: case 1: case 5:
		expected = input->term;
		break;
	case 2:
		expected = pg_application(graph, pg_reference(graph, &value_b), pg_reference(graph, &value_a));
		break;
	case 3: {
		assert(input->term->kind == PG_LAMBDA);
		const struct pg_object *binder = input->term->as.lambda.binder;
		expected = pg_lambda(graph, binder,
			pg_application(graph, pg_reference(graph, binder), pg_reference(graph, &value_a)));
		break;
	}
	case 4:
		expected = pg_reference(graph, &value_a);
		break;
	}
	assert(expected && pg_alpha_equal(expected, result) == 1);
}

static void negatives(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_substitution_work shared;
	assert(!pg_substitution_work_init(&shared, &graph));
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_term *reference = pg_reference(&graph, binder);
	const struct pg_term *constant = pg_reference(&graph, &value_a);
	struct binding_view view = {.values = {{binder, reference}, {NULL, reference}}, .count = 2};
	struct pg_binding_inputs inputs = {.owner = &view, .at = binding_at};
	struct pg_substitution local = {0};
	/* Validate the entire input even after an identity or on a constant term. */
	assert(!pg_substitution_request(&shared, reference, 2, inputs));
	assert(!pg_substitution_request(&shared, constant, 2, inputs));
	assert(pg_substitution_init(&local, &graph, reference, 2, view.values) == -1);
	assert(pg_substitution_init(&local, &graph, constant, 2, view.values) == -1);
	view.values[1] = (struct pg_binding_value){&value_a, reference};
	assert(!pg_substitution_request(&shared, constant, 2, inputs));
	assert(pg_substitution_init(&local, &graph, constant, 2, view.values) == -1);
	view.values[1] = (struct pg_binding_value){binder, NULL};
	assert(!pg_substitution_request(&shared, reference, 2, inputs));
	assert(pg_substitution_init(&local, &graph, reference, 2, view.values) == -1);
	inputs.first = SIZE_MAX;
	assert(!pg_substitution_request(&shared, reference, 2, inputs));
	assert(!pg_substitution_request(&shared, reference, SIZE_MAX, inputs));
	assert(!pg_substitution_request(&shared, NULL, 0, inputs));
	pg_substitution_work_destroy(&shared);
	pg_graph_destroy(&graph);
	puts("Invalid trailing/constant/semantic-binder/overflow/null inputs rejected");
}

static int create(unsigned which, uint64_t budget, const char *path)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct binding_view view;
	const struct pg_term *term = make_input(&graph, which, &view);
	assert(term);
	struct pg_substitution_work shared;
	assert(!pg_substitution_work_init(&shared, &graph));
	struct pg_binding_inputs inputs = {.owner = &view, .at = binding_at};
	struct pg_substitution *work = pg_substitution_request(&shared, term, view.count, inputs);
	assert(work);
	size_t initial_reads = view.calls;
	assert(pg_substitution_request(&shared, term, view.count, inputs) == work);
	uint64_t before = pg_substitution_steps(work);
	enum pg_substitution_status status = pg_substitution_status(work);
	assert(pg_substitution_advance(work, 0) == status && pg_substitution_steps(work) == before);
	pg_substitution_advance(work, budget);
	if (path) {
		FILE *stream = fopen(path, "wb");
		assert(stream && fputc((int)which, stream) != EOF);
		assert(!pg_substitution_write(stream, work, &codec, NULL) && !fclose(stream));
	} else {
		assert(pg_substitution_status(work) == PG_SUBSTITUTION_DONE);
		check_result(&graph, which, work);
		struct pg_substitution local = {0};
		assert(!pg_substitution_init(&local, &graph, term, view.count, view.values));
		assert(pg_substitution_advance(&local, UINT64_MAX) == PG_SUBSTITUTION_DONE);
		assert(pg_substitution_steps(&local) == pg_substitution_steps(work));
		check_result(&graph, which, &local);
		pg_substitution_destroy(&local);
	}
	printf("Prefix case=%u steps=%llu initial_reads=%zu status=%d pass\n", which,
		(unsigned long long)pg_substitution_steps(work), initial_reads, pg_substitution_status(work));
	pg_substitution_work_destroy(&shared);
	pg_graph_destroy(&graph);
	return 0;
}

static int read_image(const char *path)
{
	FILE *stream = fopen(path, "rb");
	assert(stream);
	int which = fgetc(stream);
	assert(which >= 0 && which < 6);
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_substitution work = {0};
	assert(!pg_substitution_read(stream, &graph, 10000, 100, &codec, NULL, &work));
	assert(fgetc(stream) == EOF && !fclose(stream));
	uint64_t before = pg_substitution_steps(&work);
	enum pg_substitution_status status = pg_substitution_status(&work);
	assert(pg_substitution_advance(&work, 0) == status && pg_substitution_steps(&work) == before);
	assert(pg_substitution_advance(&work, UINT64_MAX) == PG_SUBSTITUTION_DONE);
	check_result(&graph, (unsigned)which, &work);
	printf("Prefix case=%d total=%llu pass\n", which, (unsigned long long)pg_substitution_steps(&work));
	pg_substitution_destroy(&work);
	pg_graph_destroy(&graph);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		negatives();
		for (unsigned which = 0; which < 6; ++which) create(which, UINT64_MAX, NULL);
		return 0;
	}
	if (argc == 5 && !strcmp(argv[1], "write"))
		return create((unsigned)strtoul(argv[3], NULL, 10), strtoull(argv[4], NULL, 10), argv[2]);
	if (argc == 3 && !strcmp(argv[1], "read")) return read_image(argv[2]);
	return 2;
}
