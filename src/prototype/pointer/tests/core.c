#include "graph.h"
#include "dimension.h"
#include "eval.h"

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
	assert(identity == pg_lambda(graph, y, vy));
	assert(vx != vy);
	assert(pg_lambda(graph, x, vy) != identity);
	const struct pg_term *first = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *second = pg_lambda(graph, x, pg_lambda(graph, y, vy));
	assert(first != second);
	assert(!pg_alpha_equal(first, second));
	assert(first == pg_lambda(graph, y, pg_lambda(graph, z, vy)));
	assert(pg_lambda(graph, x, vz) == pg_lambda(graph, y, vz));
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
	assert(identity == pg_lambda(graph, z, vz));
	assert(app == pg_application(graph, identity, vx));
}

static const struct pg_dimension_map *maps[128];
static size_t map_count;

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
	pg_eval_init(&whole, residual);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&whole, graph) == va);
	pg_eval_destroy(&whole);
	/* The free y in the argument must not become bound during readback. */
	pg_eval_init(&whole, pg_application(graph, constant, vy));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	const struct pg_term *captured = pg_eval_readback(&whole, graph);
	assert(captured == pg_lambda(graph, a, vy));
	assert(captured != pg_lambda(graph, y, vy));
	pg_eval_destroy(&whole);
	/* One shared lambda is used under two distinct environments. */
	const struct pg_term *inner = pg_lambda(graph, y, vx);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *argument = i ? vb : va;
		pg_eval_init(&whole, pg_application(graph, pg_lambda(graph, x, inner), argument));
		assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&whole, graph) == pg_lambda(graph, y, argument));
		pg_eval_destroy(&whole);
	}
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	pg_eval_init(&whole, pg_application(graph, self, self));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(whole.steps == 200);
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
	evaluation_test(&graph);
	dimension_test(&graph);
	printf("graph: %zu terms; alpha sharing and stable pointers passed\n", graph.terms.count);
	pg_graph_destroy(&graph);
	return 0;
}
