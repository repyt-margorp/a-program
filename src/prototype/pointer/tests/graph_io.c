#include "graph_io.h"
#include "eval.h"
#include "wire.h"
#include "symmetry.h"
#include "dag.h"

#include <assert.h>
#include <string.h>

static const struct pg_object_class owner = {"test-descriptor"};
static const struct pg_object oracle = {PG_SEMANTIC_OBJECT, &owner};
static const struct pg_object second_oracle = {PG_SEMANTIC_OBJECT, &owner};
static const struct pg_object owned_binder = {PG_BINDER, &owner};

struct payload_dependency {
	const struct pg_term *term;
	size_t calls;
};

static int payload_child(void *context, struct pg_graph *scratch,
	const struct pg_object *object, size_t index, const struct pg_term **child)
{
	(void)scratch;
	if (object != &oracle) return -2;
	struct payload_dependency *payload = context;
	++payload->calls;
	if (index) return 0;
	*child = payload->term;
	return 1;
}

static const char *name(void *context, const struct pg_object *object)
{
	(void)context;
	if (object == &oracle) return "test/oracle/v1";
	if (object == &second_oracle) return "test/second-oracle/v1";
	if (object == &owned_binder) return "test/binder/v1";
	static char buffer[128];
	return pg_symmetry_name(object, buffer, sizeof(buffer));
}

static const struct pg_object *resolve(void *context, const char *label)
{
	(void)context;
	if (!strcmp(label, "test/oracle/v1")) return &oracle;
	if (!strcmp(label, "test/second-oracle/v1")) return &second_oracle;
	if (!strcmp(label, "test/binder/v1")) return &owned_binder;
	return pg_symmetry_resolve(context, label);
}

static void symmetry_transport(struct pg_graph *source, struct pg_graph *destination)
{
	const char *names[] = {"kernel/symmetry/v1/", "kernel/symmetry/v1/0",
		"kernel/symmetry/v1/0,1,2", "kernel/symmetry/v1/0,2,1",
		"kernel/symmetry/v1/1,0,2", "kernel/symmetry/v1/1,2,0",
		"kernel/symmetry/v1/2,0,1", "kernel/symmetry/v1/2,1,0"};
	const struct pg_term *input[8];
	const struct pg_term *argument = pg_reference(source, pg_binder(source));
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_object *object = pg_symmetry_resolve(source, names[i]);
		assert(object && object == pg_symmetry_resolve(source, names[i]));
		input[i] = pg_application(source, pg_reference(source, object), argument);
	}
	FILE *file = tmpfile();
	assert(file && !pg_graph_write(file, 8, input, name, NULL));
	rewind(file);
	size_t count;
	const struct pg_term *const *roots;
	assert(!pg_graph_read(file, destination, 100, 128, resolve, destination, &count, &roots));
	assert(count == 8);
	const struct pg_term *loaded_argument = NULL;
	for (size_t i = 0; i < count; ++i) {
		size_t dimension;
		const size_t *axes;
		const struct pg_term *term;
		assert(pg_symmetry_view(roots[i], &dimension, &axes, &term));
		if (!i) loaded_argument = term;
		assert(term == loaded_argument && term != argument);
		assert(dimension == (i < 2 ? i : 3));
		char buffer[128];
		const struct pg_object *object = roots[i]->as.application.function->as.reference;
		assert(!strcmp(pg_symmetry_name(object, buffer, sizeof(buffer)), names[i]));
		assert(object != input[i]->as.application.function->as.reference);
		struct pg_coordinate coordinates[3];
		for (size_t j = 0; j < dimension; ++j)
			coordinates[j] = (struct pg_coordinate){PG_AXIS, axes[j]};
		struct pg_dimension_map map = {.source = dimension, .target = dimension, .coordinates = coordinates};
		assert(pg_symmetry(destination, &map, term) == roots[i]);
		assert(!pg_symmetry_name(object, buffer, 2));
	}
	assert(roots[0] != loaded_argument && roots[1] != roots[0] && roots[2] != roots[1]);
	/* Identity erasure is evaluation, never descriptor relocation. */
	struct pg_eval evaluation;
	pg_eval_init(&evaluation, roots[2]);
	evaluation.dispatch = pg_symmetry_dispatch;
	assert(pg_eval_advance(&evaluation, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&evaluation, destination) == loaded_argument);
	pg_eval_destroy(&evaluation);
	const char *invalid[] = {"kernel/symmetry/v2/0", "kernel/symmetry/v1/00",
		"kernel/symmetry/v1/0,0", "kernel/symmetry/v1/2,0", "kernel/symmetry/v1/0,",
		"kernel/symmetry/v1/,0", "kernel/symmetry/v1/-1", "kernel/symmetry/v1/+0",
		"kernel/symmetry/v1/ 0", "kernel/symmetry/v1/0/x",
		"kernel/symmetry/v1/184467440737095516160"};
	size_t before = destination->objects.count;
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
		assert(!pg_symmetry_resolve(destination, invalid[i]));
	assert(destination->objects.count == before);
	assert(!fclose(file));
}

static const struct pg_object *wrong_kind(void *context, const char *label)
{
	(void)context;
	(void)label;
	return &oracle;
}

struct image_payload {
	const struct pg_term *term;
	const struct pg_term *first, *second;
	int stop;
};

static int image_write(FILE *file, const struct pg_graph_codec *codec, void *state)
{
	struct image_payload *payload = state;
	if (pg_graph_write_descriptors(file, 1, &payload->term, codec, NULL)) return -1;
	if (pg_wire_write_u64(file, 42)) return -1;
	struct pg_graph scratch;
	if (pg_graph_init(&scratch)) return -1;
	const struct pg_term *copy = pg_reference(&scratch, payload->term->as.reference);
	int status = !copy ? -1 : pg_graph_write_descriptors(file, 1, &copy, codec, NULL);
	pg_graph_destroy(&scratch);
	return status;
}

static int image_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *state)
{
	struct image_payload *payload = state;
	size_t count;
	const struct pg_term *const *roots;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, NULL, &count, &roots) || count != 1) return -1;
	payload->first = roots[0];
	if (payload->stop) return 0;
	uint64_t marker;
	if (pg_wire_read_u64(file, &marker) || marker != 42) return -1;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, NULL, &count, &roots) || count != 1) return -1;
	payload->second = roots[0];
	return 0;
}

static void shared_image(void)
{
	struct pg_graph source, destination;
	assert(!pg_graph_init(&source) && !pg_graph_init(&destination));
	struct image_payload payload = {.term = pg_reference(&source, pg_binder(&source))};
	FILE *file = tmpfile();
	const char version[8] = "APGTST\1";
	assert(file && !pg_graph_image_write(file, version, NULL, NULL, image_write, &payload));
	pg_graph_destroy(&source);
	rewind(file);
	assert(!pg_graph_image_read(file, version, &destination, 100, 100, NULL, NULL, image_read, &payload));
	assert(payload.first == payload.second);
	/* The owner's success does not bypass the complete-payload boundary. */
	payload.stop = 1;
	rewind(file);
	assert(pg_graph_image_read(file, version, &destination, 100, 100, NULL, NULL, image_read, &payload));
	assert(!fclose(file));
	pg_graph_destroy(&destination);
}

int main(void)
{
	shared_image();
	struct pg_graph source, destination;
	assert(pg_graph_init(&source) == 0 && pg_graph_init(&destination) == 0);
	symmetry_transport(&source, &destination);
	FILE *collision = tmpfile();
	const struct pg_term *separate[] = {pg_reference(&source, &oracle), pg_reference(&source, &second_oracle)};
	assert(collision && !pg_graph_write(collision, 2, separate, name, NULL));
	rewind(collision);
	size_t separate_count = 0;
	const struct pg_term *const *separate_roots = NULL;
	assert(!pg_graph_read(collision, &destination, 100, 100, resolve, NULL, &separate_count, &separate_roots));
	assert(separate_count == 2 && separate_roots[0] != separate_roots[1]);
	const struct pg_term *const *unchanged_roots = separate_roots;
	rewind(collision);
	assert(pg_graph_read(collision, &destination, 100, 100, wrong_kind, NULL, &separate_count, &separate_roots) == -1);
	assert(separate_count == 2 && separate_roots == unchanged_roots);
	assert(!fclose(collision));
	const struct pg_object *x = pg_binder(&source), *y = pg_binder(&source);
	const struct pg_term *vx = pg_reference(&source, x), *vy = pg_reference(&source, y);
	const struct pg_term *id = pg_lambda(&source, x, vx);
	const struct pg_term *other_id = pg_lambda(&source, y, vy);
	const struct pg_term *redex = pg_application(&source, id, vy);
	const struct pg_term *dag = vx;
	for (size_t i = 0; i < 40; ++i) dag = pg_application(&source, dag, dag);
	FILE *listing = tmpfile();
	size_t before_print = source.terms.count;
	assert(listing && !pg_graph_print(listing, dag));
	assert(source.terms.count == before_print);
	rewind(listing);
	char line[256];
	size_t lines = 0;
	while (fgets(line, sizeof(line), listing)) ++lines;
	assert(lines == 43 && !strcmp(line, "root := n41\n"));
	assert(!pg_graph_print(listing, redex));
	assert(source.terms.count == before_print);
	assert(pg_graph_print(NULL, dag) == -1 && pg_graph_print(listing, NULL) == -1);
	assert(!fclose(listing));
	const struct pg_term *input[] = {id, other_id, id, redex, dag,
		pg_reference(&source, &oracle), pg_lambda(&source, &owned_binder, pg_reference(&source, &owned_binder))};
	FILE *file = tmpfile();
	assert(file && pg_graph_write(file, 7, input, name, NULL) == 0);
	rewind(file);
	unsigned char bytes[4096];
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file));
	rewind(file);
	size_t count = 0;
	const struct pg_term *const *roots = NULL;
	assert(pg_graph_read(file, &destination, 100, 100, resolve, NULL, &count, &roots) == 0);
	assert(count == 7 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(pg_alpha_equal(roots[0], roots[1]) == 1);
	assert(roots[0]->as.lambda.binder != x);
	assert(roots[3]->kind == PG_APPLICATION && roots[3]->as.application.function == roots[0]);
	const struct pg_term *cursor = roots[4];
	for (size_t i = 0; i < 40; ++i) {
		assert(cursor->kind == PG_APPLICATION);
		assert(cursor->as.application.function == cursor->as.application.argument);
		cursor = cursor->as.application.argument;
	}
	assert(cursor == roots[0]->as.lambda.body);
	assert(roots[5]->as.reference == &oracle);
	assert(roots[6]->as.lambda.binder == &owned_binder);
	/* Evaluation is a separate request; the loaded redex is not interned away. */
	struct pg_eval evaluation;
	pg_eval_init(&evaluation, roots[3]);
	assert(pg_eval_advance(&evaluation, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&evaluation, &destination) == roots[1]->as.lambda.body);
	pg_eval_destroy(&evaluation);
	rewind(file);
	assert(pg_graph_read(file, &destination, 100, 100, NULL, NULL, &count, &roots) == -1);
	assert(count == 7);
	rewind(file);
	assert(pg_graph_read(file, &destination, 0, 100, resolve, NULL, &count, &roots) == -1);
	rewind(file);
	assert(pg_graph_read(file, &destination, 100, 0, resolve, NULL, &count, &roots) == -1);
	rewind(file);
	assert(pg_graph_read(file, &destination, 100, 100, wrong_kind, NULL, &count, &roots) == -1);
	assert(fseek(file, 0, SEEK_END) == 0 && fputc(0, file) != EOF);
	rewind(file);
	assert(pg_graph_read(file, &destination, 100, 100, resolve, NULL, &count, &roots) == -1);
	assert(count == 7);
	assert(fclose(file) == 0);
	for (size_t cut = 0; cut < length; ++cut) {
		struct pg_graph truncated;
		assert(pg_graph_init(&truncated) == 0);
		FILE *fragment = tmpfile();
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		const struct pg_term *const *unchanged = roots;
		assert(pg_graph_read(fragment, &truncated, 100, 100, resolve, NULL, &count, &roots) == -1);
		assert(count == 7 && roots == unchanged);
		assert(fclose(fragment) == 0);
		pg_graph_destroy(&truncated);
	}
	file = tmpfile();
	assert(file && pg_graph_write(file, 7, input, NULL, NULL) == -1);
	assert(fclose(file) == 0);
	/* A malformed cyclic Term is not a recursive declaration descriptor. */
	struct pg_term cycle = {.kind = PG_APPLICATION};
	cycle.as.application.function = &cycle;
	cycle.as.application.argument = vx;
	const struct pg_term *cyclic = &cycle;
	file = tmpfile();
	assert(file && pg_graph_write(file, 1, &cyclic, name, NULL) == -1);
	assert(fclose(file) == 0);
	file = tmpfile();
	assert(file && fwrite("APGCORE\1", 1, 8, file) == 8);
	assert(!pg_wire_write_u64(file, 0) && !pg_wire_write_u64(file, 1) && !pg_wire_write_u64(file, 1));
	assert(fputc(PG_APPLICATION, file) != EOF);
	assert(!pg_wire_write_u64(file, 1) && !pg_wire_write_u64(file, 1) && !pg_wire_write_u64(file, 1));
	rewind(file);
	assert(pg_graph_read(file, &destination, 100, 100, resolve, NULL, &count, &roots) == -1);
	assert(count == 7 && fclose(file) == 0);
	/* Collection and relocation do not recurse through the C call stack. */
	const struct pg_term *deep = vx;
	for (size_t i = 0; i < 10000; ++i) deep = pg_application(&source, deep, deep);
	deep = pg_lambda(&source, x, deep);
	struct pg_dag reachable = {0};
	assert(!pg_dag_init(&reachable, NULL, NULL));
	size_t allocated = source.terms.count;
	assert(!pg_graph_collect_objects(&reachable, 1, &deep, NULL, NULL));
	assert(reachable.count == 1 && pg_dag_find(&reachable, x));
	assert(!pg_graph_collect_objects(&reachable, 1, &deep, NULL, NULL));
	assert(reachable.count == 1 && source.terms.count == allocated);
	const struct pg_term *opaque = pg_reference(&source, &oracle);
	const struct pg_graph_codec codec = {.child = payload_child};
	struct payload_dependency payload = {.term = deep};
	assert(!pg_graph_collect_objects(&reachable, 1, &opaque, &codec, &payload));
	assert(reachable.count == 2 && pg_dag_find(&reachable, &oracle));
	assert(!pg_dag_find(&reachable, &second_oracle));
	struct pg_dag dependencies = {0};
	assert(!pg_graph_dependencies_init(&dependencies, &reachable, &codec, &payload));
	assert(!pg_dag_add(&dependencies, deep) && !pg_dag_add(&dependencies, opaque));
	size_t visited = dependencies.count, calls = payload.calls;
	assert(!pg_dag_add(&dependencies, opaque) && !pg_dag_add(&dependencies, deep));
	assert(dependencies.count == visited && payload.calls == calls);
	pg_dag_destroy(&dependencies);
	assert(pg_graph_collect_objects(&reachable, 1, &cyclic, NULL, NULL) == -1);
	pg_dag_destroy(&reachable);
	file = tmpfile();
	assert(file && pg_graph_write(file, 1, &deep, NULL, NULL) == 0);
	rewind(file);
	assert(pg_graph_read(file, &destination, 20000, 0, NULL, NULL, &count, &roots) == 0);
	assert(count == 1 && pg_alpha_equal(deep, roots[0]) == 1);
	assert(fclose(file) == 0);
	pg_graph_destroy(&destination);
	pg_graph_destroy(&source);
	puts("graph io: exact sharing, binder relocation, descriptor resolution and no implicit reduction passed");
	return 0;
}
