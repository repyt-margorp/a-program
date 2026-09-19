#include "eval_io.h"
#include "eval_internal.h"
#include "graph_internal.h"
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

static const struct pg_term *substitution_fixture(struct pg_graph *graph, struct pg_binding_value *binding)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *body = pg_reference(graph, x);
	for (size_t i = 0; i < 12; ++i) body = pg_application(graph, body, body);
	*binding = (struct pg_binding_value){x, pg_reference(graph, y)};
	return pg_lambda(graph, y, body);
}

static void write_substitution(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_binding_value binding;
	const struct pg_term *input = substitution_fixture(&graph, &binding);
	struct pg_substitution work;
	assert(!pg_substitution_init(&work, &graph, input, 1, &binding));
	assert(pg_substitution_advance(&work, 1) == PG_SUBSTITUTION_PENDING);
	assert(!pg_substitution_write(file, &work, NULL, NULL));
	pg_substitution_destroy(&work);
	pg_graph_destroy(&graph);
}

static void read_substitution(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_substitution work, baseline;
	assert(!pg_substitution_read(file, &graph, 10000, 100, NULL, NULL, &work));
	assert(pg_substitution_steps(&work) == 1 && pg_substitution_status(&work) == PG_SUBSTITUTION_PENDING);
	const struct pg_closure *source = pg_substitution_input(&work);
	assert(source && source->environment && !source->environment->parent);
	struct pg_binding_value binding = {source->environment->binder, source->environment->value.term};
	assert(!pg_substitution_init(&baseline, &graph, source->term, 1, &binding));
	assert(pg_substitution_advance(&baseline, 10000) == PG_SUBSTITUTION_DONE);
	uint64_t steps = pg_substitution_steps(&baseline);
	assert(pg_substitution_advance(&work, steps - 1) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_steps(&work) == steps);
	assert(pg_alpha_equal(pg_substitution_result(&work), pg_substitution_result(&baseline)) == 1);
	pg_substitution_destroy(&work);
	pg_substitution_destroy(&baseline);
	pg_graph_destroy(&graph);
}

static void substitution_resume(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_binding_value binding;
	const struct pg_term *input = substitution_fixture(&graph, &binding);
	const struct pg_object *y = binding.value->as.reference;
	struct pg_substitution whole;
	assert(!pg_substitution_init(&whole, &graph, input, 1, &binding));
	assert(pg_substitution_advance(&whole, 10000) == PG_SUBSTITUTION_DONE);
	uint64_t steps = pg_substitution_steps(&whole);
	const struct pg_term *expected = pg_substitution_result(&whole);
	pg_substitution_destroy(&whole);
	for (uint64_t cut = 0; cut <= steps; ++cut) {
		struct pg_substitution work;
		assert(!pg_substitution_init(&work, &graph, input, 1, &binding));
		pg_substitution_advance(&work, cut);
		assert(pg_substitution_steps(&work) == cut);
		FILE *file = tmpfile();
		assert(file && !pg_substitution_write(file, &work, NULL, NULL));
		pg_substitution_destroy(&work);
		struct pg_graph restored;
		assert(!pg_graph_init(&restored));
		rewind(file);
		assert(!pg_substitution_read(file, &restored, 10000, 100, NULL, NULL, &work));
		assert(pg_substitution_steps(&work) == cut);
		const struct pg_closure *source = pg_substitution_input(&work);
		assert(source && source->term->kind == PG_LAMBDA && source->environment);
		const struct pg_term *relocated_y = source->environment->value.term;
		assert(relocated_y->as.reference == source->term->as.lambda.binder);
		struct pg_binding_value correspondence = {y, relocated_y};
		const struct pg_term *relocated_expected = pg_term_substitute(&restored, expected, 1, &correspondence);
		FILE *again = tmpfile();
		assert(again && !pg_substitution_write(again, &work, NULL, NULL));
		rewind(file);
		rewind(again);
		int byte;
		do { byte = fgetc(file); assert(byte == fgetc(again)); } while (byte != EOF);
		assert(!ferror(file) && !ferror(again));
		assert(!fclose(again));
		assert(pg_substitution_advance(&work, steps - cut) == PG_SUBSTITUTION_DONE);
		assert(pg_substitution_steps(&work) == steps);
		assert(pg_alpha_equal(pg_substitution_result(&work), relocated_expected) == 1);
		pg_substitution_destroy(&work);
		if (cut == 1) {
			/* Missing parent, missing child and an unreachable retained node. */
			const long offsets[] = {88, 24, 16};
			const uint64_t invalid[] = {0, 2, 1};
			for (size_t i = 0; i < 3; ++i) {
				uint64_t prior;
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_read_u64(file, &prior));
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, invalid[i]));
				rewind(file);
				assert(pg_substitution_read(file, &restored, 10000, 100, NULL, NULL, &work));
				assert(!work.state);
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, prior));
			}
		}
		if (!cut) {
			assert(!fseek(file, 6, SEEK_SET) && fputc(1, file) != EOF);
			rewind(file);
			assert(pg_substitution_read(file, &restored, 10000, 100, NULL, NULL, &work));
			assert(!work.state);
			assert(!fseek(file, 6, SEEK_SET) && fputc(2, file) != EOF);
			const long offsets[] = {24, 48, 72, 88};
			const uint64_t invalid[] = {2, 3, 1, 1};
			for (size_t i = 0; i < 4; ++i) {
				uint64_t prior;
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_read_u64(file, &prior));
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, invalid[i]));
				rewind(file);
				assert(pg_substitution_read(file, &restored, 10000, 100, NULL, NULL, &work));
				assert(!work.state);
				assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, prior));
			}
		}
		pg_graph_destroy(&restored);
		assert(!fclose(file));
	}
	pg_graph_destroy(&graph);
}

static void identity_substitution_image(void)
{
	struct pg_graph graph;
	struct pg_substitution work;
	assert(!pg_graph_init(&graph));
	const struct pg_object *x = pg_binder(&graph);
	const struct pg_term *variable = pg_reference(&graph, x);
	const struct pg_term *input = pg_lambda(&graph, x, variable);
	struct pg_binding_value binding = {x, variable};
	assert(!pg_substitution_init(&work, &graph, input, 1, &binding));
	for (unsigned round = 0; round < 2; ++round) {
		assert(pg_substitution_status(&work) == PG_SUBSTITUTION_DONE);
		assert(!pg_substitution_steps(&work));
		assert(pg_substitution_result(&work) == pg_substitution_input(&work)->term);
		assert(!pg_substitution_input(&work)->environment);
		FILE *file = tmpfile();
		assert(file && !pg_substitution_write(file, &work, NULL, NULL));
		pg_substitution_destroy(&work);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		assert(!pg_substitution_read(file, &graph, 100, 100, NULL, NULL, &work));
		assert(!fclose(file));
	}
	assert(pg_substitution_result(&work) == pg_substitution_input(&work)->term);
	assert(pg_substitution_advance(&work, 100) == PG_SUBSTITUTION_DONE);
	assert(!pg_substitution_steps(&work));
	pg_substitution_destroy(&work);
	pg_graph_destroy(&graph);
}

static void shared_substitution_images(void)
{
	struct pg_graph graph;
	struct pg_substitution_work shared;
	struct pg_binding_value image;
	assert(!pg_graph_init(&graph));
	assert(!pg_substitution_work_init(&shared, &graph));
	const struct pg_term *input = substitution_fixture(&graph, &image);
	struct pg_substitution *request = pg_substitution_request(&shared, input, 1, &image);
	assert(request && pg_substitution_advance(request, 10000) == PG_SUBSTITUTION_DONE);
	uint64_t steps = pg_substitution_steps(request);
	pg_substitution_work_destroy(&shared);
	pg_graph_destroy(&graph);
	for (uint64_t cut = 0; cut <= steps; ++cut) {
		assert(!pg_graph_init(&graph));
		assert(!pg_substitution_work_init(&shared, &graph));
		input = substitution_fixture(&graph, &image);
		request = pg_substitution_request(&shared, input, 1, &image);
		assert(request);
		struct pg_substitution_state *state = request->state;
		assert(state->input_storage == &shared.storage);
		const struct pg_closure *original = pg_substitution_input(request);
		struct readback_entry *root = request->state->root;
		assert(request->state->context.pending == root && !request->state->context.temporary.blocks);
		pg_substitution_advance(request, cut);
		assert(request->state == state && state->input_storage == &shared.storage);
		assert(pg_substitution_input(request) == original && request->state->root == root);
		assert(pg_substitution_steps(request) == cut);
		assert(pg_substitution_request(&shared, input, 1, &image) == request);
		if (cut == steps) {
			assert(!request->state->context.temporary.blocks);
			assert(!request->state->context.results.count);
			assert(!root->left && !root->right && !root->next && !root->cursor);
		}
		FILE *file = tmpfile();
		assert(file && !pg_substitution_write(file, request, NULL, NULL));
		pg_substitution_work_destroy(&shared);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		struct pg_substitution restored, baseline;
		assert(!pg_substitution_read(file, &graph, 10000, 100, NULL, NULL, &restored));
		assert(!fclose(file));
		const struct pg_closure *source = pg_substitution_input(&restored);
		assert(source->environment && !source->environment->parent);
		image = (struct pg_binding_value){source->environment->binder, source->environment->value.term};
		assert(!pg_substitution_init(&baseline, &graph, source->term, 1, &image));
		assert(!restored.state->input_storage && !baseline.state->input_storage);
		assert(pg_substitution_advance(&baseline, 10000) == PG_SUBSTITUTION_DONE);
		assert(pg_substitution_steps(&restored) == cut);
		assert(pg_substitution_advance(&restored, steps - cut) == PG_SUBSTITUTION_DONE);
		assert(pg_substitution_steps(&restored) == steps);
		assert(pg_alpha_equal(pg_substitution_result(&restored), pg_substitution_result(&baseline)) == 1);
		pg_substitution_destroy(&restored);
		pg_substitution_destroy(&baseline);
		pg_graph_destroy(&graph);
	}
}

static void comparison_fixture(struct pg_graph *graph, struct pg_comparison *work, unsigned mode)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	if (mode >= 4) {
		const struct pg_term *body = pg_reference(graph, mode == 5 ? x : y);
		for (size_t i = 0; i < 4; ++i)
			body = pg_application(graph, pg_lambda(graph, pg_binder(graph), body),
				pg_lambda(graph, pg_binder(graph), body));
		assert(!pg_independence_init(work, pg_lambda(graph, pg_binder(graph), body), x));
		return;
	}
	const struct pg_term *left = pg_reference(graph, x), *right = pg_reference(graph, mode == 1 ? x : y);
	for (size_t i = 0; i < 8; ++i) {
		left = pg_application(graph, left, left);
		right = pg_application(graph, right, right);
	}
	/* Unused binders force reference lookup through multiple scope links. */
	for (size_t i = 0; i < 4; ++i) {
		left = pg_lambda(graph, pg_binder(graph), left);
		right = pg_lambda(graph, pg_binder(graph), right);
	}
	if (mode == 3) assert(!pg_independence_init(work, left, x));
	else if (mode == 2) assert(!pg_independence_init(work, pg_lambda(graph, x, left), x));
	else assert(!pg_comparison_init(work, pg_lambda(graph, x, left), pg_lambda(graph, y, right), NULL, NULL));
}

static void write_comparison(FILE *file)
{
	struct pg_graph graph;
	struct pg_comparison work;
	assert(!pg_graph_init(&graph));
	comparison_fixture(&graph, &work, 0);
	const struct pg_term *roots[] = {work.state->pending->left, work.state->pending->right};
	assert(pg_comparison_advance(&work, 13) == PG_COMPARISON_PENDING);
	assert(!pg_comparison_write(file, &work, 2, roots, NULL, NULL));
	pg_comparison_destroy(&work);
	pg_graph_destroy(&graph);
}

static void read_comparison(FILE *file)
{
	struct pg_graph graph;
	struct pg_comparison work, baseline;
	size_t count;
	const struct pg_term *const *roots;
	assert(!pg_graph_init(&graph));
	comparison_fixture(&graph, &baseline, 0);
	assert(pg_comparison_advance(&baseline, 10000) == PG_COMPARISON_EQUAL);
	assert(!pg_comparison_read(file, &graph, 10000, 100, NULL, NULL, &work, &count, &roots));
	assert(count == 2 && pg_alpha_equal(roots[0], roots[1]) == 1);
	uint64_t steps = pg_comparison_steps(&baseline);
	assert(pg_comparison_steps(&work) == 13);
	assert(pg_comparison_advance(&work, steps - 14) == PG_COMPARISON_PENDING);
	assert(pg_comparison_advance(&work, 1) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_steps(&work) == steps);
	pg_comparison_destroy(&work);
	pg_comparison_destroy(&baseline);
	pg_graph_destroy(&graph);
}

static int unexpected_normalization(void *owner, const struct pg_term *input, const struct pg_term **output)
{
	(void)owner;
	(void)input;
	(void)output;
	assert(0);
	return -1;
}

static void comparison_boundaries(void)
{
	struct pg_graph graph;
	struct pg_comparison work, restored;
	size_t count;
	const struct pg_term *const *roots;
	assert(!pg_graph_init(&graph));
	const struct pg_term *x = pg_reference(&graph, pg_binder(&graph));
	FILE *file = tmpfile();
	assert(file && !pg_comparison_init(&work, x, x, NULL, unexpected_normalization));
	assert(pg_comparison_write(file, &work, 0, NULL, NULL, NULL));
	assert(ftell(file) == 0);
	pg_comparison_destroy(&work);
	assert(!pg_comparison_init(&work, x, x, NULL, NULL));
	assert(!pg_comparison_write(file, &work, 1, &x, NULL, NULL));
	rewind(file);
	assert(!pg_comparison_read(file, &graph, 100, 100, NULL, NULL, &restored, &count, &roots));
	assert(count == 1 && roots[0]->kind == PG_REFERENCE);
	assert(pg_comparison_status(&restored) == PG_COMPARISON_EQUAL);
	assert(!pg_comparison_task_count(&restored) && !pg_comparison_steps(&restored));
	pg_comparison_destroy(&work);
	pg_comparison_destroy(&restored);
	assert(!fclose(file));
	/* Earlier writers could suspend structural work in either no-op
	 * normalization phase. Both remain readable without replaying those phases. */
	for (unsigned stage = 0; stage < 2; ++stage) {
		comparison_fixture(&graph, &work, 0);
		work.state->pending->stage = stage;
		work.state->pending->normalized[0] = stage ? work.state->pending->left : NULL;
		file = tmpfile();
		assert(file && !pg_comparison_write(file, &work, 0, NULL, NULL, NULL));
		rewind(file);
		assert(!pg_comparison_read(file, &graph, 10000, 100, NULL, NULL, &restored, &count, &roots));
		assert(pg_comparison_advance(&restored, 10000) == PG_COMPARISON_EQUAL);
		pg_comparison_destroy(&work);
		pg_comparison_destroy(&restored);
		assert(!fclose(file));
	}
	file = tmpfile();
	assert(file);
	comparison_fixture(&graph, &work, 0);
	assert(!pg_comparison_write(file, &work, 0, NULL, NULL, NULL));
	assert(!fseek(file, 6, SEEK_SET) && fputc(1, file) != EOF);
	rewind(file);
	assert(pg_comparison_read(file, &graph, 10000, 100, NULL, NULL, &restored, &count, &roots));
	assert(!restored.state && !count && !roots);
	assert(!fseek(file, 6, SEEK_SET) && fputc(2, file) != EOF);
	/* Status without a pending task, out-of-range link, and impossible stage. */
	const long offsets[] = {24, 24, 80, 48};
	const uint64_t invalid[] = {0, 2, 3, 10001};
	for (size_t i = 0; i < 4; ++i) {
		uint64_t prior;
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_read_u64(file, &prior));
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, invalid[i]));
		rewind(file);
		assert(pg_comparison_read(file, &graph, 10000, 100, NULL, NULL, &restored, &count, &roots));
		assert(!restored.state && !count && !roots);
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, prior));
	}
	assert(!fclose(file));
	pg_comparison_destroy(&work);
	pg_graph_destroy(&graph);
}

static void comparison_resume(void)
{
	for (unsigned mode = 0; mode < 6; ++mode) {
		struct pg_graph graph;
		struct pg_comparison baseline;
		assert(!pg_graph_init(&graph));
		comparison_fixture(&graph, &baseline, mode);
		enum pg_comparison_status expected = pg_comparison_advance(&baseline, 10000);
		assert(expected == (mode % 2 ? PG_COMPARISON_DIFFERENT : PG_COMPARISON_EQUAL));
		uint64_t total = pg_comparison_steps(&baseline);
		size_t tasks = pg_comparison_task_count(&baseline);
		pg_comparison_destroy(&baseline);
		pg_graph_destroy(&graph);
		for (uint64_t cut = 0; cut <= total; ++cut) {
			struct pg_comparison work;
			assert(!pg_graph_init(&graph));
			comparison_fixture(&graph, &work, mode);
			const struct pg_term *initial[] = {work.state->pending->left, work.state->pending->right, NULL};
			initial[2] = pg_lambda(&graph, initial[0]->as.lambda.binder,
				pg_application(&graph, initial[0], initial[1]));
			const struct pg_term *const *owners = initial;
			pg_comparison_advance(&work, cut);
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file && !pg_comparison_write(file, &work, 3, owners, NULL, NULL));
				pg_comparison_destroy(&work);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t count;
				assert(!pg_comparison_read(file, &graph, 10000, 100, NULL, NULL, &work, &count, &owners));
				assert(count == 3);
				assert(owners[2]->as.lambda.binder == owners[0]->as.lambda.binder);
				assert(owners[2]->as.lambda.body->as.application.function == owners[0]);
				assert(owners[2]->as.lambda.body->as.application.argument == owners[1]);
				size_t found = 0;
				for (size_t bucket = 0; bucket < work.state->seen.capacity; ++bucket)
					for (const struct pg_index_entry *node = work.state->seen.buckets[bucket]; node; node = node->next) {
						const struct alpha_entry *entry = (const struct alpha_entry *)node;
						if (entry->left == owners[0] && entry->right == owners[1]) ++found;
					}
				assert(found == 1);
				assert(!fclose(file));
			}
			assert(pg_comparison_steps(&work) == cut);
			for (uint64_t i = cut; i < total; ++i)
				assert(pg_comparison_advance(&work, 1) == (i + 1 == total ? expected : PG_COMPARISON_PENDING));
			assert(pg_comparison_status(&work) == expected);
			assert(pg_comparison_task_count(&work) == tasks);
			pg_comparison_destroy(&work);
			pg_graph_destroy(&graph);
		}
	}
}

static struct pg_eval_configuration materialization_fixture(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	struct pg_environment *environment = pg_alloc(graph, sizeof(*environment));
	struct pg_argument *arguments = pg_alloc(graph, 2 * sizeof(*arguments));
	assert(environment && arguments);
	*environment = (struct pg_environment){x, {vy, NULL}, NULL};
	struct pg_closure head = {pg_lambda(graph, y, vx), environment};
	arguments[0] = (struct pg_argument){head, &arguments[1]};
	arguments[1] = (struct pg_argument){{pg_application(graph, vx, vx), environment}, NULL};
	return (struct pg_eval_configuration){head, arguments};
}

static size_t finish_materialization(struct materialization *work, struct pg_graph *graph,
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

static void write_materialization(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval_configuration input = materialization_fixture(&graph);
	struct materialization work = {0};
	for (size_t i = 0; i < 3; ++i)
		assert(!pg_materialize_step(&work, &graph, input.head, input.arguments));
	assert(!pg_materialization_write(file, &work, &input, NULL, NULL));
	pg_materialize_destroy(&work);
	pg_graph_destroy(&graph);
}

static void read_materialization(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval_configuration input;
	struct materialization work, baseline = {0};
	assert(!pg_materialization_read(file, &graph, 10000, 100, NULL, NULL, &work, &input));
	size_t total = finish_materialization(&baseline, &graph, input);
	assert(finish_materialization(&work, &graph, input) == total - 3);
	assert(pg_alpha_equal(work.partial, baseline.partial) == 1);
	assert(work.readback.results.count == baseline.readback.results.count);
	pg_materialize_destroy(&work);
	pg_materialize_destroy(&baseline);
	pg_graph_destroy(&graph);
}

static int materialization_terms_write(FILE *file, size_t count, const struct pg_term *const *roots, void *owner)
{
	(void)owner;
	return pg_graph_write_descriptors(file, count, roots, NULL, NULL);
}

static int materialization_terms_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *owner)
{
	int status = pg_graph_read_descriptors(file, graph, limit, name_limit, NULL, NULL, count, roots);
	if (!status && owner) *count = 0;
	return status;
}

static void materialization_resume(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval_configuration original = materialization_fixture(&graph);
	const struct pg_object *y = original.head.term->as.lambda.binder;
	struct materialization baseline = {0};
	size_t steps = finish_materialization(&baseline, &graph, original);
	for (size_t cut = 0; cut <= steps; ++cut) {
		struct materialization work = {0};
		for (size_t i = 0; i < cut; ++i)
			assert(pg_materialize_step(&work, &graph, original.head, original.arguments) == (i + 1 == steps));
		FILE *file = tmpfile();
		struct pg_eval_configuration extra[] = {original, {original.head, original.arguments->next}, original};
		assert(file && !pg_materialization_write_with(file, &work, &original, 3, extra, materialization_terms_write, NULL));
		pg_materialize_destroy(&work);
		struct pg_graph restored;
		struct pg_eval_configuration input;
		const struct pg_eval_configuration *restored_extra;
		for (size_t round = 0; round < 2; ++round) {
			assert(!pg_graph_init(&restored));
			rewind(file);
			assert(!pg_materialization_read_with(file, &restored, 10000, 100,
				materialization_terms_read, NULL, &work, &input, 3, &restored_extra));
			assert(restored_extra[0].head.term == input.head.term);
			assert(restored_extra[0].head.environment == input.head.environment);
			assert(restored_extra[0].arguments == input.arguments);
			assert(restored_extra[1].arguments == input.arguments->next);
			assert(restored_extra[2].arguments == input.arguments);
			struct materialization rejected;
			struct pg_eval_configuration ignored;
			const struct pg_eval_configuration *unused;
			rewind(file);
			assert(pg_materialization_read_with(file, &restored, 10000, 100,
				materialization_terms_read, NULL, &rejected, &ignored, 2, &unused));
			assert(!rejected.entry && !ignored.head.term && !unused);
			rewind(file);
			assert(pg_materialization_read_with(file, &restored, 10000, 100,
				materialization_terms_read, &input, &rejected, &ignored, 3, &unused));
			assert(!rejected.entry && !ignored.head.term && !unused);
			assert(!fclose(file));
			if (!round) {
				file = tmpfile();
				assert(file && !pg_materialization_write_with(file, &work, &input, 3, restored_extra,
					materialization_terms_write, NULL));
				pg_materialize_destroy(&work);
				pg_graph_destroy(&restored);
			}
		}
		assert(input.head.environment == input.arguments->value.environment);
		if (cut < 2) {
			FILE *invalid = tmpfile();
			assert(invalid && !pg_materialization_write(invalid, &work, &input, NULL, NULL));
			assert(!fseek(invalid, 40, SEEK_SET) && !pg_wire_write_u64(invalid, cut ? 7 : 2));
			rewind(invalid);
			struct materialization rejected;
			struct pg_eval_configuration ignored;
			assert(pg_materialization_read(invalid, &restored, 10000, 100, NULL, NULL, &rejected, &ignored));
			assert(!rejected.entry && !ignored.head.term);
			assert(!fclose(invalid));
		}
		for (size_t i = cut; i < steps; ++i)
			assert(pg_materialize_step(&work, &restored, input.head, input.arguments) == (i + 1 == steps));
		assert(work.done && work.partial);
		const struct pg_term *shared = work.partial->as.application.function;
		assert(shared->kind == PG_APPLICATION && shared->as.application.function == shared->as.application.argument);
		assert(work.readback.steps == baseline.readback.steps);
		assert(work.readback.results.count == baseline.readback.results.count);
		struct pg_binding_value correspondence = {y, input.head.environment->value.term};
		const struct pg_term *expected = pg_term_substitute(&restored, baseline.partial, 1, &correspondence);
		assert(pg_alpha_equal(work.partial, expected) == 1);
		pg_materialize_destroy(&work);
		pg_graph_destroy(&restored);
	}
	pg_materialize_destroy(&baseline);
	pg_graph_destroy(&graph);
}

static const struct pg_object_class frame_class = {"frame-test"};
static const struct pg_object frame_operation = {PG_SEMANTIC_OBJECT, &frame_class};
static int frame_auxiliary;
static size_t frame_resumes;

static const char *frame_name(void *unused, const struct pg_object *object)
{
	(void)unused;
	return object == &frame_operation ? "test/frame/v1" : NULL;
}
static const struct pg_object *frame_resolve(void *unused, const char *name)
{
	(void)unused;
	return !strcmp(name, "test/frame/v1") ? &frame_operation : NULL;
}
static const struct pg_graph_codec frame_codec = {.name = frame_name, .resolve = frame_resolve};

static int frame_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state);
static const struct pg_eval_continuation frame_answer_continuation = {
	"tests/eval_io/frame_answer/v1", frame_answer
};

static int frame_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	assert(state == &frame_operation);
	++frame_resumes;
	if (frame_auxiliary) return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 5);
	const struct pg_closure *argument = pg_eval_argument(machine, 3);
	assert(argument->term == answer && !argument->environment);
	return 1;
}
static int frame_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &frame_operation) return 1;
	if (frame_auxiliary) return pg_eval_demand_closure(machine, *pg_eval_argument(machine, 3), &frame_answer_continuation, &frame_operation);
	return pg_eval_demand(machine, 3, &frame_answer_continuation, &frame_operation);
}

static void invalid_frame_target(FILE *file, long prefix)
{
	uint64_t entries, environments, arguments, target;
	assert(!fseek(file, prefix + 8, SEEK_SET) && !pg_wire_read_u64(file, &entries));
	long configurations = prefix + 48 + 48 * (long)entries;
	assert(!fseek(file, configurations + 8, SEEK_SET));
	assert(!pg_wire_read_u64(file, &environments) && !pg_wire_read_u64(file, &arguments));
	long target_link = configurations + 32 + 16 * (long)(environments + arguments + 4 * entries + 4) + 8;
	assert(!fseek(file, target_link, SEEK_SET) && !pg_wire_read_u64(file, &target));
	assert(target && !fseek(file, target_link, SEEK_SET) && !pg_wire_write_u64(file, 0));
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	struct pg_eval_frame *rejected;
	struct pg_eval_configuration current;
	rewind(file);
	/* Auxiliary demand cannot have a partly copied argument prefix. */
	assert((prefix ? pg_eval_frames_payload_read : pg_eval_frame_payload_read)
		(file, &arena, &graph, 10000, 100, &frame_codec, NULL, &rejected, &current));
	assert(!rejected && !current.head.term);
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
	assert(!fseek(file, target_link, SEEK_SET) && !pg_wire_write_u64(file, target));
}

static const struct pg_term *frame_input(struct pg_graph *graph, size_t depth, const struct pg_object **free_binder)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *body = pg_lambda(graph, y, vx);
	for (size_t level = 0; level < depth; ++level) {
		const struct pg_term *call = pg_reference(graph, &frame_operation);
		for (size_t i = 0; i < 5; ++i) call = pg_application(graph, call, i == 3 ? body : vx);
		body = call;
	}
	if (free_binder) *free_binder = y;
	return pg_application(graph, pg_lambda(graph, x, body), vy);
}

static void write_frames(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval machine;
	frame_auxiliary = 1;
	pg_eval_init(&machine, frame_input(&graph, 3, NULL));
	machine.output = &graph; machine.dispatch = frame_dispatch;
	while (!machine.head_ready) assert(pg_eval_advance(&machine, 1) == PG_EVAL_PENDING && machine.steps < 1000);
	assert(machine.frames->parent && machine.frames->parent->parent);
	assert(pg_eval_advance(&machine, 3) == PG_EVAL_PENDING);
	struct pg_eval_configuration current = {machine.current, machine.arguments};
	assert(!pg_wire_write_u64(file, machine.steps));
	assert(!pg_eval_frames_payload_write(file, machine.frames, &current, &frame_codec, NULL));
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

static void read_frames(FILE *file)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval machine;
	pg_eval_init(&machine, pg_reference(&graph, &frame_operation));
	assert(!pg_wire_read_u64(file, &machine.steps));
	struct pg_eval_configuration current;
	assert(!pg_eval_frames_payload_read(file, &machine.temporary, &graph, 10000, 100,
		&frame_codec, NULL, &machine.frames, &current));
	frame_auxiliary = 1; frame_resumes = 0;
	machine.current = current.head; machine.arguments = current.arguments;
	machine.output = &graph; machine.dispatch = frame_dispatch; machine.head_ready = 1;
	size_t count = 0;
	for (struct pg_eval_frame *frame = machine.frames; frame; frame = frame->parent) {
		++count;
		frame->continuation = &frame_answer_continuation; frame->state = &frame_operation;
	}
	assert(count == 3);
	const struct pg_term *value = machine.frames->arguments->value.environment->value.term;
	const struct pg_term *expected = pg_lambda(&graph, pg_binder(&graph), value);
	assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF && frame_resumes == 3);
	assert(pg_alpha_equal(pg_eval_readback(&machine, &graph), expected) == 1);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

static void frame_resume(size_t depth)
{
	for (frame_auxiliary = 0; frame_auxiliary < 2; ++frame_auxiliary) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		const struct pg_object *y;
		const struct pg_term *input = frame_input(&graph, depth, &y);
		struct pg_eval baseline;
		pg_eval_init(&baseline, input);
		baseline.output = &graph; baseline.dispatch = frame_dispatch;
		assert(pg_eval_advance(&baseline, 1000) == PG_EVAL_WHNF);
		const struct pg_term *expected = pg_eval_readback(&baseline, &graph);
		uint64_t total = baseline.steps;
		pg_eval_destroy(&baseline);
		unsigned prefix_lengths = 0;
		size_t maximum_depth = 0;
		for (uint64_t cut = 0; cut < total; ++cut) {
			struct pg_eval machine;
			pg_eval_init(&machine, input);
			machine.output = &graph; machine.dispatch = frame_dispatch;
			frame_resumes = 0;
			pg_eval_advance(&machine, cut);
			size_t completed = frame_resumes;
			if (!machine.frames) { pg_eval_destroy(&machine); continue; }
			assert(!machine.task);
			size_t frames = 0;
			for (const struct pg_eval_frame *frame = machine.frames; frame; frame = frame->parent) {
				++frames;
				if (frame == machine.frames) continue;
				assert(!frame->answer.readback.output && !frame->answer.entry && !frame->answer.done);
				assert(!frame->first && !frame->last && frame->cursor == frame->arguments);
			}
			if (frames > maximum_depth) maximum_depth = frames;
			size_t copied = 0;
			for (const struct pg_argument *p = machine.frames->arguments; p != machine.frames->cursor; p = p->next) ++copied;
			assert(copied < 4);
			prefix_lengths |= 1u << copied;
			int ready = machine.head_ready;
			struct pg_graph restored = {0};
			frame_resumes = 0;
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				struct pg_eval_configuration current = {machine.current, machine.arguments};
				if (!round && machine.frames->parent) {
					FILE *invalid = tmpfile();
					assert(invalid);
					machine.frames->parent->answer.readback.output = &graph;
					assert(pg_eval_frames_payload_write(invalid, machine.frames, &current, &frame_codec, NULL));
					machine.frames->parent->answer.readback.output = NULL;
					struct pg_eval_frame *parent = machine.frames->parent;
					machine.frames->parent = machine.frames;
					assert(pg_eval_frames_payload_write(invalid, machine.frames, &current, &frame_codec, NULL));
					machine.frames->parent = parent;
					assert(!fclose(invalid));
				}
				assert(file && !(depth == 1 ? pg_eval_frame_payload_write : pg_eval_frames_payload_write)
					(file, machine.frames, &current, &frame_codec, NULL));
				if (copied) invalid_frame_target(file, depth == 1 ? 0 : 16);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&restored);
				assert(!pg_graph_init(&restored));
				pg_eval_init(&machine, input);
				rewind(file);
				assert(!(depth == 1 ? pg_eval_frame_payload_read : pg_eval_frames_payload_read)(file, &machine.temporary, &restored, 10000, 100,
					&frame_codec, NULL, &machine.frames, &current));
				assert(!fclose(file));
				machine.current = current.head; machine.arguments = current.arguments;
				machine.output = &restored; machine.dispatch = frame_dispatch;
				machine.head_ready = ready; machine.steps = cut;
				size_t restored_frames = 0;
				for (struct pg_eval_frame *frame = machine.frames; frame; frame = frame->parent) {
					++restored_frames;
					assert(!frame->continuation && !frame->state);
					frame->continuation = &frame_answer_continuation; frame->state = &frame_operation;
					assert(frame->arguments->value.environment == machine.frames->arguments->value.environment);
				}
				assert(restored_frames == frames);
				if (machine.current.environment) assert(machine.frames->arguments->value.environment == machine.current.environment);
			}
			assert(!frame_resumes);
			struct pg_binding_value correspondence = {y, machine.frames->arguments->value.environment->value.term};
			const struct pg_term *relocated = pg_term_substitute(&restored, expected, 1, &correspondence);
			assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF && machine.steps == total && frame_resumes == depth - completed);
			assert(pg_alpha_equal(pg_eval_readback(&machine, &restored), relocated) == 1);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&restored);
		}
		assert(prefix_lengths == (frame_auxiliary ? 1u : 15u));
		assert(maximum_depth == depth);
		pg_graph_destroy(&graph);
	}
}

int main(int argc, char **argv)
{
	if (argc == 3) {
		const struct {
			const char *name, *mode;
			void (*run)(FILE *);
		} commands[] = {
			{"write", "wb", write_fixture}, {"read", "rb", read_fixture},
			{"write-substitution", "wb", write_substitution},
			{"read-substitution", "rb", read_substitution},
			{"write-materialization", "wb", write_materialization},
			{"read-materialization", "rb", read_materialization},
			{"write-comparison", "wb", write_comparison}, {"read-comparison", "rb", read_comparison},
			{"write-frames", "wb", write_frames}, {"read-frames", "rb", read_frames}
		};
		for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
			if (strcmp(argv[1], commands[i].name)) continue;
			FILE *file = fopen(argv[2], commands[i].mode);
			assert(file);
			commands[i].run(file);
			assert(!fclose(file));
			return 0;
		}
		assert(!"unknown command");
		return 1;
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
	substitution_resume();
	identity_substitution_image();
	shared_substitution_images();
	materialization_resume();
	frame_resume(1);
	frame_resume(3);
	comparison_resume();
	comparison_boundaries();
	puts("evaluation configuration: captured environments, shared tails, inert resave and lexical relocation passed");
	return 0;
}
