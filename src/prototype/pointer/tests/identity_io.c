#include "identity_internal.h"
#include "eval_io.h"
#include "eval_internal.h"
#include "computation.h"
#include "computation_io.h"
#include "computation_internal.h"
#include "classifier.h"
#include "descriptor_io.h"
#include "evidence.h"
#include "symmetry.h"
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

static void continuation_frames(void)
{
	const char *names[] = {
		"computation/force_answer/v1", "computation/fold_answer/v1",
		"iadt/match_answer/v1", "iadt/action_answer/v1", "symmetry/symmetry_answer/v1",
		"identity/right_endpoint/v1", "identity/left_endpoint/v1", "identity/action_body/v1",
		"identity/action_source/v1", "identity/thunk_return_field/v1", "identity/field_answer/v1"
	};
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_term *term = pg_reference(&graph, pg_binder(&graph));
	struct action_binding binding = {.source = term->as.reference};
	struct action_scope scope = {term, term, 1, &binding};
	struct pg_eval_frame initial[11] = {0};
	for (size_t i = 0; i < 11; ++i) {
		initial[i].caller.term = term;
		initial[i].continuation = pg_computation_continuation_resolve(names[i]);
		assert(initial[i].continuation);
		if (pg_identity_continuation_uses_scope(initial[i].continuation)) initial[i].state = &scope;
		if (i + 1 < 11) initial[i].parent = &initial[i + 1];
	}
	struct pg_eval_frame *frames = initial;
	struct pg_eval_configuration current = {{term, NULL}, NULL};
	for (unsigned round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_computation_frames_write(file, frames, &current, &codec, NULL));
		pg_materialize_destroy(&frames->answer);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		assert(!pg_computation_frames_read(file, &arena, &graph, 1000, 100, &codec, NULL, &frames, &current));
		size_t i = 0;
		const struct action_scope *shared = NULL;
		for (struct pg_eval_frame *p = frames; p; p = p->parent, ++i) {
			assert(i < 11 && p->continuation == pg_computation_continuation_resolve(names[i]));
			assert(p->caller.term == current.head.term);
			if (p->state) {
				if (!shared) shared = p->state;
				assert(shared == p->state && shared->source == current.head.term);
				assert(shared->bindings[0].source == current.head.term->as.reference);
			}
		}
		assert(i == 11 && shared);
		/* Unknown names cannot select arbitrary host callbacks. */
		assert(!fseek(file, 24, SEEK_SET) && fputc('?', file) != EOF);
		rewind(file);
		struct pg_eval_frame *rejected;
		struct pg_eval_configuration empty;
		assert(pg_computation_frames_read(file, &arena, &graph, 1000, 100, &codec, NULL, &rejected, &empty));
		assert(!rejected && !empty.head.term && !empty.arguments);
		assert(!fclose(file));
	}
	FILE *bad = tmpfile();
	assert(bad);
	frames->state = frames->parent->parent->parent->parent->parent->state;
	assert(pg_computation_frames_write(bad, frames, &current, &codec, NULL));
	assert(!fclose(bad));
	pg_materialize_destroy(&frames->answer);
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

static void force_frames(void)
{
	uint64_t total = 0;
	unsigned retained = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		const struct pg_term *thunk = pg_reference(&graph, &pg_thunk_operation);
		const struct pg_term *force = pg_reference(&graph, &pg_force_operation);
		const struct pg_object *x = pg_binder(&graph);
		const struct pg_term *body = pg_application(&graph, thunk, value);
		const struct pg_term *delayed = pg_application(&graph, pg_lambda(&graph, x, body), value);
		const struct pg_term *term = pg_application(&graph, force, delayed);
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph, term);
		struct pg_environment anchor = {pg_binder(&graph), {value, NULL}, NULL};
		machine.current.environment = &anchor;
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.frames) {
			++retained;
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				struct pg_eval_configuration current = {machine.current, machine.arguments};
				assert(file && !pg_computation_frames_write(file, machine.frames, &current, &pg_builtin_graph_codec, NULL));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				assert(!machine.task);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				struct pg_eval_frame *frames;
				assert(!pg_computation_frames_read(file, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &frames, &current));
				value = frames->arguments->value.environment->value.term;
				assert(!fclose(file));
				pg_computation_eval_init(&machine, &graph, current.head.term);
				machine.current = current.head;
				machine.arguments = current.arguments;
				machine.frames = frames;
				machine.steps = steps;
				machine.head_ready = ready;
			}
		}
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		const struct pg_term *result = pg_eval_readback(&machine, &graph);
		assert(result == value);
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
	assert(retained);
}

static const struct pg_term *const *handler_fixture(struct pg_graph *graph)
{
	const struct pg_term **roots = pg_alloc(graph, 8 * sizeof(*roots));
	assert(roots);
	const struct pg_object *a = pg_binder(graph), *k = pg_binder(graph);
	const struct pg_term *v0 = pg_reference(graph, pg_binder(graph));
	const struct pg_term *v1 = pg_reference(graph, pg_binder(graph));
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	roots[2] = pg_application(graph, ret, v0);
	roots[3] = pg_application(graph, ret, v1);
	/* Equal payload records must not merge generative operation identities. */
	const struct pg_object *first = pg_operation_label_create(graph, v0, v0);
	const struct pg_object *second = pg_operation_label_create(graph, v0, v0);
	assert(first && second && first != second);
	struct pg_operation_clause clauses[] = {
		{second, pg_lambda(graph, a, pg_lambda(graph, k, roots[2]))},
		{first, pg_lambda(graph, a, pg_lambda(graph, k, roots[3]))}
	};
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *request = pg_computation_request(graph, clauses[i].label, v0, ret);
		roots[i] = pg_computation_fold(graph, request, ret, 2, clauses);
		assert(roots[i]);
	}
	roots[4] = roots[0];
	roots[5] = pg_computation_fold(graph, roots[2], pg_lambda(graph, a, roots[3]), 0, NULL);
	size_t axes[128];
	for (size_t i = 0; i < 128; ++i) axes[i] = 127 - i;
	const struct pg_term *swap = pg_reference(graph, pg_symmetry_restore(graph, 128, axes));
	roots[6] = pg_application(graph, swap, pg_application(graph, swap, v0));
	roots[7] = pg_application(graph, pg_reference(graph, pg_symmetry_restore(graph, 0, NULL)), v1);
	return roots;
}

struct fold_codec {
	struct pg_graph *arena;
	struct fold_work *work;
};

static int write_fold(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct fold_codec *context = opaque;
	return pg_fold_work_write(file, context->work, count, roots, &pg_builtin_graph_codec, NULL);
}

static int read_fold(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct fold_codec *context = opaque;
	return pg_fold_work_read(file, context->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, NULL, &context->work, count, roots);
}

static void fold_progress(void)
{
	unsigned phases = 0;
	int rejected_positions = 0;
	for (size_t selected = 0; selected < 3; ++selected) {
		uint64_t total = 0;
		for (uint64_t cut = 0; ; ++cut) {
			struct pg_graph graph, arena = {0};
			assert(!pg_graph_init(&graph));
			const struct pg_term *const *roots = handler_fixture(&graph);
			const struct pg_term *expected = roots[2 + selected], *input = roots[selected];
			if (selected == 2) {
				const struct pg_term *ret = pg_reference(&graph, &pg_return_operation);
				const struct pg_term *payload = roots[2]->as.application.argument;
				const struct pg_object *label = pg_operation_label_create(&graph, payload, payload);
				expected = pg_computation_request(&graph, label, payload, ret);
				input = pg_computation_fold(&graph, expected, ret, 0, NULL);
			}
			struct pg_eval machine;
			pg_computation_eval_init(&machine, &graph, input);
			if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
				assert(machine.steps == total);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&graph);
				break;
			}
			if (machine.task) {
				assert(machine.task->operation == &pg_fold_work_operation && !machine.frames);
				struct fold_codec context = {&arena, machine.task->state};
				phases |= 1u << context.work->phase;
				if (!rejected_positions) {
					const long offsets[] = {8, 24, 32};
					for (size_t i = 0; i < 3; ++i) {
						FILE *bad = tmpfile();
						assert(bad && !pg_fold_work_write(bad, context.work, 0, NULL, &pg_builtin_graph_codec, NULL));
						assert(!fseek(bad, offsets[i], SEEK_SET) && !pg_wire_write_u64(bad, UINT64_MAX));
						rewind(bad);
						struct fold_work *rejected;
						size_t count;
						const struct pg_term *const *unused;
						assert(pg_fold_work_read(bad, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
							&rejected, &count, &unused));
						assert(!rejected && !count && !unused && !fclose(bad));
					}
					rejected_positions = 1;
				}
				for (unsigned round = 0; round < 2; ++round) {
					struct pg_eval_configuration inputs[] = {
						{machine.current, machine.arguments}, {{expected, NULL}, NULL}
					};
					FILE *file = tmpfile();
					assert(file && !pg_eval_configurations_write_with(file, 2, inputs, write_fold, &context));
					uint64_t steps = machine.steps;
					int ready = machine.head_ready;
					pg_eval_destroy(&machine);
					pg_graph_destroy(&arena);
					pg_graph_destroy(&graph);
					assert(!pg_graph_init(&graph));
					rewind(file);
					size_t n;
					const struct pg_eval_configuration *restored;
					assert(!pg_eval_configurations_read_with(file, &graph, 10000, 100, &n, &restored, read_fold, &context));
					assert(n == 2 && !fclose(file));
					assert(context.work->head == restored[0].head.term);
					expected = restored[1].head.term;
					pg_computation_eval_init(&machine, &graph, restored[0].head.term);
					machine.current = restored[0].head;
					machine.arguments = restored[0].arguments;
					machine.steps = steps;
					machine.head_ready = ready;
					assert(!pg_eval_defer(&machine, &pg_fold_work_operation, context.work));
				}
			}
			assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
			const struct pg_term *result = pg_eval_readback(&machine, &graph);
			if (selected < 2) assert(result == expected);
			else {
				const struct pg_object *label, *expected_label;
				const struct pg_term *payload, *resume, *expected_payload, *expected_resume;
				assert(pg_computation_request_view(result, &label, &payload, &resume));
				assert(pg_computation_request_view(expected, &expected_label, &expected_payload, &expected_resume));
				assert(label == expected_label && payload == expected_payload);
				struct pg_eval continuation;
				pg_computation_eval_init(&continuation, &graph, pg_application(&graph, resume, payload));
				assert(pg_eval_advance(&continuation, 1000) == PG_EVAL_WHNF);
				assert(pg_eval_readback(&continuation, &graph) == pg_application(&graph, expected_resume, payload));
				pg_eval_destroy(&continuation);
			}
			if (!cut) total = machine.steps;
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
		}
	}
	assert(phases == 15);
}

static const struct pg_object *handler_head(const struct pg_term *term)
{
	while (term->kind == PG_APPLICATION) term = term->as.application.function;
	assert(term->kind == PG_REFERENCE);
	return term->as.reference;
}

static void check_handlers(struct pg_graph *graph, const struct pg_term *const *roots)
{
	assert(roots[0] == roots[4]);
	const struct pg_object *object = handler_head(roots[0]);
	assert(object == handler_head(roots[1]));
	size_t count;
	const struct pg_clause_position *positions;
	assert(pg_computation_handler_view(object, &count, &positions) && count == 2);
	assert(positions[0].label != positions[1].label);
	assert(pg_computation_handler_restore(graph, count, positions) == object);
	struct pg_clause_position reversed[] = {positions[1], positions[0]};
	assert(pg_computation_handler_restore(graph, 2, reversed) == object);
	assert(handler_head(roots[5]) == &pg_fold_operation);
	size_t dimension;
	const size_t *axes;
	assert(pg_symmetry_object_view(handler_head(roots[6]), &dimension, &axes) && dimension == 128);
	for (size_t i = 0; i < dimension; ++i) assert(axes[i] == 127 - i);
	assert(pg_symmetry_object_view(handler_head(roots[7]), &dimension, &axes) && dimension == 0);
	assert(roots[7]->kind == PG_APPLICATION);
	const size_t inputs[] = {0, 1, 5, 6, 7};
	const struct pg_term *expected[] = {roots[2], roots[3], roots[3],
		roots[2]->as.application.argument, roots[3]->as.application.argument};
	for (size_t i = 0; i < 5; ++i) {
		struct pg_eval machine;
		pg_computation_eval_init(&machine, graph, roots[inputs[i]]);
		while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < 10000);
		assert(machine.status == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, graph) == expected[i]);
		pg_eval_destroy(&machine);
	}
	struct pg_clause_position invalid[] = {positions[0], positions[1]};
	invalid[1].position = invalid[0].position;
	assert(!pg_computation_handler_restore(graph, 2, invalid));
	invalid[1] = positions[1];
	invalid[1].label = invalid[0].label;
	assert(!pg_computation_handler_restore(graph, 2, invalid));
	invalid[1] = positions[1];
	invalid[1].position = 2;
	assert(!pg_computation_handler_restore(graph, 2, invalid));
	assert(!pg_computation_handler_restore(graph, 0, NULL));
	struct pg_operation_clause missing = {positions[0].label, NULL};
	assert(!pg_computation_fold(graph, roots[2], roots[3], 1, &missing));
	uint64_t bad_axes[] = {0, 0};
	size_t owners = graph->objects.count;
	assert(!pg_builtin_graph_codec.restore(NULL, graph, "symmetry/v1", 0, NULL, 2, bad_axes));
	bad_axes[1] = 2;
	assert(!pg_builtin_graph_codec.restore(NULL, graph, "symmetry/v1", 0, NULL, 2, bad_axes));
	assert(graph->objects.count == owners);
}

static void handlers(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *const *roots = handler_fixture(&graph);
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_graph_write_descriptors(file, 8, roots, &pg_builtin_graph_codec, NULL));
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		struct pg_classifiers classifiers;
		assert(!pg_classifiers_init(&classifiers, &graph));
		rewind(file);
		size_t count;
		assert(!pg_graph_read_descriptors(file, &graph, 1000, 100, &pg_builtin_graph_codec, &classifiers, &count, &roots));
		assert(count == 8 && !fclose(file));
		check_handlers(&graph, roots);
		pg_classifiers_destroy(&classifiers);
	}
	pg_graph_destroy(&graph);
}

struct scope_frame_codec {
	struct pg_graph *arena;
	struct action_scope *scope;
	int omit_root;
};

static size_t scope_answers;

static int write_scope_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct scope_frame_codec *context = opaque;
	return pg_action_scope_write(file, context->scope, count, roots, &codec, NULL);
}

static int read_scope_terms(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct scope_frame_codec *context = opaque;
	int status = pg_action_scope_read(file, context->arena, graph, limit, name_limit, &codec, NULL,
		&context->scope, count, roots);
	if (!status && context->omit_root && *count) --*count;
	return status;
}

static int scope_answer(struct pg_eval *machine, const struct pg_term *answer, const void *opaque);
static const struct pg_eval_continuation scope_answer_continuation = {
	"tests/identity_io/scope_answer/v1", scope_answer
};

static int scope_answer(struct pg_eval *machine, const struct pg_term *answer, const void *opaque)
{
	++scope_answers;
	const struct action_scope *scope = opaque;
	assert(scope->source == machine->current.term);
	assert(scope->bindings[0].source == machine->current.environment->binder);
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 0);
}

static void scope_frames(void)
{
	uint64_t total_steps = 0;
	for (uint64_t cut = 0; ; ++cut) {
		scope_answers = 0;
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
		const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y);
		const struct pg_term *source = pg_lambda(&graph, x, vx);
		const struct pg_term *body = pg_lambda(&graph, y, vx);
		struct action_binding binding = {.source = x, .arguments = {y, NULL, y}};
		struct action_scope initial = {source, body, 1, &binding};
		struct pg_environment environment = {x, {vy, NULL}, NULL};
		struct pg_eval machine;
		pg_eval_init(&machine, source);
		machine.current.environment = &environment;
		machine.output = &graph;
		assert(!pg_eval_demand_closure(&machine, (struct pg_closure){body, &environment}, &scope_answer_continuation, &initial));
		pg_eval_advance(&machine, cut);
		if (!machine.frames) {
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		assert(!scope_answers);
		struct scope_frame_codec context = {.arena = &arena, .scope = &initial};
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			struct pg_eval_configuration current = {machine.current, machine.arguments};
			assert(file && !pg_eval_frames_payload_write_with(file, machine.frames, &current, write_scope_terms, &context));
			uint64_t steps = machine.steps;
			int ready = machine.head_ready;
			pg_eval_destroy(&machine);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
			assert(!pg_graph_init(&graph));
			rewind(file);
			struct pg_eval_frame *frames;
			assert(!pg_eval_frames_payload_read_with(file, &arena, &graph, 10000, 100,
				read_scope_terms, &context, &frames, &current));
			/* Failure outside a successfully restored owner must not publish
			 * a partially connected frame. Scope storage is arena-owned. */
			rewind(file);
			struct scope_frame_codec bad = {.arena = &arena, .omit_root = 1};
			struct pg_eval_frame *rejected;
			struct pg_eval_configuration unused;
			assert(pg_eval_frames_payload_read_with(file, &arena, &graph, 10000, 100,
				read_scope_terms, &bad, &rejected, &unused));
			assert(bad.scope && !rejected && !unused.head.term && !unused.arguments);
			assert(!fclose(file));
			assert(frames->caller.term == context.scope->source);
			assert(frames->caller.environment->binder == context.scope->bindings[0].source);
			assert(context.scope->body->as.lambda.binder == context.scope->bindings[0].arguments[0]);
			assert(context.scope->bindings[0].arguments[0] == context.scope->bindings[0].arguments[2]);
			assert(!context.scope->bindings[0].arguments[1]);
			pg_eval_init(&machine, current.head.term);
			machine.current = current.head;
			machine.arguments = current.arguments;
			machine.output = &graph;
			machine.frames = frames;
			machine.steps = steps;
			machine.head_ready = ready;
			/* Test supplies the known continuation; callback selection is not
			 * part of the data codec and no host address came from the file. */
			frames->continuation = &scope_answer_continuation;
			frames->state = context.scope;
		}
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(scope_answers == 1);
		if (!cut) total_steps = machine.steps;
		assert(machine.steps == total_steps);
		const struct pg_term *answer = pg_eval_readback(&machine, &graph);
		assert(answer->kind == PG_LAMBDA);
		assert(answer->as.lambda.binder != context.scope->bindings[0].arguments[0]);
		assert(answer->as.lambda.body->as.reference == context.scope->bindings[0].arguments[0]);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(cut < 1000);
	}
}

static void scope_sharing(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_term *term = pg_reference(&graph, pg_binder(&graph));
	struct action_binding bindings[2] = {{.source = term->as.reference}, {.source = term->as.reference}};
	struct action_binding separate[2] = {bindings[0], bindings[1]};
	struct action_scope a = {term, term, 2, bindings}, b = a, c = {term, term, 2, separate};
	struct action_scope prefix = {term, term, 1, bindings};
	struct action_scope *initial[] = {&a, &b, &a, NULL, &c, &prefix};
	struct action_scope *const *scopes = initial;
	const struct pg_term *const *extra = &term;
	for (size_t round = 0; round < 2; ++round) {
		const struct action_scope *inputs[6];
		for (size_t i = 0; i < 6; ++i) inputs[i] = scopes[i];
		FILE *file = tmpfile();
		assert(file && !pg_action_scopes_write(file, 6, inputs, 1, extra, &codec, NULL));
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		size_t n, count;
		assert(!pg_action_scopes_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &scopes, &count, &extra));
		assert(n == 6 && count == 1 && !scopes[3]);
		assert(scopes[0] == scopes[2] && scopes[0] != scopes[1]);
		assert(scopes[0]->bindings == scopes[1]->bindings && scopes[0]->bindings == scopes[5]->bindings);
		assert(scopes[0]->bindings != scopes[4]->bindings && scopes[5]->count == 1);
		assert(scopes[0]->source == extra[0] && scopes[4]->body == extra[0]);
		scopes[0]->bindings[0].arguments[0] = extra[0]->as.reference;
		assert(scopes[1]->bindings[0].arguments[0] == extra[0]->as.reference);
		assert(!scopes[4]->bindings[0].arguments[0]);
		assert(!fseek(file, 32, SEEK_SET) && !pg_wire_write_u64(file, 1001));
		rewind(file);
		struct action_scope *const *rejected;
		const struct pg_term *const *unused;
		assert(pg_action_scopes_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &rejected, &count, &unused));
		assert(!n && !rejected && !count && !unused);
		assert(!fclose(file));
	}
	FILE *file = tmpfile();
	assert(file && !pg_action_scopes_write(file, 0, NULL, 1, extra, &codec, NULL));
	rewind(file);
	size_t n, count;
	assert(!pg_action_scopes_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &scopes, &count, &extra));
	assert(!n && count == 1 && !fclose(file));
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

static void scopes(void)
{
	/* Every nullable binding field is retained, not synthesized on import. */
	for (int variant = 0; variant < 34; ++variant) {
		int mask = variant % 17 - 1;
		size_t arity = variant < 17 ? 2 : 0;
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
		const struct pg_term *body = pg_reference(&graph, x);
		const struct pg_term *source = arity ? pg_lambda(&graph, x, pg_lambda(&graph, y, body)) : body;
		struct action_binding bindings[2] = {0};
		if (mask & 1) bindings[0].source = bindings[1].source = x;
		for (size_t j = 0; j < 3; ++j)
			if (mask & (2 << j)) bindings[0].arguments[j] = bindings[1].arguments[j] = y;
		struct action_scope initial = {source, body, arity, mask < 0 ? NULL : bindings};
		struct action_scope *scope = &initial;
		const struct pg_term *extra[] = {source, body, pg_reference(&graph, y)};
		const struct pg_term *const *roots = extra;
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			assert(file && !pg_action_scope_write(file, scope, 3, roots, &codec, NULL));
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
			assert(!pg_graph_init(&graph));
			rewind(file);
			size_t count;
			assert(!pg_action_scope_read(file, &arena, &graph, 10000, 100, &codec, NULL, &scope, &count, &roots));
			assert(count == 3 && scope->count == arity);
			assert(scope->source == roots[0] && scope->body == roots[1]);
			x = roots[1]->as.reference;
			y = roots[2]->as.reference;
			if (arity) {
				assert(roots[0]->as.lambda.binder == x);
				assert(roots[0]->as.lambda.body->as.lambda.binder == y);
			} else assert(scope->source == scope->body);
			assert((scope->bindings != NULL) == (mask >= 0));
			for (size_t i = 0; scope->bindings && i < arity; ++i) {
				assert(scope->bindings[i].source == ((mask & 1) ? x : NULL));
				for (size_t j = 0; j < 3; ++j)
					assert(scope->bindings[i].arguments[j] == ((mask & (2 << j)) ? y : NULL));
			}
			/* The stream cannot request a field outside action_binding. */
			if (mask >= 0 && arity) {
				assert(!fseek(file, 64, SEEK_SET) && !pg_wire_write_u64(file, 16));
				rewind(file);
				struct action_scope *rejected;
				const struct pg_term *const *unused;
				assert(pg_action_scope_read(file, &arena, &graph, 10000, 100, &codec, NULL,
					&rejected, &count, &unused));
				assert(!rejected && !count && !unused);
			}
			assert(!fclose(file));
		}
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
}

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

static void check_configuration_resume(struct action_body_work *work, struct pg_eval_configuration caller,
	const struct pg_term *expected)
{
	struct pg_eval machine;
	pg_eval_init(&machine, caller.head.term);
	machine.current = caller.head;
	machine.arguments = caller.arguments;
	machine.output = work->output;
	assert(!pg_eval_defer(&machine, &pg_action_body_operation, work));
	assert(pg_eval_advance(&machine, 10) == PG_EVAL_WHNF);
	assert(pg_alpha_equal(pg_eval_readback(&machine, work->output), expected) == 1);
	pg_eval_destroy(&machine);
}

static void configurations(void)
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
		for (size_t cut = 0; cut <= total; ++cut) {
			assert(!pg_graph_init(&graph));
			struct action_body_work *work = fixture(&graph, &arena, different, &request, &expected);
			struct pg_environment environment = {work->scope.source->as.lambda.binder,
				{pg_reference(&graph, pg_binder(&graph)), NULL}, NULL};
			struct pg_argument argument = {{work->scope.source, &environment}, NULL};
			struct pg_eval_configuration initial[] = {
				{{request->as.application.function, &environment}, &argument},
				{{expected, NULL}, NULL},
				{{work->scope.source, &environment}, &argument}
			};
			const struct pg_eval_configuration *roots = initial;
			for (size_t i = 0; i < cut; ++i) assert(pg_action_body_operation.poll(work) == (i + 1 == total));
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file && !pg_action_body_configurations_write(file, work, 3, roots, &codec, NULL));
				pg_action_body_operation.destroy(work);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t count;
				assert(!pg_action_body_configurations_read(file, &arena, &graph, 10000, 100,
					&codec, NULL, &work, &count, &roots));
				assert(!fclose(file) && count == 3);
				assert(roots[0].arguments == roots[2].arguments);
				assert(roots[0].head.environment == roots[0].arguments->value.environment);
				assert(roots[0].head.environment == roots[2].head.environment);
				assert(roots[0].arguments->value.term == work->scope.source);
				assert(roots[0].head.environment->binder == work->scope.source->as.lambda.binder);
			}
			for (size_t i = cut; i < total; ++i) assert(pg_action_body_operation.poll(work) == (i + 1 == total));
			check_configuration_resume(work, roots[0], roots[1].head.term);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
		}
	}
}

static int invalid_environment_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	const struct action_body_work *work = opaque;
	struct pg_graph scratch = {0};
	const struct pg_term **changed = pg_alloc(&scratch, count * sizeof(*changed));
	assert(changed && count);
	memcpy(changed, roots, count * sizeof(*changed));
	/* An APP is a valid Term but not an environment's binder reference. */
	changed[0] = work->scope.body;
	int status = pg_action_body_write(file, work, count, changed, &codec, NULL);
	pg_graph_destroy(&scratch);
	return status;
}

static void configuration_failure(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_term *request, *expected;
	struct action_body_work *work = fixture(&graph, &arena, 1, &request, &expected);
	struct pg_environment environment = {work->scope.source->as.lambda.binder, {expected, NULL}, NULL};
	struct pg_eval_configuration root = {{request, &environment}, NULL};
	FILE *file = tmpfile();
	assert(file && !pg_eval_configurations_write_with(file, 1, &root, invalid_environment_terms, work));
	pg_action_body_operation.destroy(work);
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
	assert(!pg_graph_init(&graph));
	rewind(file);
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(pg_action_body_configurations_read(file, &arena, &graph, 10000, 100, &codec, NULL, &work, &count, &roots));
	assert(!work && !count && !roots);
	assert(!fclose(file));
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	if (argc == 3 && (!strcmp(argv[1], "write-handlers") || !strcmp(argv[1], "read-handlers"))) {
		struct pg_graph graph;
		struct pg_classifiers classifiers;
		assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
		if (!strcmp(argv[1], "write-handlers")) {
			const struct pg_term *const *roots = handler_fixture(&graph);
			FILE *file = fopen(argv[2], "wb");
			assert(file && !pg_graph_write_descriptors(file, 8, roots, &pg_builtin_graph_codec, &classifiers));
			assert(!fclose(file));
		} else {
			FILE *file = fopen(argv[2], "rb");
			size_t count;
			const struct pg_term *const *roots;
			assert(file && !pg_graph_read_descriptors(file, &graph, 1000, 100,
				&pg_builtin_graph_codec, &classifiers, &count, &roots));
			assert(count == 8 && !fclose(file));
			check_handlers(&graph, roots);
		}
		pg_classifiers_destroy(&classifiers);
		pg_graph_destroy(&graph);
		return 0;
	}
	if (argc == 3) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		if (!strcmp(argv[1], "write")) {
			const struct pg_term *roots[2];
			struct action_body_work *work = fixture(&graph, &arena, 1, &roots[0], &roots[1]);
			for (size_t i = 0; i < 5; ++i) assert(!pg_action_body_operation.poll(work));
			struct pg_environment environment = {work->scope.source->as.lambda.binder,
				{pg_reference(&graph, pg_binder(&graph)), NULL}, NULL};
			struct pg_argument argument = {{work->scope.source, &environment}, NULL};
			struct pg_eval_configuration configurations[] = {
				{{roots[0]->as.application.function, &environment}, &argument}, {{roots[1], NULL}, NULL}
			};
			FILE *file = fopen(argv[2], "wb");
			assert(file && !pg_action_body_configurations_write(file, work, 2, configurations, &codec, NULL));
			assert(!fclose(file));
			pg_action_body_operation.destroy(work);
		} else {
			assert(!strcmp(argv[1], "read"));
			FILE *file = fopen(argv[2], "rb");
			struct action_body_work *work;
			size_t count;
			const struct pg_eval_configuration *roots;
			assert(file && !pg_action_body_configurations_read(file, &arena, &graph, 10000, 100, &codec, NULL, &work, &count, &roots));
			assert(!fclose(file) && count == 2);
			assert(roots[0].head.environment == roots[0].arguments->value.environment);
			assert(roots[0].arguments->value.term == work->scope.source);
			finish(work);
			check_configuration_resume(work, roots[0], roots[1].head.term);
		}
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		return 0;
	}
	assert(argc == 1);
	handlers();
	scopes();
	scope_sharing();
	continuation_frames();
	force_frames();
	fold_progress();
	scope_frames();
	all_cuts();
	configurations();
	configuration_failure();
	puts("Identity ownership: scope/frame sharing and body work resume through the original evaluator");
	return 0;
}
