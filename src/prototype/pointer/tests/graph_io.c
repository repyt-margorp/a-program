#include "graph_io.h"
#include "eval.h"
#include "wire.h"

#include <assert.h>
#include <string.h>

static const struct pg_object_class owner = {"test-descriptor"};
static const struct pg_object oracle = {PG_SEMANTIC_OBJECT, &owner};
static const struct pg_object second_oracle = {PG_SEMANTIC_OBJECT, &owner};
static const struct pg_object owned_binder = {PG_BINDER, &owner};

static const char *name(void *context, const struct pg_object *object)
{
	(void)context;
	if (object == &oracle) return "test/oracle/v1";
	if (object == &second_oracle) return "test/second-oracle/v1";
	if (object == &owned_binder) return "test/binder/v1";
	return NULL;
}

static const struct pg_object *resolve(void *context, const char *label)
{
	(void)context;
	if (!strcmp(label, "test/oracle/v1")) return &oracle;
	if (!strcmp(label, "test/second-oracle/v1")) return &second_oracle;
	if (!strcmp(label, "test/binder/v1")) return &owned_binder;
	return NULL;
}

static const struct pg_object *wrong_kind(void *context, const char *label)
{
	(void)context;
	(void)label;
	return &oracle;
}

int main(void)
{
	struct pg_graph source, destination;
	assert(pg_graph_init(&source) == 0 && pg_graph_init(&destination) == 0);
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
	assert(file && fwrite("APGCORE\0", 1, 8, file) == 8);
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
