#include "graph.h"
#include "dimension.h"
#include "eval.h"
#include "typing.h"

#include <assert.h>
#include <stdio.h>

static void graph_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *vz = pg_reference(graph, z);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	assert(identity == pg_lambda(graph, x, vx));
	assert(identity != pg_lambda(graph, y, vy));
	assert(pg_alpha_equal(identity, pg_lambda(graph, y, vy)) == 1);
	assert(vx != vy);
	assert(pg_lambda(graph, x, vy) != identity);
	const struct pg_term *first = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *second = pg_lambda(graph, x, pg_lambda(graph, y, vy));
	assert(first != second);
	assert(!pg_alpha_equal(first, second));
	assert(pg_alpha_equal(first, pg_lambda(graph, y, pg_lambda(graph, z, vy))) == 1);
	assert(pg_lambda(graph, x, vz) != pg_lambda(graph, y, vz));
	assert(pg_alpha_equal(pg_lambda(graph, x, vz), pg_lambda(graph, y, vz)) == 1);
	assert(pg_lambda(graph, x, vz) != pg_lambda(graph, x, vy));
	const struct pg_term *app = pg_application(graph, identity, vx);
	assert(app == pg_application(graph, identity, vx));
	assert(app != pg_application(graph, vx, identity));
	struct pg_object *opaque = pg_alloc(graph, sizeof(*opaque));
	assert(opaque);
	opaque->kind = PG_SEMANTIC_OBJECT;
	assert(!pg_lambda(graph, opaque, vx));
	assert(pg_reference(graph, opaque) != vx);
	for (size_t i = 0; i < 4096; ++i) {
		const struct pg_object *fresh = pg_binder(graph);
		assert(fresh);
		assert(pg_reference(graph, fresh));
	}
	assert(graph->terms.capacity > 1024);
	assert(vx->as.reference == x);
	assert(identity == pg_lambda(graph, x, vx));
	assert(app == pg_application(graph, identity, vx));
	/* These DAGs contain 40 application nodes each, not 2^40 independent
	 * subterms. Hashing and alpha comparison must preserve that sharing. */
	const struct pg_term *left_dag = vx;
	const struct pg_term *right_dag = vy;
	for (size_t i = 0; i < 40; ++i) {
		left_dag = pg_application(graph, left_dag, left_dag);
		right_dag = pg_application(graph, right_dag, right_dag);
	}
	const struct pg_term *left_lambda = pg_lambda(graph, x, left_dag);
	const struct pg_term *right_lambda = pg_lambda(graph, y, right_dag);
	assert(left_lambda != right_lambda);
	assert(pg_alpha_equal(left_lambda, right_lambda) == 1);
	assert(pg_alpha_equal(left_dag, right_dag) == 0);
}

static const struct pg_dimension_map *maps[128];
static size_t map_count;

static void context_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	assert(pg_typing_init(&typing, graph) == 0);
	struct pg_object *type_a = pg_alloc(graph, sizeof(*type_a));
	struct pg_object *type_b = pg_alloc(graph, sizeof(*type_b));
	assert(type_a && type_b);
	type_a->kind = PG_SEMANTIC_OBJECT;
	type_b->kind = PG_SEMANTIC_OBJECT;
	const struct pg_term *a = pg_reference(graph, type_a);
	const struct pg_term *b = pg_reference(graph, type_b);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_context *in_a = pg_context_bind(&typing, NULL, x, a);
	const struct pg_context *in_b = pg_context_bind(&typing, NULL, x, b);
	assert(in_a && in_b);
	assert(in_a != in_b);
	assert(in_a == pg_context_bind(&typing, NULL, x, a));
	const struct pg_term *identity = pg_lambda(graph, x, pg_reference(graph, x));
	assert(identity == pg_lambda(graph, in_a->binder, pg_reference(graph, in_a->binder)));
	assert(identity == pg_lambda(graph, in_b->binder, pg_reference(graph, in_b->binder)));
	assert(pg_context_lookup(in_a, x)->declared_type == a);
	assert(pg_context_lookup(in_b, x)->declared_type == b);
	assert(!pg_context_lookup(in_a, y));
	const struct pg_context *extended = pg_context_bind(&typing, in_a, y, b);
	assert(extended->parent == in_a);
	assert(pg_context_lookup(extended, x) == in_a);
	assert(pg_context_lookup(extended, y) == extended);
	for (size_t i = 0; i < 1000; ++i) {
		assert(pg_context_bind(&typing, in_a, pg_binder(graph), a));
	}
	assert(in_a == pg_context_bind(&typing, NULL, x, a));
	assert(in_a->declared_type == a);
	assert(in_b->declared_type == b);
	assert(!pg_context_bind(&typing, NULL, type_a, a));
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_occurrence *body_a = pg_occurrence(&typing, in_a, vx, NULL, 0, NULL);
	const struct pg_occurrence *body_b = pg_occurrence(&typing, in_b, vx, NULL, 0, NULL);
	assert(body_a && body_b && body_a != body_b);
	assert(body_a->core == body_b->core);
	const struct pg_occurrence *lambda_a = pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a);
	const struct pg_occurrence *lambda_b = pg_occurrence(&typing, NULL, identity, NULL, 1, &body_b);
	assert(lambda_a && lambda_b && lambda_a != lambda_b);
	assert(lambda_a->core == lambda_b->core);
	assert(lambda_a == pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a));
	assert(lambda_a->operands[0]->context == in_a);
	assert(lambda_b->operands[0]->context == in_b);
	const struct pg_occurrence *annotated = pg_occurrence(&typing, in_a, vx, a, 0, NULL);
	assert(annotated && annotated != body_a);
	assert(annotated->annotation == a);
	assert(!pg_occurrence(&typing, NULL, NULL, NULL, 0, NULL));
	assert(!pg_occurrence(&typing, NULL, identity, NULL, 1, NULL));
	assert(!pg_occurrence(&typing, NULL, identity, NULL, SIZE_MAX, &body_a));
	for (size_t i = 0; i < 1000; ++i) {
		const struct pg_term *variable = pg_reference(graph, pg_binder(graph));
		assert(pg_occurrence(&typing, NULL, variable, NULL, 0, NULL));
	}
	assert(lambda_a == pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a));
	pg_typing_destroy(&typing);
	puts("typing inputs: persistent contexts and distinct occurrences over shared Core passed");
}

static void evaluation_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_object *a = pg_binder(graph);
	const struct pg_object *b = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *va = pg_reference(graph, a);
	const struct pg_term *vb = pg_reference(graph, b);
	const struct pg_term *constant = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *input = pg_application(graph, pg_application(graph, constant, va), vb);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *redex = pg_application(graph, identity, va);
	assert(redex != va);
	struct pg_eval whole, split;
	pg_eval_init(&whole, input);
	pg_eval_init(&split, input);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_advance(&split, 0) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&split, 2) == PG_EVAL_PENDING);
	const struct pg_term *residual = pg_eval_readback(&split, graph);
	assert(residual);
	while (pg_eval_advance(&split, 1) == PG_EVAL_PENDING) assert(split.steps < 100);
	assert(split.steps == whole.steps);
	assert(pg_eval_readback(&whole, graph) == va);
	assert(pg_eval_readback(&split, graph) == va);
	pg_eval_destroy(&whole);
	pg_eval_destroy(&split);
	pg_eval_init(&whole, redex);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&whole, graph) == va);
	assert(pg_application(graph, identity, va) == redex);
	assert(redex != va);
	pg_eval_destroy(&whole);
	pg_eval_init(&whole, residual);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&whole, graph) == va);
	pg_eval_destroy(&whole);
	/* The free y in the argument must not become bound during readback. */
	pg_eval_init(&whole, pg_application(graph, constant, vy));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	const struct pg_term *captured = pg_eval_readback(&whole, graph);
	assert(pg_alpha_equal(captured, pg_lambda(graph, a, vy)) == 1);
	assert(pg_alpha_equal(captured, pg_lambda(graph, y, vy)) == 0);
	pg_eval_destroy(&whole);
	/* One shared lambda is used under two distinct environments. */
	const struct pg_term *inner = pg_lambda(graph, y, vx);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *argument = i ? vb : va;
		pg_eval_init(&whole, pg_application(graph, pg_lambda(graph, x, inner), argument));
		assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
		assert(pg_alpha_equal(pg_eval_readback(&whole, graph), pg_lambda(graph, y, argument)) == 1);
		pg_eval_destroy(&whole);
	}
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	pg_eval_init(&whole, pg_application(graph, self, self));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(whole.steps == 200);
	pg_eval_destroy(&whole);
	const struct pg_term *shared = vx;
	const struct pg_term *expected = va;
	for (size_t i = 0; i < 40; ++i) {
		shared = pg_application(graph, shared, shared);
		expected = pg_application(graph, expected, expected);
	}
	const struct pg_term *suspended = pg_lambda(graph, x, pg_lambda(graph, y, shared));
	pg_eval_init(&whole, pg_application(graph, suspended, va));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_alpha_equal(pg_eval_readback(&whole, graph), pg_lambda(graph, y, expected)) == 1);
	pg_eval_destroy(&whole);
	puts("evaluation: lexical capture, shared closures, split budgets and divergence passed");
}

static void enumerate(struct pg_dimensions *dimensions, size_t source,
	size_t target, struct pg_coordinate *coordinates, size_t position)
{
	if (position == target) {
		const struct pg_dimension_map *map = pg_dimension_map(dimensions, source, target, coordinates);
		if (map) {
			assert(map_count < sizeof(maps) / sizeof(*maps));
			maps[map_count++] = map;
		}
		return;
	}
	coordinates[position] = (struct pg_coordinate){PG_ENDPOINT_ZERO, 0};
	enumerate(dimensions, source, target, coordinates, position + 1);
	coordinates[position] = (struct pg_coordinate){PG_ENDPOINT_ONE, 0};
	enumerate(dimensions, source, target, coordinates, position + 1);
	for (size_t i = 0; i < source; ++i) {
		coordinates[position] = (struct pg_coordinate){PG_AXIS, i};
		enumerate(dimensions, source, target, coordinates, position + 1);
	}
}

static void dimension_test(struct pg_graph *graph)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	struct pg_coordinate coordinates[3];
	for (size_t source = 0; source <= 2; ++source) {
		for (size_t target = 0; target <= 2; ++target) {
			enumerate(&dimensions, source, target, coordinates, 0);
		}
	}
	size_t triples = 0;
	for (size_t i = 0; i < map_count; ++i) {
		const struct pg_dimension_map *a = maps[i];
		assert(pg_dimension_compose(&dimensions, pg_dimension_identity(&dimensions, a->target), a) == a);
		assert(pg_dimension_compose(&dimensions, a, pg_dimension_identity(&dimensions, a->source)) == a);
		for (size_t j = 0; j < map_count; ++j) {
			const struct pg_dimension_map *b = maps[j];
			if (a->source != b->target) continue;
			const struct pg_dimension_map *ab = pg_dimension_compose(&dimensions, a, b);
			assert(ab);
			for (size_t k = 0; k < map_count; ++k) {
				const struct pg_dimension_map *c = maps[k];
				if (b->source != c->target) continue;
				const struct pg_dimension_map *bc = pg_dimension_compose(&dimensions, b, c);
				assert(bc);
				assert(pg_dimension_compose(&dimensions, ab, c) == pg_dimension_compose(&dimensions, a, bc));
				triples++;
			}
		}
	}
	struct pg_coordinate duplicate[] = {{PG_AXIS, 0}, {PG_AXIS, 0}};
	assert(!pg_dimension_map(&dimensions, 1, 2, duplicate));
	struct pg_coordinate invalid = {PG_AXIS, 1};
	assert(!pg_dimension_map(&dimensions, 1, 1, &invalid));
	invalid = (struct pg_coordinate){PG_ENDPOINT_ZERO, 1};
	assert(!pg_dimension_map(&dimensions, 0, 1, &invalid));
	struct pg_coordinate permutation[] = {{PG_AXIS, 2}, {PG_AXIS, 0}, {PG_AXIS, 1}};
	const struct pg_dimension_map *cycle = pg_dimension_map(&dimensions, 3, 3, permutation);
	const struct pg_dimension_map *twice = pg_dimension_compose(&dimensions, cycle, cycle);
	assert(pg_dimension_compose(&dimensions, cycle, twice) == pg_dimension_identity(&dimensions, 3));
	struct pg_coordinate face_coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ONE, 0}, {PG_AXIS, 1}};
	struct pg_coordinate projection_coordinates[] = {{PG_AXIS, 0}, {PG_AXIS, 2}};
	const struct pg_dimension_map *face = pg_dimension_map(&dimensions, 2, 3, face_coordinates);
	const struct pg_dimension_map *projection = pg_dimension_map(&dimensions, 3, 2, projection_coordinates);
	assert(pg_dimension_compose(&dimensions, projection, face) == pg_dimension_identity(&dimensions, 2));
	assert(!pg_dimension_compose(&dimensions, face, face));
	/* Restricting a cube along equal composites names the same variable. */
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 3);
	const struct pg_binding_face *top = pg_binding_face(&dimensions, cube,
		pg_dimension_identity(&dimensions, 3));
	assert(top);
	const struct pg_binding_face *surface = pg_binding_restrict(&dimensions, top, face);
	assert(surface);
	struct pg_coordinate edge_coordinates[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	const struct pg_dimension_map *edge = pg_dimension_map(&dimensions, 1, 2, edge_coordinates);
	const struct pg_binding_face *line = pg_binding_restrict(&dimensions, surface, edge);
	assert(line == pg_binding_restrict(&dimensions, top, pg_dimension_compose(&dimensions, face, edge)));
	const struct pg_term *line_term = pg_reference(graph, &line->variable);
	assert(line_term == pg_reference(graph, &pg_binding_face(&dimensions, cube, line->face)->variable));
	const struct pg_binding_cube *other_cube = pg_binding_cube(&dimensions, 3);
	assert(pg_binding_face(&dimensions, other_cube, line->face) != line);
	const struct pg_binding_cube *square = pg_binding_cube(&dimensions, 2);
	struct pg_coordinate down[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	struct pg_coordinate left[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
	struct pg_coordinate endpoint = {PG_ENDPOINT_ZERO, 0};
	const struct pg_dimension_map *zero = pg_dimension_map(&dimensions, 0, 1, &endpoint);
	const struct pg_binding_face *bottom = pg_binding_face(&dimensions, square,
		pg_dimension_map(&dimensions, 1, 2, down));
	const struct pg_binding_face *side = pg_binding_face(&dimensions, square,
		pg_dimension_map(&dimensions, 1, 2, left));
	assert(pg_binding_restrict(&dimensions, bottom, zero) == pg_binding_restrict(&dimensions, side, zero));
	/* Dropping a source dimension cannot introduce an independent binder. */
	assert(!pg_binding_face(&dimensions, square, projection));
	assert(!pg_binding_restrict(&dimensions, top, edge));
	assert(pg_binding_restrict(&dimensions, top, pg_dimension_identity(&dimensions, 3)) == top);
	for (size_t n = 4; n < 150; ++n) assert(pg_dimension_identity(&dimensions, n));
	assert(dimensions.maps.capacity > 64);
	assert(cycle == pg_dimension_map(&dimensions, 3, 3, permutation));
	printf("dimension: %zu maps, %zu composable triples; 3D faces/permutations passed\n", map_count, triples);
	pg_dimensions_destroy(&dimensions);
}

int main(void)
{
	struct pg_graph graph;
	assert(pg_graph_init(&graph) == 0);
	graph_test(&graph);
	context_test(&graph);
	evaluation_test(&graph);
	dimension_test(&graph);
	printf("graph: %zu terms; pointer-key interning and separate alpha comparison passed\n", graph.terms.count);
	pg_graph_destroy(&graph);
	return 0;
}
