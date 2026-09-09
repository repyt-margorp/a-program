#include "eval_io.h"
#include "wire.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_fixture(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_object *z = pg_binder(&graph), *w = pg_binder(&graph);
	const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y);
	struct pg_environment base = {x, {vy, NULL}, NULL};
	struct pg_environment inner = {y, {pg_lambda(&graph, z, vx), &base}, &base};
	struct pg_environment outer = {x, {vy, &inner}, &base};
	struct pg_argument tail = {{vx, &base}, NULL};
	struct pg_argument head = {{vy, &inner}, &tail};
	struct pg_eval_configuration roots[] = {
		{{vx, &outer}, NULL},
		{{pg_lambda(&graph, w, pg_lambda(&graph, z, pg_reference(&graph, z))), &inner}, &head},
		{{pg_lambda(&graph, w, pg_lambda(&graph, z, pg_reference(&graph, z))), &inner}, &head},
		{{pg_lambda(&graph, y, vx), &base}, NULL},
		{{vy, NULL}, NULL}
	};
	size_t terms = graph.terms.count;
	assert(!pg_eval_configurations_write(file, 5, roots, NULL, NULL));
	assert(graph.terms.count == terms);
	pg_graph_destroy(&graph);
}

static const struct pg_term *evaluate(struct pg_graph *graph, struct pg_eval_configuration configuration)
{
	struct pg_eval machine;
	pg_eval_init(&machine, configuration.head.term);
	machine.current = configuration.head;
	machine.arguments = configuration.arguments;
	assert(machine.steps == 0);
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	pg_eval_destroy(&machine);
	assert(result);
	return result;
}

static void read_fixture(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(!pg_eval_configurations_read(file, &graph, 1000, 100, NULL, NULL, &count, &roots));
	assert(count == 5);
	assert(roots[0].head.environment->value.environment == roots[1].head.environment);
	assert(roots[0].head.environment->parent == roots[3].head.environment);
	assert(roots[1].head.environment->parent == roots[3].head.environment);
	assert(roots[1].arguments == roots[2].arguments && roots[1].head.term == roots[2].head.term);
	assert(roots[1].arguments->value.environment == roots[1].head.environment);
	assert(roots[1].arguments->next->value.environment == roots[3].head.environment);
	assert(!roots[1].arguments->next->next);
	assert(roots[3].head.term->as.lambda.binder == roots[4].head.term->as.reference);
	/* An unsolved resave must not allocate into or evaluate the restored graph. */
	FILE *saved = tmpfile();
	assert(saved);
	size_t terms = graph.terms.count;
	assert(!pg_eval_configurations_write(saved, count, roots, NULL, NULL));
	assert(graph.terms.count == terms);
	rewind(file);
	rewind(saved);
	int byte;
	do { byte = fgetc(file); assert(byte == fgetc(saved)); } while (byte != EOF);
	assert(!ferror(file) && !ferror(saved));
	assert(!fclose(saved));
	const struct pg_term *expected = roots[4].head.term;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_term *result = evaluate(&graph, roots[i]);
		if (i == 0 || i == 3) {
			assert(result->kind == PG_LAMBDA && result->as.lambda.body == expected);
			assert(result->as.lambda.binder != expected->as.reference);
		} else assert(result == expected);
	}
	pg_graph_destroy(&graph);
}

static void deep_shared(void)
{
	struct pg_graph graph, restored;
	assert(!pg_graph_init(&graph) && !pg_graph_init(&restored));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_term *value = pg_reference(&graph, y);
	const size_t depth = 10000;
	struct pg_environment *chain = pg_alloc(&graph, depth * sizeof(*chain));
	assert(chain);
	for (size_t i = 0; i < depth; ++i) {
		const struct pg_environment *parent = i ? &chain[i - 1] : NULL;
		chain[i] = (struct pg_environment){x, {value, parent}, parent};
	}
	struct pg_eval_configuration input = {{value, &chain[depth - 1]}, NULL};
	FILE *file = tmpfile();
	assert(file && !pg_eval_configurations_write(file, 1, &input, NULL, NULL));
	assert(ftell(file) < 1000000);
	rewind(file);
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(!pg_eval_configurations_read(file, &restored, 100000, 100, NULL, NULL, &count, &roots));
	assert(count == 1);
	const struct pg_environment *environment = roots[0].head.environment;
	for (size_t i = 0; i < depth; ++i) {
		assert(environment && environment->value.environment == environment->parent);
		environment = environment->parent;
	}
	assert(!environment);
	pg_graph_destroy(&graph);
	assert(evaluate(&restored, roots[0]) == roots[0].head.term);
	pg_graph_destroy(&restored);
	assert(!fclose(file));
}

static void beta_resume(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_object *z = pg_binder(&graph), *w = pg_binder(&graph);
	const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y);
	const struct pg_term *identity = pg_lambda(&graph, w, pg_reference(&graph, w));
	const struct pg_term *body = pg_lambda(&graph, z, pg_application(&graph, vx, vy));
	const struct pg_term *input = pg_application(&graph, pg_lambda(&graph, x,
		pg_application(&graph, pg_lambda(&graph, y, body), vx)), identity);
	struct pg_eval whole;
	pg_eval_init(&whole, input);
	assert(pg_eval_advance(&whole, 1000) == PG_EVAL_WHNF);
	uint64_t steps = whole.steps;
	const struct pg_term *expected = pg_eval_readback(&whole, &graph);
	pg_eval_destroy(&whole);
	for (uint64_t cut = 0; cut < steps; ++cut) {
		struct pg_eval machine;
		pg_eval_init(&machine, input);
		assert(pg_eval_advance(&machine, cut) == PG_EVAL_PENDING);
		assert(!machine.frames && !machine.task);
		struct pg_eval_configuration state = {machine.current, machine.arguments};
		FILE *file = tmpfile();
		assert(file && !pg_eval_configurations_write(file, 1, &state, NULL, NULL));
		pg_eval_destroy(&machine);
		struct pg_graph restored;
		assert(!pg_graph_init(&restored));
		rewind(file);
		size_t count;
		const struct pg_eval_configuration *roots;
		assert(!pg_eval_configurations_read(file, &restored, 1000, 100, NULL, NULL, &count, &roots));
		assert(count == 1);
		pg_eval_init(&machine, roots[0].head.term);
		machine.current = roots[0].head;
		machine.arguments = roots[0].arguments;
		assert(pg_eval_advance(&machine, steps - cut) == PG_EVAL_WHNF);
		assert(machine.steps == steps - cut);
		assert(pg_alpha_equal(pg_eval_readback(&machine, &restored), expected) == 1);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&restored);
		assert(!fclose(file));
	}
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	if (argc == 3) {
		int writing = !strcmp(argv[1], "write");
		assert(writing || !strcmp(argv[1], "read"));
		FILE *file = fopen(argv[2], writing ? "wb" : "rb");
		assert(file);
		if (writing) write_fixture(file);
		else read_fixture(file);
		assert(!fclose(file));
		return 0;
	}
	assert(argc == 1);
	FILE *file = tmpfile();
	assert(file);
	write_fixture(file);
	rewind(file);
	read_fixture(file);
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	size_t count = 99;
	const struct pg_eval_configuration *roots = (const void *)&graph;
	rewind(file);
	assert(pg_eval_configurations_read(file, &graph, 0, 100, NULL, NULL, &count, &roots));
	assert(!count && !roots);
	/* Environment edges and argument tails must precede their parent record;
	 * cross-table and root references must stay within their respective tables. */
	const long offsets[] = {32, 40, 80, 88, 112, 120};
	const uint64_t invalid[] = {1, 1, 1, 4, 4, 3};
	for (size_t i = 0; i < sizeof(offsets) / sizeof(*offsets); ++i) {
		uint64_t previous;
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_read_u64(file, &previous));
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, invalid[i]));
		rewind(file);
		assert(pg_eval_configurations_read(file, &graph, 1000, 100, NULL, NULL, &count, &roots));
		assert(!count && !roots);
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, previous));
	}
	assert(!fclose(file));
	const struct pg_object *binder = pg_binder(&graph);
	struct pg_environment cycle = {binder, {pg_reference(&graph, binder), NULL}, NULL};
	cycle.parent = &cycle;
	struct pg_eval_configuration cyclic = {{cycle.value.term, &cycle}, NULL};
	file = tmpfile();
	assert(file && pg_eval_configurations_write(file, 1, &cyclic, NULL, NULL));
	assert(!fclose(file));
	file = tmpfile();
	assert(file && !pg_eval_configurations_write(file, 0, NULL, NULL, NULL));
	rewind(file);
	assert(!pg_eval_configurations_read(file, &graph, 100, 100, NULL, NULL, &count, &roots));
	assert(!count);
	assert(!fclose(file));
	pg_graph_destroy(&graph);
	deep_shared();
	beta_resume();
	puts("evaluation configuration: captured environments, shared tails, inert resave and lexical relocation passed");
	return 0;
}
