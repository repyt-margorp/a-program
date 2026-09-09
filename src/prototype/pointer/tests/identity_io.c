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
#include "symmetry_internal.h"
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

static unsigned machine_tasks;

static void machine_resave(struct pg_eval *machine, struct pg_graph *graph,
	struct pg_classifiers *classifiers, const struct pg_term **expected)
{
	const struct pg_eval_work_operation *entries[] = {
		&pg_fold_work_operation, &pg_symmetry_composition_operation, &pg_symmetry_prefix_operation,
		&pg_action_scope_operation, &pg_action_result_operation, &pg_scope_operation,
		&pg_action_body_operation, &pg_higher_scope_operation,
		&pg_force_family_scope_operation, &pg_field_family_scope_operation,
		&pg_force_family_result_operation, &pg_field_family_result_operation
	};
	const struct pg_eval_work_operation *operation = machine->task ? machine->task->operation : NULL;
	for (size_t i = 0; i < 12; ++i) if (operation == entries[i]) machine_tasks |= 1u << i;
	uint64_t steps = machine->steps;
	enum pg_eval_status status = machine->status;
	int ready = machine->head_ready, framed = machine->frames != NULL;
	for (size_t round = 0; round < 2; ++round) {
		struct pg_eval_configuration extra[] = {{{*expected, NULL}, NULL}};
		FILE *file = tmpfile();
		assert(file && !pg_computation_machine_write(file, machine, &pg_pure_policy, 1, extra,
			&pg_builtin_graph_codec, classifiers));
		pg_eval_destroy(machine);
		if (classifiers) pg_classifiers_destroy(classifiers);
		pg_graph_destroy(graph);
		assert(!pg_graph_init(graph));
		if (classifiers) assert(!pg_classifiers_init(classifiers, graph));
		rewind(file);
		const struct pg_eval_policy *policy;
		size_t count;
		const struct pg_eval_configuration *roots;
		assert(!pg_computation_machine_read(file, machine, graph, 10000, 100, &pg_builtin_graph_codec,
			classifiers, &policy, &count, &roots));
		assert(policy == &pg_pure_policy && count == 1);
		assert(machine->steps == steps && machine->status == status && machine->head_ready == ready);
		assert((machine->frames != NULL) == framed);
		assert((machine->task ? machine->task->operation : NULL) == operation);
		*expected = roots[0].head.term;
		if (operation || framed) {
			assert(!fseek(file, 16, SEEK_SET) && !pg_wire_write_u64(file, PG_EVAL_WHNF));
			rewind(file);
			struct pg_eval rejected;
			assert(pg_computation_machine_read(file, &rejected, graph, 10000, 100,
				&pg_builtin_graph_codec, classifiers, &policy, &count, &roots));
			assert(!policy && !count && !roots && !rejected.task && !rejected.frames);
			pg_eval_destroy(&rejected);
		}
		assert(!fclose(file));
	}
}

static void policy_names(void)
{
	const struct pg_eval_policy *policies[] = {&pg_beta_policy, &pg_pure_policy};
	struct pg_graph graph;
	struct pg_whnf_work work;
	assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&work, &graph));
	const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
	const struct pg_term *thunk = pg_application(&graph, pg_reference(&graph, &pg_thunk_operation), value);
	const struct pg_term *input = pg_application(&graph, pg_reference(&graph, &pg_force_operation), thunk);
	assert(input);
	for (size_t i = 0; i < 2; ++i) {
		const char *name = pg_computation_policy_name(policies[i]);
		assert(name && strlen(name) < 100);
		FILE *file = tmpfile();
		assert(file && fwrite(name, 1, strlen(name), file) == strlen(name));
		rewind(file);
		char restored[100] = {0};
		assert(fread(restored, 1, strlen(name), file) == strlen(name) && !fclose(file));
		const struct pg_eval_policy *policy = pg_computation_policy_resolve(restored);
		assert(policy == policies[i]);
		struct pg_eval_policy local = *policy;
		assert(!pg_computation_policy_name(&local));
		struct pg_whnf_job *job = pg_whnf_request(&work, policy, input);
		assert(job && job == pg_whnf_request(&work, policies[i], input));
		assert(pg_whnf_advance(job, 1000) == PG_EVAL_WHNF);
		assert(pg_whnf_result(job) == (i ? value : input));
		assert(pg_reduction_policy(pg_whnf_certificate(job)) == policy);
	}
	assert(strcmp(pg_computation_policy_name(policies[0]), pg_computation_policy_name(policies[1])));
	assert(!pg_computation_policy_name(NULL));
	assert(!pg_computation_policy_resolve(NULL));
	assert(!pg_computation_policy_resolve(""));
	assert(!pg_computation_policy_resolve("evaluation/pure/v2"));
	assert(!pg_computation_policy_resolve("computation/fold_work/v1"));
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
}

static void work_names(void)
{
	const struct pg_eval_work_operation *entries[] = {
		&pg_fold_work_operation, &pg_symmetry_composition_operation, &pg_symmetry_prefix_operation,
		&pg_action_scope_operation, &pg_action_result_operation, &pg_scope_operation,
		&pg_action_body_operation, &pg_higher_scope_operation,
		&pg_force_family_scope_operation, &pg_field_family_scope_operation,
		&pg_force_family_result_operation, &pg_field_family_result_operation
	};
	for (size_t i = 0; i < sizeof(entries) / sizeof(*entries); ++i) {
		assert(entries[i]->name && pg_computation_work_resolve(entries[i]->name) == entries[i]);
		assert(entries[i]->poll && entries[i]->resume && entries[i]->destroy);
		for (size_t j = 0; j < i; ++j) assert(strcmp(entries[i]->name, entries[j]->name));
	}
	assert(!pg_computation_work_resolve(NULL));
	assert(!pg_computation_work_resolve(""));
	assert(!pg_computation_work_resolve("identity/action_scope/v2"));
	assert(!pg_computation_work_resolve("computation/force_answer/v1"));
	assert(!pg_identity_work_resolve(pg_fold_work_operation.name));
	assert(!pg_symmetry_work_resolve(pg_action_scope_operation.name));
}

static void visit_forest(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_term *term = pg_reference(&graph, binder);
	struct scope_shadow left = {binder, NULL}, right = {binder, NULL};
	const struct scope_shadow *initial_shadows[] = {&left, &right};
	const struct scope_shadow *const *shadows = initial_shadows;
	struct scope_visit tail = {.term = term};
	struct scope_visit first = {.term = term, .shadow = &left, .next = &tail};
	struct scope_visit second = first;
	struct scope_visit third = {.term = term, .shadow = &right, .next = &tail};
	struct scope_visit *initial[] = {&first, &second, &tail, NULL, &third, &first};
	struct scope_visit *const *visits = initial;
	struct action_binding binding = {.source = binder, .arguments = {binder, binder, binder}};
	struct action_scope a = {term, term, 1, &binding}, b = a;
	const struct action_scope *owned[] = {&a, &b, &a};
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_scope_visits_write(file, 6, visits, 2, shadows, 3, owned, 1, &term, &codec, NULL));
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		size_t n, m, count, kept;
		struct action_scope *const *restored;
		const struct pg_term *const *roots;
		assert(!pg_scope_visits_read(file, &arena, &graph, 1000, 100, &codec, NULL,
			&n, &visits, &m, &shadows, &kept, &restored, &count, &roots));
		assert(n == 6 && m == 2 && count == 1 && !fclose(file));
		assert(kept == 3 && restored[0] == restored[2] && restored[0] != restored[1]);
		assert(restored[0]->bindings == restored[1]->bindings);
		assert(restored[0]->bindings[0].source == shadows[0]->binder);
		assert(restored[1]->bindings[0].arguments[2] == shadows[1]->binder);
		for (size_t i = 0; i < 3; ++i) owned[i] = restored[i];
		term = roots[0];
		assert(visits[0] == visits[5] && visits[0] != visits[1] && !visits[3]);
		assert(visits[0]->next == visits[2] && visits[1]->next == visits[2] && visits[4]->next == visits[2]);
		assert(visits[0]->shadow == shadows[0] && visits[1]->shadow == shadows[0]);
		assert(visits[4]->shadow == shadows[1] && shadows[0] != shadows[1]);
		assert(shadows[0]->binder == term->as.reference && shadows[1]->binder == term->as.reference);
		for (size_t i = 0; i < n; ++i) if (visits[i]) assert(visits[i]->term == term);
		/* Transport preserves duplicates; only seen membership requires unique keys. */
		struct scope_work work = {0};
		struct scope_visit *seen[] = {visits[0], visits[2], visits[4]};
		assert(!pg_scope_indexes_restore(&work, 3, seen, 0, NULL));
		assert(work.seen.count == 3);
		pg_scope_operation.destroy(&work);
		assert(pg_scope_indexes_restore(&work, 2, visits, 0, NULL) == -1);
		assert(!work.seen.buckets && !work.sources.buckets);
	}
	for (size_t variant = 0; variant < 2; ++variant) {
		FILE *file = tmpfile();
		assert(file && !pg_scope_visits_write(file, 6, visits, 2, shadows, 3, owned, 1, &term, &codec, NULL));
		assert(!fseek(file, variant ? 72 : 24, SEEK_SET));
		assert(!pg_wire_write_u64(file, variant ? 1 : 5));
		rewind(file);
		size_t n, m, count, kept;
		struct action_scope *const *restored;
		struct scope_visit *const *rejected;
		const struct scope_shadow *const *unused;
		const struct pg_term *const *roots;
		assert(pg_scope_visits_read(file, &arena, &graph, 1000, 100, &codec, NULL,
			&n, &rejected, &m, &unused, &kept, &restored, &count, &roots) == -1);
		assert(!n && !rejected && !m && !unused && !kept && !restored && !count && !roots && !fclose(file));
	}
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

static void shadow_forest(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_object *binder = pg_binder(&graph);
	const struct scope_shadow *tail = NULL;
	for (size_t i = 0; i < 2048; ++i) {
		struct scope_shadow *node = pg_alloc(&arena, sizeof(*node));
		assert(node);
		*node = (struct scope_shadow){binder, tail};
		tail = node;
	}
	struct scope_shadow other = {binder, tail->parent};
	const struct scope_shadow *initial[] = {tail, tail, &other, NULL, tail->parent};
	const struct scope_shadow *const *shadows = initial;
	const struct pg_term *term = pg_lambda(&graph, binder, pg_reference(&graph, binder));
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_scope_shadows_write(file, 5, shadows, 0, NULL, 1, &term, &codec, NULL));
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		size_t count, n, kept;
		struct action_scope *const *restored;
		const struct pg_term *const *roots;
		assert(!pg_scope_shadows_read(file, &arena, &graph, 10000, 100, &codec, NULL, &n, &shadows, &kept, &restored, &count, &roots));
		assert(n == 5 && !kept && count == 1 && !fclose(file));
		term = roots[0];
		assert(shadows[0] == shadows[1] && shadows[0] != shadows[2] && !shadows[3]);
		assert(shadows[0]->parent == shadows[4] && shadows[2]->parent == shadows[4]);
		assert(shadows[0]->binder == term->as.lambda.binder && shadows[2]->binder == shadows[0]->binder);
		size_t depth = 0;
		for (tail = shadows[0]; tail; tail = tail->parent) ++depth;
		assert(depth == 2048);
	}
	for (size_t variant = 0; variant < 2; ++variant) {
		FILE *file = tmpfile();
		assert(file && !pg_scope_shadows_write(file, 5, shadows, 0, NULL, 1, &term, &codec, NULL));
		assert(!fseek(file, variant ? 64 : 24, SEEK_SET));
		assert(!pg_wire_write_u64(file, variant ? 1 : 2050));
		rewind(file);
		size_t count, n, kept;
		struct action_scope *const *restored;
		const struct scope_shadow *const *rejected;
		const struct pg_term *const *roots;
		assert(pg_scope_shadows_read(file, &arena, &graph, 10000, 100, &codec, NULL,
			&n, &rejected, &kept, &restored, &count, &roots) == -1);
		assert(!n && !rejected && !kept && !restored && !count && !roots && !fclose(file));
	}
	struct scope_shadow cycle = {.binder = term->as.lambda.binder};
	cycle.parent = &cycle;
	const struct scope_shadow *cyclic = &cycle;
	FILE *file = tmpfile();
	assert(file && pg_scope_shadows_write(file, 1, &cyclic, 0, NULL, 0, NULL, &codec, NULL) == -1);
	assert(!fclose(file));
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

struct analysis_codec {
	struct scope_work *work;
	struct pg_graph *arena;
	const struct action_scope *scopes[4];
};

static int analysis_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct analysis_codec *state = opaque;
	return pg_scope_work_write(file, state->work, 4, state->scopes, count, roots, &codec, NULL);
}

static int analysis_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct analysis_codec *state = opaque;
	size_t scope_count;
	struct action_scope *const *scopes;
	int status = pg_scope_work_read(file, state->arena, graph, limit, name_limit, &codec, NULL,
		&state->work, &scope_count, &scopes, count, roots);
	if (!status) {
		assert(scope_count == 4 && scopes[0] == &state->work->scope && scopes[2] == scopes[0]);
		assert(scopes[1] != scopes[0] && !scopes[3]);
		assert(scopes[1]->bindings == state->work->scope.bindings);
		for (size_t i = 0; i < 4; ++i) state->scopes[i] = scopes[i];
	}
	return status;
}

static void scope_indexes(void)
{
	unsigned phases = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, storage = {0}, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
		const struct pg_term *a = pg_reference(&graph, pg_binder(&graph));
		const struct pg_term *shared = pg_application(&graph, pg_reference(&graph, y), pg_reference(&graph, x));
		const struct pg_term *body = pg_identity_instance(&graph, pg_identity_action(&graph, a),
			pg_lambda(&graph, x, shared), pg_application(&graph, shared, shared));
		const struct pg_term *term = pg_identity_action(&graph,
			pg_lambda(&graph, x, pg_lambda(&graph, y, pg_lambda(&graph, pg_binder(&graph), body))));
		for (size_t i = 0; i < 9; ++i) term = pg_application(&graph, term, pg_reference(&graph, pg_binder(&graph)));
		struct pg_eval baseline, machine;
		pg_computation_eval_init(&baseline, &graph, term);
		assert(pg_eval_advance(&baseline, 10000) == PG_EVAL_WHNF);
		const struct pg_term *expected = pg_eval_readback(&baseline, &graph);
		uint64_t steps = baseline.steps;
		pg_eval_destroy(&baseline);
		pg_computation_eval_init(&machine, &graph, term);
		int done = pg_eval_advance(&machine, cut) == PG_EVAL_WHNF;
		if (machine.task && machine.task->operation == &pg_scope_operation) {
			struct scope_work *work = machine.task->state;
			phases |= 1u << work->phase;
			if (work->phase == SCOPE_SOURCES && !work->position) {
				for (size_t variant = 0; variant < 3; ++variant) {
					FILE *file = tmpfile();
					assert(file && !pg_scope_work_write(file, work, 0, NULL, 0, NULL, &codec, NULL));
					const long offsets[] = {8, 16, 88};
					const uint64_t values[] = {0, SCOPE_READY + 1, work->scope.count};
					assert(!fseek(file, offsets[variant], SEEK_SET) && !pg_wire_write_u64(file, values[variant]));
					rewind(file);
					struct scope_work *rejected;
					size_t n;
					const struct pg_term *const *roots;
					size_t scope_count;
					struct action_scope *const *scopes;
					assert(pg_scope_work_read(file, &arena, &graph, 10000, 100, &codec, NULL,
						&rejected, &scope_count, &scopes, &n, &roots) == -1);
					assert(!rejected && !scope_count && !scopes && !n && !roots && !fclose(file));
				}
			}
			for (size_t round = 0; round < 2; ++round) {
				size_t n = 0, m = 0;
				struct scope_visit **visits = pg_alloc(&storage, work->seen.count * sizeof(*visits));
				struct scope_binding_index **sources = pg_alloc(&storage, work->sources.count * sizeof(*sources));
				assert(visits && sources);
				for (size_t i = 0; i < work->seen.capacity; ++i)
					for (struct pg_index_entry *entry = work->seen.buckets[i]; entry; entry = entry->next)
						visits[n++] = (struct scope_visit *)entry;
				for (size_t i = 0; i < work->sources.capacity; ++i)
					for (struct pg_index_entry *entry = work->sources.buckets[i]; entry; entry = entry->next)
						sources[m++] = (struct scope_binding_index *)entry;
				assert(n == work->seen.count && m == work->sources.count);
				pg_index_destroy(&work->seen);
				pg_index_destroy(&work->sources);
				for (size_t i = 0; i < n; ++i) visits[i]->index.hash = 0;
				for (size_t i = 0; i < m; ++i) sources[i]->index.hash = 0;
				for (struct scope_visit *visit = work->pending; visit; visit = visit->next) visit->index.hash = 0;
				assert(!pg_scope_indexes_restore(work, n, visits, m, sources));
			}
			assert(!machine.frames);
			struct action_scope distinct = work->scope;
			struct analysis_codec state = {work, &arena, {&work->scope, &distinct, &work->scope, NULL}};
			for (size_t round = 0; round < 2; ++round) {
				struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{expected, NULL}, NULL}};
				FILE *file = tmpfile();
				assert(file && !pg_eval_configurations_write_with(file, 2, inputs, analysis_write, &state));
				uint64_t elapsed = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t n;
				const struct pg_eval_configuration *restored;
				assert(!pg_eval_configurations_read_with(file, &graph, 10000, 100,
					&n, &restored, analysis_read, &state));
				assert(n == 2 && !fclose(file));
				expected = restored[1].head.term;
				pg_computation_eval_init(&machine, &graph, restored[0].head.term);
				machine.current = restored[0].head;
				machine.arguments = restored[0].arguments;
				machine.steps = elapsed;
				machine.head_ready = ready;
				assert(!pg_eval_defer(&machine, &pg_scope_operation, state.work));
			}
		}
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF && machine.steps == steps);
		assert(pg_alpha_equal(pg_eval_readback(&machine, &graph), expected) == 1);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&storage);
		pg_graph_destroy(&graph);
		if (done) break;
	}
	assert(phases == 255);
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_term *term = pg_reference(&graph, binder);
	struct scope_shadow shadow = {binder, NULL};
	struct scope_visit first = {.term = term}, second = {.term = term, .shadow = &shadow};
	struct scope_visit *visits[] = {&first, &second};
	struct scope_binding_index source = {.binder = binder};
	struct scope_binding_index *sources[] = {&source, &source};
	struct scope_work work = {.scope = {.count = 1}};
	assert(!pg_scope_indexes_restore(&work, 2, visits, 1, sources));
	assert(work.seen.count == 2 && work.sources.count == 1);
	pg_scope_operation.destroy(&work);
	second.shadow = NULL;
	assert(pg_scope_indexes_restore(&work, 2, visits, 1, sources) == -1);
	assert(!work.seen.buckets && !work.sources.buckets);
	assert(pg_scope_indexes_restore(&work, 1, visits, 2, sources) == -1);
	assert(!work.seen.buckets && !work.sources.buckets);
	pg_graph_destroy(&graph);
}

struct family_codec {
	struct family_scope_work *work;
	struct pg_graph *arena;
	struct pg_classifiers *classifiers;
	struct family_result_work *result;
	const struct pg_term *expected;
};

static int family_owner_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct family_codec *state = opaque;
	struct pg_graph temporary = {0};
	const struct pg_term **all = pg_alloc(&temporary, (count + 1) * sizeof(*all));
	assert(all);
	for (size_t i = 0; i < count; ++i) all[i] = roots[i];
	all[count] = state->expected;
	int status = pg_family_result_write(file, state->result, scope_count, scopes,
		count + 1, all, &pg_builtin_graph_codec, state->classifiers);
	pg_graph_destroy(&temporary);
	return status;
}

static int family_owner_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *scope_count, struct action_scope *const **scopes, size_t *count,
	const struct pg_term *const **roots, void *opaque)
{
	struct family_codec *state = opaque;
	int status = pg_family_result_read(file, state->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, state->classifiers, &state->result, scope_count, scopes, count, roots);
	if (status) return status;
	assert(*count);
	state->expected = (*roots)[--*count];
	return 0;
}

static int family_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct family_codec *state = opaque;
	if (state->result) return pg_family_result_write(file, state->result, 0, NULL,
		count, roots, &pg_builtin_graph_codec, state->classifiers);
	return pg_family_scope_write(file, state->work, 0, NULL, count, roots, &pg_builtin_graph_codec, state->classifiers);
}

static int family_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct family_codec *state = opaque;
	if (state->result) {
		size_t n;
		struct action_scope *const *scopes;
		int status = pg_family_result_read(file, state->arena, graph, limit, name_limit,
			&pg_builtin_graph_codec, state->classifiers, &state->result, &n, &scopes, count, roots);
		assert(status || !n);
		return status;
	}
	size_t n;
	struct action_scope *const *scopes;
	int status = pg_family_scope_read(file, state->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, state->classifiers, &state->work, &n, &scopes, count, roots);
	assert(status || !n);
	return status;
}

static void family_resume(void)
{
	/* Erased work transport only: a free relation is not admitted type evidence. */
	for (size_t mode = 0; mode < 4; ++mode) {
		int function = mode & 1;
		const struct pg_eval_work_operation *operation = function
			? &pg_force_family_scope_operation : &pg_field_family_scope_operation;
		const struct pg_eval_work_operation *result_operation = function
			? &pg_force_family_result_operation : &pg_field_family_result_operation;
		unsigned stages = 0;
		for (uint64_t cut = 0; ; ++cut) {
			struct pg_graph graph, arena = {0};
			struct pg_classifiers classifiers;
			assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
			const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
			const struct pg_term *type = pg_universe(&classifiers, 0);
			const struct pg_term *content = pg_return_type(&classifiers, pg_reference(&graph, x));
			if (function) content = pg_pi(&graph, pg_reference(&graph, x), y, content);
			const struct pg_term *source = pg_lambda(&graph, x, pg_thunk_type(&classifiers, content));
			const struct pg_term *family = pg_identity_action(&graph, source);
			family = pg_application(&graph, family, type);
			family = pg_application(&graph, family, pg_universe(&classifiers, 1));
			family = pg_application(&graph, family, pg_reference(&graph, pg_binder(&graph)));
			const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
			const struct pg_term *body = pg_application(&graph, pg_reference(&graph, &pg_return_operation),
				function ? pg_reference(&graph, y) : value);
			if (function) body = pg_lambda(&graph, y, body);
			const struct pg_term *quote = pg_application(&graph, pg_reference(&graph, &pg_thunk_operation), body);
			const struct pg_term *term = pg_identity_transport(&graph, family, quote, PG_IDENTITY_RIGHT);
			if (function) term = pg_application(&graph,
				pg_application(&graph, pg_reference(&graph, &pg_force_operation), term), value);
			if (mode >= 2) {
				if (!function) term = pg_application(&graph, pg_reference(&graph, &pg_force_operation), term);
				const struct pg_object *z = pg_binder(&graph);
				const struct pg_term *k = pg_lambda(&graph, z,
					pg_application(&graph, pg_reference(&graph, &pg_return_operation), value));
				term = pg_application(&graph, pg_application(&graph, pg_reference(&graph, &pg_fold_operation), term), k);
			}
			struct pg_eval baseline;
			pg_computation_eval_init(&baseline, &graph, term);
			assert(pg_eval_advance(&baseline, 10000) == PG_EVAL_WHNF);
			const struct pg_term *expected = pg_eval_readback(&baseline, &graph);
			uint64_t steps = baseline.steps;
			pg_eval_destroy(&baseline);
			struct pg_eval machine;
			pg_computation_eval_init(&machine, &graph, term);
			int finished = pg_eval_advance(&machine, cut) == PG_EVAL_WHNF;
			if (machine.task && (mode < 2 || machine.frames) && (machine.task->operation == result_operation ||
				(mode < 2 && machine.task->operation == operation))) {
				assert((machine.frames != NULL) == (mode >= 2));
				const struct pg_eval_work_operation *active = machine.task->operation;
				struct family_codec state = {.arena = &arena, .classifiers = &classifiers, .expected = expected};
				if (active == result_operation) {
					state.result = machine.task->state;
					stages |= 1u << (2 + state.result->phase);
				} else {
					state.work = machine.task->state;
					stages |= state.work->scope.source ? 2u : 1u;
				}
				for (size_t save = 0; save < 2; ++save) {
					struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{expected, NULL}, NULL}};
					FILE *file = tmpfile();
					assert(file);
					size_t length = strlen(active->name);
					assert(!pg_wire_write_u64(file, length) && fwrite(active->name, 1, length, file) == length);
					if (mode >= 2) {
						struct pg_eval_configuration captured = {machine.frames->caller, machine.frames->arguments};
						assert(!pg_computation_frames_write_with(file, machine.frames, inputs, 1, &captured, family_owner_write, &state));
					}
					else assert(!pg_eval_configurations_write_with(file, 2, inputs, family_write, &state));
					uint64_t elapsed = machine.steps;
					int ready = machine.head_ready;
					pg_eval_destroy(&machine);
					pg_classifiers_destroy(&classifiers);
					pg_graph_destroy(&arena);
					pg_graph_destroy(&graph);
					assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
					rewind(file);
					uint64_t name_length;
					char task_name[100];
					assert(!pg_wire_read_u64(file, &name_length) && name_length < sizeof(task_name));
					assert(fread(task_name, 1, (size_t)name_length, file) == name_length);
					task_name[name_length] = 0;
					const struct pg_eval_work_operation *restored_operation = pg_computation_work_resolve(task_name);
					assert(restored_operation == active);
					size_t count;
					struct pg_eval_configuration current;
					const struct pg_eval_configuration *restored = &current;
					struct pg_eval_frame *frames = NULL;
					if (mode >= 2) {
						const struct pg_eval_configuration *captured;
						assert(!pg_computation_frames_read_with(file, &arena, &graph, 10000, 100,
							family_owner_read, &state, &frames, &current, 1, &captured));
						assert(captured->head.term == frames->caller.term);
						assert(captured->head.environment == frames->caller.environment);
						assert(captured->arguments == frames->arguments);
						expected = state.expected;
					} else {
						assert(!pg_eval_configurations_read_with(file, &graph, 10000, 100,
							&count, &restored, family_read, &state));
						assert(count == 2);
						expected = restored[1].head.term;
					}
					assert(!fclose(file));
					pg_computation_eval_init(&machine, &graph, restored[0].head.term);
					machine.current = restored[0].head;
					machine.arguments = restored[0].arguments;
					machine.frames = frames;
					machine.steps = elapsed;
					machine.head_ready = ready;
					assert(!pg_eval_defer(&machine, restored_operation, state.result ? (void *)state.result : state.work));
				}
			}
			machine_resave(&machine, &graph, &classifiers, &expected);
			assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF && machine.steps == steps);
			assert(pg_alpha_equal(pg_eval_readback(&machine, &graph), expected) == 1);
			pg_eval_destroy(&machine);
			pg_classifiers_destroy(&classifiers);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
			if (finished) break;
		}
		assert(stages == (mode >= 2 ? 28 : 31));
	}
}

static void family_discovery(void)
{
	const struct pg_eval_work_operation *operations[] = {
		&pg_field_family_scope_operation, &pg_force_family_scope_operation
	};
	for (size_t mode = 0; mode < 2; ++mode) {
		struct pg_graph graph, arena = {0};
		struct pg_classifiers classifiers;
		assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
		const struct pg_term *content = pg_return_type(&classifiers, pg_universe(&classifiers, 0));
		const struct pg_term *body = pg_thunk_type(&classifiers, content);
		const struct pg_term *source = pg_lambda(&graph, pg_binder(&graph),
			pg_lambda(&graph, pg_binder(&graph), body));
		const struct pg_term *family = pg_identity_action(&graph, source);
		for (size_t i = 0; i < 6; ++i) family = pg_application(&graph, family, content);
		const struct pg_term *extra[] = {family, source, body, content};
		const struct pg_term *const *roots = extra;
		struct family_scope_work initial = {.cursor = family, .value = mode ? content : NULL};
		struct family_scope_work *work = &initial;
		size_t polls = 0;
		int done = 0;
		while (!done) {
			for (size_t save = 0; save < 2; ++save) {
				FILE *file = tmpfile();
				struct action_scope distinct = {.source = roots[1], .body = roots[2]};
				const struct action_scope *inputs[] = {&work->scope, &distinct, &work->scope, NULL};
				assert(file && !pg_family_scope_write(file, work, 4, inputs, 4, roots, &pg_builtin_graph_codec, &classifiers));
				pg_classifiers_destroy(&classifiers);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
				rewind(file);
				size_t count;
				size_t scope_count;
				struct action_scope *const *scopes;
				assert(!pg_family_scope_read(file, &arena, &graph, 1000, 100,
					&pg_builtin_graph_codec, &classifiers, &work, &scope_count, &scopes, &count, &roots));
				assert(!fclose(file) && count == 4 && !work->scope.bindings);
				assert(scope_count == 4 && scopes[0] == &work->scope && scopes[2] == scopes[0]);
				assert(scopes[1] != scopes[0] && !scopes[3]);
				assert(scopes[1]->source == roots[1] && scopes[1]->body == roots[2]);
				assert(work->value == (mode ? roots[3] : NULL));
				if (work->scope.source) assert(work->scope.source == roots[1]);
			}
			done = operations[mode]->poll(work);
			assert(done >= 0 && ++polls <= 10);
		}
		assert(polls == 10 && work->supplied == 6 && work->scope.count == 2);
		assert(work->scope.body == roots[2] && work->content == roots[3]);
		/* Invalid supplied arity must not publish a partially restored work handle. */
		FILE *file = tmpfile();
		assert(file && !pg_family_scope_write(file, work, 0, NULL, 4, roots, &pg_builtin_graph_codec, &classifiers));
		assert(!fseek(file, 8, SEEK_SET) && !pg_wire_write_u64(file, 5));
		rewind(file);
		size_t count = 1;
		size_t scope_count;
		struct action_scope *const *scopes;
		assert(pg_family_scope_read(file, &arena, &graph, 1000, 100,
			&pg_builtin_graph_codec, &classifiers, &work, &scope_count, &scopes, &count, &roots) == -1);
		assert(!work && !count && !roots && !scope_count && !scopes && !fclose(file));
		pg_classifiers_destroy(&classifiers);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
}

static void family_result_failure(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_term *term = pg_reference(&graph, pg_binder(&graph));
	struct family_result_work input = {.closure = {.graph = &graph, .result = term},
		.family = term, .phase = FAMILY_APPLY};
	/* Empty retained arrays and terminal work still have a representable state. */
	for (size_t variant = 0; variant < 4; ++variant) {
		FILE *file = tmpfile();
		assert(file && !pg_family_result_write(file, &input, 0, NULL, 1, &term, &codec, NULL));
		if (variant) {
			assert(!fseek(file, (long)(8 * variant), SEEK_SET));
			assert(!pg_wire_write_u64(file, variant == 3 ? 3 : 1));
		}
		rewind(file);
		struct family_result_work *work;
		struct action_scope *const *scopes;
		const struct pg_term *const *roots;
		size_t count, scope_count;
		int status = pg_family_result_read(file, &arena, &graph, 1000, 100, &codec, NULL,
			&work, &scope_count, &scopes, &count, &roots);
		if (variant) {
			assert(status == -1 && !work && !scope_count && !scopes && !count && !roots);
		} else {
			assert(!status && !scope_count && count == 1);
			assert(work->family == roots[0] && work->closure.result == roots[0]);
			assert(pg_field_family_result_operation.poll(work) == 1);
		}
		assert(!fclose(file));
	}
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

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
	struct action_result_work initial_result = {&graph, &binding, term, 0, 0};
	struct action_result_work *result = &initial_result;
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
		assert(file && !pg_computation_frames_write(file, frames, result, &current, &codec, NULL));
		pg_materialize_destroy(&frames->answer);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		assert(!pg_computation_frames_read(file, &arena, &graph, 1000, 100, &codec, NULL, &frames, &result, &current));
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
		assert(result && result->bindings == shared->bindings && result->result == current.head.term);
		/* Unknown names cannot select arbitrary host callbacks. */
		assert(!fseek(file, 24, SEEK_SET) && fputc('?', file) != EOF);
		rewind(file);
		struct pg_eval_frame *rejected;
		struct action_result_work *rejected_result;
		struct pg_eval_configuration empty;
		assert(pg_computation_frames_read(file, &arena, &graph, 1000, 100, &codec, NULL, &rejected, &rejected_result, &empty));
		assert(!rejected && !rejected_result && !empty.head.term && !empty.arguments);
		assert(!fclose(file));
	}
	FILE *bad = tmpfile();
	assert(bad);
	frames->state = frames->parent->parent->parent->parent->parent->state;
	assert(pg_computation_frames_write(bad, frames, result, &current, &codec, NULL));
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
				assert(file && !pg_computation_frames_write(file, machine.frames, NULL, &current, &pg_builtin_graph_codec, NULL));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				assert(!machine.task);
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				struct pg_eval_frame *frames;
				struct action_result_work *result_work;
				assert(!pg_computation_frames_read(file, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &frames, &result_work, &current));
				assert(!result_work);
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

struct task_scope_codec {
	struct pg_graph *arena;
	struct action_scope *scope;
	int omit_roots;
};

static int task_scope_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct task_scope_codec *context = opaque;
	return pg_action_scope_write(file, context->scope, count, roots, &pg_builtin_graph_codec, NULL);
}

static int task_scope_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct task_scope_codec *context = opaque;
	int status = pg_action_scope_read(file, context->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, NULL, &context->scope, count, roots);
	if (!status && context->omit_roots) *count = 0;
	return status;
}

struct fold_codec {
	struct task_scope_codec scope;
	struct fold_work *work;
};

static int write_fold(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct fold_codec *context = opaque;
	return pg_fold_work_write_with(file, context->work, count, roots, task_scope_write, &context->scope);
}

static int read_fold(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct fold_codec *context = opaque;
	return pg_fold_work_read_with(file, context->scope.arena, graph, limit, name_limit,
		task_scope_read, &context->scope, &context->work, count, roots);
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
				struct fold_codec context = {.scope.arena = &arena, .work = machine.task->state};
				struct action_scope scope = {.source = context.work->head, .body = context.work->payload};
				struct action_binding binding = {0};
				if (context.work->phase != FOLD_BINDERS || context.work->position) {
					binding.source = context.work->binders[0];
					scope.count = 1;
					scope.bindings = &binding;
				}
				context.scope.scope = &scope;
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
					struct fold_codec bad = {.scope = {.arena = &arena, .omit_roots = 1}};
					size_t rejected_count;
					const struct pg_eval_configuration *rejected_roots;
					rewind(file);
					assert(pg_eval_configurations_read_with(file, &graph, 10000, 100,
						&rejected_count, &rejected_roots, read_fold, &bad));
					assert(!bad.work && !rejected_count && !rejected_roots);
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
					assert(context.scope.scope->source == context.work->head);
					assert(context.scope.scope->body == context.work->payload);
					if (context.scope.scope->count) assert(context.scope.scope->bindings[0].source == context.work->binders[0]);
					expected = restored[1].head.term;
					pg_computation_eval_init(&machine, &graph, restored[0].head.term);
					machine.current = restored[0].head;
					machine.arguments = restored[0].arguments;
					machine.steps = steps;
					machine.head_ready = ready;
					assert(!pg_eval_defer(&machine, &pg_fold_work_operation, context.work));
				}
			}
			machine_resave(&machine, &graph, NULL, &expected);
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

struct symmetry_codec {
	struct task_scope_codec scope;
	struct composition_work *work;
};

static int write_symmetry(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct symmetry_codec *context = opaque;
	return pg_symmetry_work_write_with(file, context->work, count, roots, task_scope_write, &context->scope);
}

static int read_symmetry(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct symmetry_codec *context = opaque;
	return pg_symmetry_work_read_with(file, context->scope.arena, graph, limit, name_limit,
		task_scope_read, &context->scope, &context->work, count, roots);
}

static void symmetry_progress(void)
{
	uint64_t total = 0;
	unsigned positions = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		size_t axes[] = {2, 1, 0};
		const struct pg_term *swap = pg_reference(&graph, pg_symmetry_restore(&graph, 3, axes));
		const struct pg_term *term = pg_application(&graph, swap, pg_application(&graph, swap, value));
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph, term);
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.task) {
			assert(machine.task->operation == &pg_symmetry_composition_operation && !machine.frames);
			struct action_scope scope = {.source = swap, .body = value};
			struct symmetry_codec context = {.scope = {.arena = &arena, .scope = &scope}, .work = machine.task->state};
			positions |= 1u << context.work->position;
			if (context.work->position == 1) {
				const long offsets[] = {8, 16, 24};
				const uint64_t invalid[] = {4, 4, 3};
				for (size_t i = 0; i < 3; ++i) {
					FILE *bad = tmpfile();
					assert(bad && !pg_symmetry_work_write(bad, context.work, 0, NULL, &pg_builtin_graph_codec, NULL));
					assert(!fseek(bad, offsets[i], SEEK_SET) && !pg_wire_write_u64(bad, invalid[i]));
					rewind(bad);
					struct composition_work *rejected;
					size_t n;
					const struct pg_term *const *unused;
					assert(pg_symmetry_work_read(bad, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
						&rejected, &n, &unused));
					assert(!rejected && !n && !unused && !fclose(bad));
				}
			}
			for (unsigned round = 0; round < 2; ++round) {
				struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{value, NULL}, NULL}};
				FILE *file = tmpfile();
				assert(file && !pg_eval_configurations_write_with(file, 2, inputs, write_symmetry, &context));
				struct symmetry_codec bad = {.scope = {.arena = &arena, .omit_roots = 1}};
				size_t rejected_count;
				const struct pg_eval_configuration *rejected_roots;
				rewind(file);
				assert(pg_eval_configurations_read_with(file, &graph, 10000, 100,
					&rejected_count, &rejected_roots, read_symmetry, &bad));
				assert(!bad.work && !rejected_count && !rejected_roots);
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t n;
				const struct pg_eval_configuration *restored;
				assert(!pg_eval_configurations_read_with(file, &graph, 10000, 100, &n, &restored, read_symmetry, &context));
				assert(n == 2 && !fclose(file));
				assert(context.work->outer == context.work->inner);
				assert(&context.work->outer->base.object == restored[0].head.term->as.reference);
				value = restored[1].head.term;
				assert(context.work->argument == value);
				assert(context.scope.scope->body == value);
				assert(context.scope.scope->source->as.reference == &context.work->outer->base.object);
				for (size_t i = 0; i < context.work->position; ++i) assert(context.work->axes[i] == i);
				pg_computation_eval_init(&machine, &graph, restored[0].head.term);
				machine.current = restored[0].head;
				machine.arguments = restored[0].arguments;
				machine.steps = steps;
				machine.head_ready = ready;
				assert(!pg_eval_defer(&machine, &pg_symmetry_composition_operation, context.work));
			}
		}
		machine_resave(&machine, &graph, NULL, &value);
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == value);
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
	assert(positions == 15);
}

struct work_frames {
	struct pg_graph *arena;
	struct pg_eval_frame *frames;
	struct pg_eval_configuration current;
};

static int work_scopes_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, void *unused)
{
	(void)unused;
	return pg_action_scopes_write(file, scope_count, scopes, count, roots, &pg_builtin_graph_codec, NULL);
}

static int work_scopes_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *scope_count, struct action_scope *const **scopes, size_t *count,
	const struct pg_term *const **roots, void *opaque)
{
	struct work_frames *context = opaque;
	return pg_action_scopes_read(file, context->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, NULL, scope_count, scopes, count, roots);
}

static int work_frames_write(FILE *file, size_t count, const struct pg_eval_configuration *roots, void *opaque)
{
	struct work_frames *context = opaque;
	if (pg_wire_write_u64(file, count)) return -1;
	return pg_computation_frames_write_with(file, context->frames, &context->current,
		count, roots, work_scopes_write, context);
}

static int work_frames_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_eval_configuration **roots, void *opaque)
{
	struct work_frames *context = opaque;
	uint64_t n;
	if (pg_wire_read_u64(file, &n) || n > limit) return -1;
	if (pg_computation_frames_read_with(file, context->arena, graph, limit, name_limit,
		work_scopes_read, context, &context->frames, &context->current, (size_t)n, roots)) return -1;
	*count = (size_t)n;
	return 0;
}

struct machine_image_test {
	struct work_frames stack;
	const struct pg_term *expected;
	int framed;
};

static int machine_config_write(FILE *file, size_t count, const struct pg_eval_configuration *roots, void *opaque)
{
	struct machine_image_test *context = opaque;
	if (context->framed) return work_frames_write(file, count, roots, &context->stack);
	return pg_eval_configurations_write(file, count, roots, &pg_builtin_graph_codec, NULL);
}

static int machine_config_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_eval_configuration **roots, void *opaque)
{
	struct machine_image_test *context = opaque;
	if (context->framed) return work_frames_read(file, graph, limit, name_limit, count, roots, &context->stack);
	return pg_eval_configurations_read(file, graph, limit, name_limit, &pg_builtin_graph_codec, NULL, count, roots);
}

static int machine_payload_write(FILE *file, const struct pg_eval *machine, void *opaque)
{
	struct machine_image_test *context = opaque;
	context->framed = machine->frames != NULL;
	context->stack.frames = machine->frames;
	context->stack.current = (struct pg_eval_configuration){machine->current, machine->arguments};
	struct pg_eval_configuration roots[] = {context->stack.current, {{context->expected, NULL}, NULL}};
	if (!machine->task) return machine_config_write(file, 2, roots, context);
	assert(machine->task->operation == &pg_action_scope_operation);
	return pg_action_scope_work_write_with(file, machine->task->state, 2, roots, machine_config_write, context);
}

static int machine_payload_read(FILE *file, struct pg_eval *machine,
	const struct pg_eval_work_operation *operation, int frames, void *opaque)
{
	struct machine_image_test *context = opaque;
	context->framed = frames;
	context->stack = (struct work_frames){.arena = &machine->temporary};
	size_t count;
	const struct pg_eval_configuration *roots;
	struct action_scope_work *work = NULL;
	int status;
	if (operation) {
		assert(operation == &pg_action_scope_operation);
		status = pg_action_scope_work_read_with(file, &machine->temporary, machine->output, 10000, 100,
			machine_config_read, context, &work, &count, &roots);
	} else status = machine_config_read(file, machine->output, 10000, 100, &count, &roots, context);
	/* Attach first, so the envelope owns cleanup even if an inner task fails. */
	machine->frames = context->stack.frames;
	if (status || count != 2) return -1;
	machine->current = roots[0].head;
	machine->arguments = roots[0].arguments;
	context->expected = roots[1].head.term;
	return operation ? pg_eval_defer(machine, operation, work) : 0;
}

static void machine_envelope(void)
{
	const struct pg_eval_policy *policies[] = {&pg_beta_policy, &pg_pure_policy};
	unsigned seen = 0;
	for (size_t p = 0; p < 2; ++p) {
		uint64_t total = 0;
		for (uint64_t cut = 0; ; ++cut) {
			struct pg_graph graph;
			assert(!pg_graph_init(&graph));
			const struct pg_object *x = pg_binder(&graph);
			const struct pg_term *expected = pg_reference(&graph, pg_binder(&graph));
			const struct pg_term *value = pg_application(&graph, pg_reference(&graph, &pg_thunk_operation), expected);
			const struct pg_term *term = pg_identity_action(&graph, pg_lambda(&graph, x, pg_reference(&graph, x)));
			for (size_t i = 0; i < 3; ++i) term = pg_application(&graph, term, value);
			term = pg_application(&graph, pg_reference(&graph, &pg_force_operation), term);
			if (!p) expected = term;
			struct pg_eval machine;
			pg_eval_init(&machine, term);
			machine.output = &graph; machine.dispatch = policies[p]->dispatch;
			int finished = pg_eval_advance(&machine, cut) == PG_EVAL_WHNF;
			if (machine.task && machine.task->operation != &pg_action_scope_operation) {
				pg_eval_destroy(&machine); pg_graph_destroy(&graph); continue;
			}
			seen |= finished ? 1u : 0;
			seen |= machine.head_ready ? 2u : 0;
			seen |= machine.frames ? 4u : 0;
			seen |= machine.task ? 8u : 0;
			struct machine_image_test context = {.expected = expected};
			for (size_t round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				uint64_t steps = machine.steps;
				enum pg_eval_status status = machine.status;
				int ready = machine.head_ready;
				assert(file && !pg_computation_machine_write_with(file, &machine, policies[p], machine_payload_write, &context));
				pg_eval_destroy(&machine); pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				const struct pg_eval_policy *policy;
				assert(!pg_computation_machine_read_with(file, &machine, &graph, 100, machine_payload_read, &context, &policy));
				assert(policy == policies[p] && machine.steps == steps && machine.status == status && machine.head_ready == ready);
				if (!cut) {
					assert(!fseek(file, 16, SEEK_SET) && !pg_wire_write_u64(file, 3));
					rewind(file);
					struct pg_eval rejected;
					assert(pg_computation_machine_read_with(file, &rejected, &graph, 100, machine_payload_read, &context, &policy));
					assert(!policy && rejected.status == PG_EVAL_ERROR && !rejected.current.term && !rejected.task && !rejected.frames);
					pg_eval_destroy(&rejected);
				}
				if (machine.frames) {
					assert(!fseek(file, 16, SEEK_SET) && !pg_wire_write_u64(file, PG_EVAL_WHNF));
					rewind(file);
					struct pg_eval rejected;
					struct machine_image_test bad = {0};
					assert(pg_computation_machine_read_with(file, &rejected, &graph, 100,
						machine_payload_read, &bad, &policy));
					/* The payload was restored, but a live frame is not WHNF. */
					assert(bad.expected && !policy && !rejected.task && !rejected.frames);
					assert(rejected.status == PG_EVAL_ERROR && !rejected.current.term);
					pg_eval_destroy(&rejected);
				}
				assert(!fclose(file));
			}
			assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
			assert(pg_eval_readback(&machine, &graph) == context.expected);
			if (!cut) total = machine.steps;
			assert(machine.steps == total && cut < 10000);
			pg_eval_destroy(&machine); pg_graph_destroy(&graph);
			if (finished) break;
		}
	}
	assert(seen == 15);
}

static void prefix_progress(int framed)
{
	uint64_t total = 0;
	unsigned positions = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		if (framed) value = pg_application(&graph, pg_reference(&graph, &pg_thunk_operation), value);
		const struct pg_object *x = pg_binder(&graph);
		size_t outer_axes[] = {0, 2, 1}, inner_axes[] = {1, 0};
		const struct pg_term *outer = pg_reference(&graph, pg_symmetry_restore(&graph, 3, outer_axes));
		const struct pg_term *inner = pg_reference(&graph, pg_symmetry_restore(&graph, 2, inner_axes));
		const struct pg_term *body = pg_application(&graph, outer,
			pg_application(&graph, inner, pg_reference(&graph, x)));
		struct pg_eval machine;
		if (framed) body = pg_application(&graph, pg_reference(&graph, &pg_force_operation), body);
		const struct pg_term *input = pg_application(&graph, pg_lambda(&graph, x, body), value);
		pg_computation_eval_init(&machine, &graph, input);
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.task && machine.task->operation == &pg_symmetry_prefix_operation) {
			assert((machine.frames != NULL) == framed);
			struct prefix_work *work = machine.task->state;
			if (framed) assert(work->argument.environment == machine.frames->arguments->value.environment);
			positions |= 1u << work->position;
			for (unsigned round = 0; round < 2; ++round) {
				struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{value, NULL}, NULL}};
				struct work_frames context = {.arena = &arena, .frames = machine.frames, .current = inputs[0]};
				FILE *file = tmpfile();
				assert(file);
				if (framed) assert(!pg_symmetry_prefix_write_with(file, work, 2, inputs, work_frames_write, &context));
				else assert(!pg_symmetry_prefix_write(file, work, 2, inputs, &pg_builtin_graph_codec, NULL));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t n;
				const struct pg_eval_configuration *restored;
				context.frames = NULL;
				if (framed) {
					assert(!pg_symmetry_prefix_read_with(file, &arena, &graph, 10000, 100,
						work_frames_read, &context, &work, &n, &restored));
					assert(context.frames && context.current.head.term == restored[0].head.term);
					assert(context.current.arguments == restored[0].arguments);
					assert(context.current.head.environment == restored[0].head.environment);
					assert(work->argument.environment == context.frames->arguments->value.environment);
				} else assert(!pg_symmetry_prefix_read(file, &arena, &graph, 10000, 100,
					&pg_builtin_graph_codec, NULL, &work, &n, &restored));
				assert(n == 2);
				assert(work->argument.environment == restored[0].arguments->value.environment);
				value = restored[1].head.term;
				assert(work->argument.environment->value.term == value);
				assert(work->argument.term == restored[0].arguments->value.term);
				assert(&work->outer->base.object == restored[0].head.term->as.reference);
				for (size_t i = 0; i < work->position; ++i) assert(work->axes[i] == 1 - i);
				/* A plausible allocation bound must still match its owner. */
				assert(!fseek(file, 8, SEEK_SET) && !pg_wire_write_u64(file, 3));
				rewind(file);
				struct prefix_work *rejected;
				const struct pg_eval_configuration *unused;
				if (framed) {
					struct work_frames bad = {.arena = &arena};
					assert(pg_symmetry_prefix_read_with(file, &arena, &graph, 10000, 100,
						work_frames_read, &bad, &rejected, &n, &unused));
					assert(bad.frames);
					pg_materialize_destroy(&bad.frames->answer);
				} else assert(pg_symmetry_prefix_read(file, &arena, &graph, 10000, 100,
					&pg_builtin_graph_codec, NULL, &rejected, &n, &unused));
				assert(!rejected && !n && !unused && !fclose(file));
				pg_computation_eval_init(&machine, &graph, restored[0].head.term);
				machine.current = restored[0].head;
				machine.arguments = restored[0].arguments;
				machine.steps = steps;
				machine.head_ready = ready;
				machine.frames = context.frames;
				assert(!pg_eval_defer(&machine, &pg_symmetry_prefix_operation, work));
			}
		}
		machine_resave(&machine, &graph, NULL, &value);
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == (framed ? value->as.application.argument : value));
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
	assert(positions == 7);
}

struct higher_codec {
	struct pg_graph *arena;
	struct higher_scope_work *work;
	struct action_scope *scopes[4];
};

static int write_higher(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct higher_codec *context = opaque;
	return pg_higher_scope_write(file, context->work, 4, (const struct action_scope *const *)context->scopes,
		count, roots, &pg_builtin_graph_codec, NULL);
}

static int read_higher(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct higher_codec *context = opaque;
	size_t scope_count;
	struct action_scope *const *scopes;
	if (pg_higher_scope_read(file, context->arena, graph, limit, name_limit,
		&pg_builtin_graph_codec, NULL, &context->work, &scope_count, &scopes, count, roots)) return -1;
	if (scope_count != 4) return -1;
	for (size_t i = 0; i < 4; ++i) context->scopes[i] = scopes[i];
	return 0;
}

static void higher_progress(void)
{
	uint64_t total = 0;
	unsigned phases = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		const struct pg_object *x = pg_binder(&graph);
		const struct pg_term *term = pg_identity_action(&graph,
			pg_identity_action(&graph, pg_lambda(&graph, x, pg_reference(&graph, x))));
		for (size_t i = 0; i < 9; ++i) term = pg_application(&graph, term, value);
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph, term);
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.task && machine.task->operation == &pg_higher_scope_operation) {
			assert(!machine.frames);
			struct higher_codec context = {.arena = &arena, .work = machine.task->state};
			struct action_scope scope = {.source = context.work->source, .body = context.work->cursor};
			struct action_binding binding = {0};
			size_t kept = context.work->wrapping ? context.work->arity : context.work->position;
			if (kept) {
				binding.source = context.work->binders[0];
				scope.count = 1;
				scope.bindings = &binding;
			}
			struct action_scope distinct = scope;
			context.scopes[0] = context.scopes[2] = &scope;
			context.scopes[1] = &distinct;
			unsigned phase = !context.work->body ? (unsigned)context.work->collecting : 2u + (unsigned)context.work->wrapping;
			phases |= 1u << phase;
			if (!phase && context.work->arity == 1) {
				const long offsets[] = {8, 16, 32};
				const uint64_t invalid[] = {0, UINT64_MAX, 8};
				for (size_t i = 0; i < 3; ++i) {
					FILE *bad = tmpfile();
					assert(bad && !pg_higher_scope_write(bad, context.work, 0, NULL, 0, NULL, &pg_builtin_graph_codec, NULL));
					assert(!fseek(bad, offsets[i], SEEK_SET) && !pg_wire_write_u64(bad, invalid[i]));
					rewind(bad);
					struct higher_scope_work *rejected;
					size_t n;
					size_t scope_count;
					struct action_scope *const *scopes;
					const struct pg_term *const *unused;
					assert(pg_higher_scope_read(bad, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
						&rejected, &scope_count, &scopes, &n, &unused));
					assert(!rejected && !scope_count && !scopes && !n && !unused && !fclose(bad));
				}
			}
			for (unsigned round = 0; round < 2; ++round) {
				struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{value, NULL}, NULL}};
				FILE *file = tmpfile();
				assert(file && !pg_eval_configurations_write_with(file, 2, inputs, write_higher, &context));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t n;
				const struct pg_eval_configuration *restored;
				assert(!pg_eval_configurations_read_with(file, &graph, 10000, 100, &n, &restored, read_higher, &context));
				assert(n == 2 && !fclose(file));
				assert(context.work->source == restored[0].arguments->value.term);
				assert(context.scopes[0] == context.scopes[2] && !context.scopes[3]);
				assert(context.scopes[0] != context.scopes[1]);
				assert(context.scopes[0]->source == context.work->source);
				assert(context.scopes[0]->body == context.work->cursor);
				assert(context.scopes[0]->bindings == context.scopes[1]->bindings);
				if (kept) assert(context.scopes[0]->bindings[0].source == context.work->binders[0]);
				value = restored[1].head.term;
				pg_computation_eval_init(&machine, &graph, restored[0].head.term);
				machine.current = restored[0].head;
				machine.arguments = restored[0].arguments;
				machine.steps = steps;
				machine.head_ready = ready;
				assert(!pg_eval_defer(&machine, &pg_higher_scope_operation, context.work));
			}
		}
		machine_resave(&machine, &graph, NULL, &value);
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == value);
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
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

static void comparison_scope_owner(void)
{
	for (int different = 0; different < 2; ++different) {
		uint64_t total = 0;
		for (uint64_t cut = 0; ; ++cut) {
			struct pg_graph graph, arena = {0};
			assert(!pg_graph_init(&graph));
			const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
			const struct pg_term *left = pg_lambda(&graph, x, pg_reference(&graph, x));
			const struct pg_term *right = pg_lambda(&graph, y,
				pg_reference(&graph, different ? pg_binder(&graph) : y));
			struct action_binding binding = {.source = x, .arguments = {y, x, y}};
			struct action_scope scope = {left, right, 1, &binding};
			struct scope_frame_codec context = {.arena = &arena, .scope = &scope};
			struct pg_comparison work;
			assert(!pg_comparison_init(&work, left, right, NULL, NULL));
			int finished = pg_comparison_advance(&work, cut) != PG_COMPARISON_PENDING;
			for (size_t round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				const struct pg_term *roots[] = {context.scope->source, context.scope->body};
				uint64_t steps = pg_comparison_steps(&work);
				enum pg_comparison_status status = pg_comparison_status(&work);
				assert(file && !pg_comparison_write_with(file, &work, 2, roots, write_scope_terms, &context));
				pg_comparison_destroy(&work); pg_graph_destroy(&arena); pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t count;
				const struct pg_term *const *restored;
				assert(!pg_comparison_read_with(file, &graph, 1000, 100, read_scope_terms, &context, &work, &count, &restored));
				assert(count == 2 && restored[0] == context.scope->source && restored[1] == context.scope->body);
				assert(restored[0]->as.lambda.binder == context.scope->bindings[0].source);
				assert(restored[1]->as.lambda.binder == context.scope->bindings[0].arguments[0]);
				assert(context.scope->bindings[0].arguments[0] == context.scope->bindings[0].arguments[2]);
				assert(pg_comparison_status(&work) == status && pg_comparison_steps(&work) == steps);
				rewind(file);
				struct scope_frame_codec bad = {.arena = &arena, .omit_root = 1};
				struct pg_comparison rejected;
				assert(pg_comparison_read_with(file, &graph, 1000, 100, read_scope_terms, &bad, &rejected, &count, &restored));
				assert(bad.scope && !rejected.state && !count && !restored);
				pg_comparison_destroy(&rejected);
				assert(!fclose(file));
			}
			assert(pg_comparison_advance(&work, 1000) == (different ? PG_COMPARISON_DIFFERENT : PG_COMPARISON_EQUAL));
			if (!cut) total = pg_comparison_steps(&work);
			assert(pg_comparison_steps(&work) == total && cut < 1000);
			pg_comparison_destroy(&work); pg_graph_destroy(&arena); pg_graph_destroy(&graph);
			if (finished) break;
		}
	}
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
		struct pg_environment distinct = environment;
		struct pg_argument tail = {{body, &environment}, NULL};
		struct pg_argument first = {{source, &environment}, &tail};
		struct pg_eval_configuration owned[] = {
			{{body, &environment}, &first}, {{source, &environment}, &tail},
			{{body, &distinct}, &first}
		};
		const struct pg_eval_configuration *extra = owned;
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			struct pg_eval_configuration current = {machine.current, machine.arguments};
			assert(file && !pg_eval_frames_payload_write_with(file, machine.frames, &current, 3, extra, write_scope_terms, &context));
			uint64_t steps = machine.steps;
			int ready = machine.head_ready;
			pg_eval_destroy(&machine);
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
			assert(!pg_graph_init(&graph));
			rewind(file);
			struct pg_eval_frame *frames;
			assert(!pg_eval_frames_payload_read_with(file, &arena, &graph, 10000, 100,
				read_scope_terms, &context, &frames, &current, 3, &extra));
			assert(extra[0].head.environment == frames->caller.environment);
			assert(extra[1].head.environment == frames->caller.environment);
			assert(extra[2].head.environment != frames->caller.environment);
			assert(extra[2].head.environment->binder == frames->caller.environment->binder);
			assert(extra[0].arguments == extra[2].arguments);
			assert(extra[0].arguments->next == extra[1].arguments);
			assert(extra[1].arguments->value.environment == frames->caller.environment);
			assert(extra[0].head.term == context.scope->body);
			/* Failure outside a successfully restored owner must not publish
			 * a partially connected frame. Scope storage is arena-owned. */
			rewind(file);
			struct scope_frame_codec bad = {.arena = &arena, .omit_root = 1};
			struct pg_eval_frame *rejected;
			struct pg_eval_configuration unused;
			const struct pg_eval_configuration *unused_extra;
			assert(pg_eval_frames_payload_read_with(file, &arena, &graph, 10000, 100,
				read_scope_terms, &bad, &rejected, &unused, 3, &unused_extra));
			assert(bad.scope && !rejected && !unused.head.term && !unused.arguments && !unused_extra);
			/* The frame-only reader must not silently discard task roots. */
			rewind(file);
			bad.omit_root = 0;
			assert(pg_eval_frames_payload_read_with(file, &arena, &graph, 10000, 100,
				read_scope_terms, &bad, &rejected, &unused, 0, &unused_extra));
			assert(!rejected && !unused.head.term && !unused_extra);
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
		assert(!fseek(file, 40, SEEK_SET) && !pg_wire_write_u64(file, 1001));
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

static void result_ownership(void)
{
	struct pg_graph graph, arena = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
	struct action_binding bindings[2] = {0};
	for (size_t i = 0; i < 2; ++i) {
		bindings[i].source = pg_binder(&graph);
		for (size_t j = 0; j < 3; ++j) bindings[i].arguments[j] = pg_binder(&graph);
	}
	struct action_scope initial = {value, value, 2, bindings};
	struct action_scope *scope = &initial;
	struct action_result_work original = {&graph, bindings, pg_reference(&graph, bindings[0].arguments[2]), 1, 1};
	struct action_result_work *work = &original;
	unsigned polls = 0;
	for (;;) {
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			const struct action_scope *input = scope;
			assert(file && !pg_action_ownership_write(file, 1, &input, work, 1, &value, &codec, NULL));
			pg_graph_destroy(&arena);
			pg_graph_destroy(&graph);
			assert(!pg_graph_init(&graph));
			rewind(file);
			size_t n, count;
			struct action_scope *const *scopes;
			const struct pg_term *const *roots;
			assert(!pg_action_ownership_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &scopes, &work, &count, &roots));
			assert(n == 1 && count == 1 && work);
			scope = scopes[0];
			value = roots[0];
			assert(scope->bindings == work->bindings && scope->count == 2);
			assert(scope->source == value && work->graph == &graph);
			/* The scope retains the tail after the result cursor has passed it. */
			assert(work->bindings[1].arguments[2] == scope->bindings[1].arguments[2]);
			rewind(file);
			struct action_scope *const *rejected;
			const struct pg_term *const *unused;
			assert(pg_action_scopes_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &rejected, &count, &unused));
			assert(!n && !rejected && !count && !unused && !fclose(file));
		}
		int status = pg_action_result_operation.poll(work);
		++polls;
		assert(status >= 0 && polls <= 3);
		if (status) break;
	}
	assert(polls == 3);
	FILE *file = tmpfile();
	assert(file && !pg_action_ownership_write(file, 0, NULL, work, 1, &value, &codec, NULL));
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
	assert(!pg_graph_init(&graph));
	rewind(file);
	size_t n, count;
	struct action_scope *const *scopes;
	const struct pg_term *const *roots;
	assert(!pg_action_ownership_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &scopes, &work, &count, &roots));
	assert(!n && count == 1 && work->bindings && !work->remaining);
	value = roots[0];
	assert(!fseek(file, 56, SEEK_SET) && !pg_wire_write_u64(file, 2));
	rewind(file);
	struct action_result_work *rejected;
	assert(pg_action_ownership_read(file, &arena, &graph, 1000, 100, &codec, NULL, &n, &scopes, &rejected, &count, &roots));
	assert(!n && !scopes && !rejected && !count && !roots && !fclose(file));
	const struct pg_term *term = work->result;
	const struct pg_term *other = pg_reference(&graph, pg_binder(&graph));
	for (size_t i = 0; i < 4; ++i) term = pg_application(&graph, term, i == 2 ? value : other);
	struct pg_eval machine;
	pg_eval_init(&machine, term);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == value);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&arena);
	pg_graph_destroy(&graph);
}

static const struct pg_term *result_frame_fixture(struct pg_graph *graph, const struct pg_term **value)
{
	*value = pg_reference(graph, pg_binder(graph));
	const struct pg_object *x = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_term *source = pg_identity_action(graph, pg_lambda(graph, x, pg_application(graph, ret, vx)));
	for (size_t i = 0; i < 3; ++i) source = pg_application(graph, source, *value);
	return pg_computation_fold(graph, source, pg_lambda(graph, x, vx), 0, NULL);
}

static void result_frames(void)
{
	uint64_t total = 0;
	unsigned retained = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value;
		const struct pg_term *term = result_frame_fixture(&graph, &value);
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph, term);
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.task && machine.task->operation == &pg_action_result_operation) {
			assert(machine.frames);
			++retained;
			for (unsigned round = 0; round < 2; ++round) {
				struct pg_eval_configuration current = {machine.current, machine.arguments};
				FILE *file = tmpfile();
				assert(file && !pg_computation_frames_write(file, machine.frames, machine.task->state, &current, &pg_builtin_graph_codec, NULL));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				struct pg_eval_frame *frames;
				struct action_result_work *work;
				assert(!pg_computation_frames_read(file, &arena, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &frames, &work, &current));
				assert(work && !fclose(file));
				value = current.arguments->next->value.term;
				pg_computation_eval_init(&machine, &graph, current.head.term);
				machine.current = current.head;
				machine.arguments = current.arguments;
				machine.frames = frames;
				machine.steps = steps;
				machine.head_ready = ready;
				assert(!pg_eval_defer(&machine, &pg_action_result_operation, work));
			}
		}
		machine_resave(&machine, &graph, NULL, &value);
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == value);
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
	assert(retained);
}

static void machine_forest(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	/* An enclosing image may put metadata before an empty machine forest. */
	FILE *empty = tmpfile();
	assert(empty && !pg_wire_write_u64(empty, 123));
	assert(!pg_computation_machines_write(empty, 0, NULL, NULL, 0, NULL, &pg_builtin_graph_codec, NULL));
	assert(!fseek(empty, 8, SEEK_SET));
	size_t empty_count, empty_roots;
	struct pg_eval *const *empty_machines;
	const struct pg_eval_policy *const *empty_policies;
	const struct pg_term *const *empty_terms;
	assert(!pg_computation_machines_read(empty, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
		&empty_count, &empty_machines, &empty_policies, &empty_roots, &empty_terms));
	assert(!empty_count && !empty_roots && !fclose(empty));
	const struct pg_term *value, *other;
	const struct pg_term *term = result_frame_fixture(&graph, &value);
	const struct pg_term *different = result_frame_fixture(&graph, &other);
	struct pg_eval original[4];
	const struct pg_eval *inputs[4];
	const struct pg_eval_policy *policies[] = {&pg_pure_policy, &pg_pure_policy, &pg_beta_policy, &pg_pure_policy};
	const struct pg_term *initial_roots[] = {term, value, different, other};
	const struct pg_term *const *roots = initial_roots;
	uint64_t total[4], elapsed[4];
	for (size_t i = 0; i < 4; ++i) {
		const struct pg_term *input = i == 3 ? different : term;
		struct pg_eval baseline;
		pg_eval_init(&baseline, input);
		baseline.output = &graph;
		baseline.dispatch = policies[i]->dispatch;
		assert(pg_eval_advance(&baseline, 10000) == PG_EVAL_WHNF);
		total[i] = baseline.steps;
		pg_eval_destroy(&baseline);
		pg_eval_init(&original[i], input);
		original[i].output = &graph;
		original[i].dispatch = policies[i]->dispatch;
		if (i == 2) assert(pg_eval_advance(&original[i], 10000) == PG_EVAL_WHNF);
		else {
			while (!original[i].task || original[i].task->operation != &pg_action_result_operation) {
				assert(pg_eval_advance(&original[i], 1) == PG_EVAL_PENDING);
				assert(original[i].steps < total[i]);
			}
		}
		elapsed[i] = original[i].steps;
		inputs[i] = &original[i];
	}
	struct pg_eval *const *machines = NULL;
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_computation_machines_write(file, 4, inputs, policies, 4, roots, &pg_builtin_graph_codec, NULL));
		for (size_t i = 0; i < 4; ++i) pg_eval_destroy(round ? machines[i] : &original[i]);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		size_t n, count;
		const struct pg_eval_policy *const *restored_policies;
		assert(!pg_computation_machines_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
			&n, &machines, &restored_policies, &count, &roots));
		assert(n == 4 && count == 4 && roots[1] != roots[3]);
		for (size_t i = 0; i < 4; ++i) {
			assert(restored_policies[i] == policies[i] && machines[i]->steps == elapsed[i]);
			inputs[i] = machines[i];
		}
		assert(machines[0]->arguments->next->value.term == roots[1]);
		assert(machines[1]->arguments->next->value.term == roots[1]);
		assert(machines[3]->arguments->next->value.term == roots[3]);
		assert(machines[0]->current.term == machines[1]->current.term);
		/* Fail after restoring earlier machines; their task arenas must be released. */
		assert(!fseek(file, 16, SEEK_SET) && !pg_wire_write_u64(file, 3));
		rewind(file);
		struct pg_eval *const *rejected;
		const struct pg_term *const *unused;
		assert(pg_computation_machines_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL,
			&n, &rejected, &restored_policies, &count, &unused));
		assert(!n && !rejected && !restored_policies && !count && !unused && !fclose(file));
	}
	for (size_t i = 0; i < 4; ++i) {
		assert(pg_eval_advance(machines[i], 10000) == PG_EVAL_WHNF && machines[i]->steps == total[i]);
		assert(pg_eval_readback(machines[i], &graph) == (i == 2 ? roots[0] : i == 3 ? roots[3] : roots[1]));
		pg_eval_destroy(machines[i]);
	}
	pg_graph_destroy(&graph);
}

struct whnf_image {
	struct pg_whnf_job *job;
	const struct pg_term *expected;
};

static int whnf_image_write(FILE *file, const struct pg_graph_codec *codec, void *state)
{
	struct whnf_image *image = state;
	if (pg_graph_write_descriptors(file, 1, &image->expected, codec, NULL)) return -1;
	return pg_whnf_pending_write(file, image->job, codec, NULL);
}

static int whnf_image_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *state)
{
	struct whnf_image *image = state;
	size_t count;
	const struct pg_term *const *roots;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, NULL, &count, &roots) || count != 1) return -1;
	image->expected = roots[0];
	return pg_whnf_pending_read(file, image->job, graph, limit, name_limit, codec, NULL);
}

static const struct pg_term *whnf_fixture(struct pg_graph *graph, int materializing,
	const struct pg_term **expected)
{
	if (!materializing) return result_frame_fixture(graph, expected);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_term *vy = pg_reference(graph, y), *body = pg_reference(graph, x);
	const struct pg_term *result = vy;
	for (size_t i = 0; i < 8; ++i) {
		body = pg_application(graph, body, body);
		result = pg_application(graph, result, result);
	}
	*expected = pg_lambda(graph, z, result);
	return pg_application(graph, pg_lambda(graph, x, pg_lambda(graph, y, body)), vy);
}

static void whnf_progress(void)
{
	unsigned positions = 0;
	const char version[8] = "APGWTST";
	for (unsigned variant = 0; variant < 2; ++variant) {
		uint64_t total = 0;
		for (uint64_t cut = 0; ; ++cut) {
			struct pg_graph graph;
			struct pg_whnf_work work;
			assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&work, &graph));
			const struct pg_term *expected, *term = whnf_fixture(&graph, (int)variant, &expected);
			struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, term);
			if (pg_whnf_advance(job, cut) == PG_EVAL_WHNF) {
				assert(pg_whnf_steps(job) == total);
				FILE *file = tmpfile();
				assert(file && pg_whnf_pending_write(file, job, &pg_builtin_graph_codec, NULL));
				assert(!fclose(file));
				pg_whnf_work_destroy(&work);
				pg_graph_destroy(&graph);
				break;
			}
			positions |= job->machine.status == PG_EVAL_PENDING ? 1 : job->output.readback.output ? 4 : 2;
			if (job->machine.task) positions |= 8;
			if (job->machine.steps) {
				FILE *bad = tmpfile();
				assert(bad && !pg_whnf_pending_write(bad, job, &pg_builtin_graph_codec, NULL));
				assert(!fseek(bad, 8, SEEK_SET) && !pg_wire_write_u64(bad, 0));
				rewind(bad);
				struct pg_whnf_job rejected;
				assert(pg_whnf_pending_read(bad, &rejected, &graph, 10000, 100, &pg_builtin_graph_codec, NULL));
				assert(!rejected.request.input && !rejected.request.work && !rejected.request.policy);
				assert(!rejected.machine.current.term && !rejected.output.readback.output && !rejected.steps);
				assert(!fclose(bad));
			}
			struct pg_whnf_job restored;
			for (unsigned round = 0; round < 2; ++round) {
				struct whnf_image image = {job, expected};
				FILE *file = tmpfile();
				assert(file && !pg_graph_image_write(file, version, &pg_builtin_graph_codec, NULL, whnf_image_write, &image));
				pg_whnf_work_destroy(&work);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&work, &graph));
				image.job = &restored;
				rewind(file);
				assert(!pg_graph_image_read(file, version, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, whnf_image_read, &image));
				assert(!restored.request.work && !restored.certificate && pg_whnf_steps(&restored) == cut);
				assert(pg_whnf_advance(&restored, 1) == PG_EVAL_ERROR && pg_whnf_steps(&restored) == cut);
				/* This fixture's origin is the known local evaluator, not imported evidence. */
				assert(!pg_whnf_job_attach(&work, &restored));
				assert(pg_whnf_job_attach(&work, &restored));
				struct pg_whnf_job duplicate = {.request = {.input = restored.request.input,
					.policy = restored.request.policy}, .machine = {.output = &graph}};
				assert(pg_whnf_job_attach(&work, &duplicate) && !duplicate.request.work);
				assert(pg_whnf_request(&work, restored.request.policy, restored.request.input) == &restored);
				job = &restored;
				expected = image.expected;
				assert(!fclose(file));
			}
			assert(pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
			if (!cut) total = pg_whnf_steps(job);
			assert(pg_whnf_steps(job) == total && pg_alpha_equal(pg_whnf_result(job), expected) == 1);
			pg_whnf_work_destroy(&work);
			pg_graph_destroy(&graph);
		}
	}
	assert(positions == 15);
	puts("WHNF pending transport: evaluation, deferred work and capture-avoiding readback retain exact progress");
}

static void reduction_leaf_checks(void)
{
	for (unsigned kind = 0; kind < 4; ++kind) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		const struct pg_object *x = pg_binder(&graph), *z = pg_binder(&graph);
		const struct pg_term *vx = pg_reference(&graph, x);
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		const struct pg_term *source = pg_application(&graph, pg_lambda(&graph, x, vx), value);
		const struct pg_term *target = value;
		if (kind == 1) target = source; /* Convertible, but not WHNF. */
		if (kind == 2) {
			source = pg_application(&graph, pg_lambda(&graph, x, pg_lambda(&graph, z, vx)), value);
			target = pg_lambda(&graph, pg_binder(&graph), value);
		}
		if (kind == 3) {
			const struct pg_term *loop = pg_lambda(&graph, x, pg_application(&graph, vx, vx));
			source = pg_application(&graph, loop, loop);
		}
		struct pg_reduction_certificate claim = {.source = source, .target = target,
			.policy = &pg_beta_policy, .kind = PG_REDUCTION_WHNF};
		const struct pg_reduction_certificate *root = &claim;
		FILE *file = tmpfile();
		assert(file && !pg_reduction_records_write(file, 1, &root, 0, NULL, &pg_builtin_graph_codec, NULL));
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		const struct pg_reduction_archive *archive;
		assert(!pg_reduction_records_read(file, &graph, 1000, 100, &pg_builtin_graph_codec, NULL, &archive));
		assert(!fclose(file));
		struct pg_whnf_work work;
		struct pg_reduction_check check;
		assert(!pg_whnf_work_init(&work, &graph) && !pg_reduction_check_init(&check, &work, archive));
		assert(pg_reduction_check_advance(&check, 0) == PG_COMPARISON_PENDING && !pg_reduction_check_steps(&check));
		assert(!pg_reduction_check_certificate(&check, 0));
		enum pg_comparison_status status = PG_COMPARISON_PENDING;
		for (unsigned i = 0; i < 1000 && status == PG_COMPARISON_PENDING; ++i)
			status = pg_reduction_check_advance(&check, 1);
		assert(status == (kind == 1 ? PG_COMPARISON_DIFFERENT : kind == 3 ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL));
		assert(!!pg_reduction_check_certificate(&check, 0) == (status == PG_COMPARISON_EQUAL));
		assert(!pg_reduction_check_certificate(&check, 1));
		struct pg_whnf_job *job = pg_whnf_request(&work, &pg_beta_policy, archive->roots[0]->source);
		uint64_t steps = pg_whnf_steps(job);
		pg_reduction_check_destroy(&check);
		if (status == PG_COMPARISON_EQUAL) {
			assert(!pg_reduction_check_init(&check, &work, archive));
			assert(pg_reduction_check_advance(&check, 1000) == PG_COMPARISON_EQUAL);
			assert(pg_whnf_steps(job) == steps);
			pg_reduction_check_destroy(&check);
		}
		pg_whnf_work_destroy(&work);
		pg_graph_destroy(&graph);
	}
	puts("Reduction checking: shared WHNF recomputation, alpha relocation, non-normal claim rejection and pending divergence passed");
}

static void reduction_records(void)
{
	struct pg_graph graph;
	struct pg_whnf_work work;
	assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&work, &graph));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
	const struct pg_term *vx = pg_reference(&graph, x);
	const struct pg_term *argument = pg_application(&graph, pg_lambda(&graph, y, pg_reference(&graph, y)), value);
	const struct pg_term *shared = argument;
	for (size_t i = 0; i < 20; ++i) shared = pg_application(&graph, shared, shared);
	const struct pg_term *term = pg_application(&graph, pg_lambda(&graph, x, pg_application(&graph, vx, vx)), shared);
	struct pg_nf_job *pure = pg_nf_request(&work, &pg_pure_policy, term);
	struct pg_nf_job *beta = pg_nf_request(&work, &pg_beta_policy, term);
	assert(pg_nf_advance(pure, 100000) == PG_NF_DONE && pg_nf_advance(beta, 100000) == PG_NF_DONE);
	struct pg_nf_job *normal = pg_nf_request(&work, &pg_pure_policy, pg_nf_result(pure));
	const struct pg_term *neutral = pg_reference(&graph, pg_binder(&graph));
	struct pg_nf_job *duplicated = pg_nf_request(&work, &pg_pure_policy, pg_application(&graph, neutral, neutral));
	assert(pg_nf_advance(duplicated, 10000) == PG_NF_DONE);
	const struct pg_reduction_certificate *initial[] = {
		pg_nf_certificate(pure), pg_nf_certificate(normal), pg_nf_certificate(pure), pg_nf_certificate(beta),
		pg_nf_certificate(duplicated)
	};
	struct pg_nf_job *pending = pg_nf_request(&work, &pg_pure_policy,
		pg_lambda(&graph, pg_binder(&graph), pg_application(&graph, neutral, neutral)));
	while (!pending->phases) {
		assert(pg_nf_advance(pending, 1) == PG_NF_PENDING && pg_nf_steps(pending) < 10000);
	}
	assert(!pending->certificate && pending->stage == NF_RECHECK);
	const struct pg_reduction_phase *initial_phases[] = {pending->phases, pending->phases, initial[4]->phases};
	const struct pg_reduction_archive *archive = NULL;
	for (unsigned round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file);
		assert(!(round ? pg_reduction_archive_write(file, archive, &pg_builtin_graph_codec, NULL)
			: pg_reduction_records_write(file, 5, initial, 3, initial_phases, &pg_builtin_graph_codec, NULL)));
		if (!round) pg_whnf_work_destroy(&work);
		pg_graph_destroy(&graph);
		assert(!pg_graph_init(&graph));
		rewind(file);
		assert(!pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &archive));
		/* Inspect private raw layout only; never pass it to evidence constructors. */
		assert(archive->count == 5 && archive->roots[0] == archive->roots[2]);
		assert(archive->phase_count == 3 && archive->phases[0] == archive->phases[1]);
		assert(archive->phases[0]->children[0] == archive->roots[4]);
		assert(archive->phases[2] == archive->roots[4]->phases);
		assert(archive->roots[1]->normality == archive->roots[0]);
		assert(archive->roots[1]->source == archive->roots[0]->target);
		assert(archive->roots[0]->source == archive->roots[3]->source);
		assert(archive->roots[0]->policy == &pg_pure_policy && archive->roots[3]->policy == &pg_beta_policy);
		unsigned shared_children = 0;
		for (const struct pg_reduction_phase *phase = archive->roots[4]->phases; phase; phase = phase->previous) {
			assert(phase->head && phase->head->kind == PG_REDUCTION_WHNF);
			if (phase->children[0] && phase->children[0] == phase->children[1]) ++shared_children;
		}
		assert(shared_children);
		struct pg_whnf_work verification_work;
		struct pg_reduction_check check;
		assert(!pg_whnf_work_init(&verification_work, &graph));
		assert(!pg_reduction_check_init(&check, &verification_work, archive));
		assert(!pg_reduction_check_certificate(&check, 0));
		assert(pg_reduction_check_advance(&check, 100000) == PG_COMPARISON_EQUAL);
		assert(pg_reduction_check_certificate(&check, 0) == pg_reduction_check_certificate(&check, 2));
		assert(pg_reduction_check_certificate(&check, 4) == archive->roots[4]);
		pg_reduction_check_destroy(&check);
		pg_whnf_work_destroy(&verification_work);
		/* The first topological node is a leaf receipt. A self edge is invalid. */
		assert(!fseek(file, 32, SEEK_SET) && fgetc(file) == 0);
		assert(fgetc(file) == PG_REDUCTION_WHNF);
		uint64_t length;
		assert(!pg_wire_read_u64(file, &length) && length < 100);
		assert(!fseek(file, (long)length, SEEK_CUR) && !pg_wire_write_u64(file, 1));
		rewind(file);
		const struct pg_reduction_archive *rejected = archive;
		assert(pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &rejected) && !rejected);
		assert(!fclose(file));
	}
	struct pg_reduction_certificate cycle = *archive->roots[0];
	cycle.normality = &cycle;
	const struct pg_reduction_certificate *root = &cycle;
	FILE *file = tmpfile();
	assert(file && pg_reduction_records_write(file, 1, &root, 0, NULL, &pg_builtin_graph_codec, NULL));
	assert(!fclose(file));
	struct pg_reduction_phase phase_cycle = *archive->phases[0];
	phase_cycle.previous = &phase_cycle;
	const struct pg_reduction_phase *phase_root = &phase_cycle;
	file = tmpfile();
	assert(file && pg_reduction_records_write(file, 0, NULL, 1, &phase_root, &pg_builtin_graph_codec, NULL));
	assert(!fclose(file));
	/* A partial NF history needs no synthetic completed root receipt. */
	file = tmpfile();
	assert(file && !pg_reduction_records_write(file, 0, NULL, 1, archive->phases, &pg_builtin_graph_codec, NULL));
	rewind(file);
	const struct pg_reduction_archive *phase_only;
	assert(!pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &phase_only));
	assert(!phase_only->count && phase_only->phase_count == 1 && phase_only->phases[0]->children[0]);
	assert(!fclose(file));
	/* Raw writing does not certify local phase equations; reading checks the
	 * same reconstruction rule as the executor before exposing an archive. */
	for (unsigned kind = 0; kind < 4; ++kind) {
		struct pg_reduction_phase invalid = *archive->phases[0];
		struct pg_reduction_certificate head = *invalid.head;
		invalid.head = &head;
		switch (kind) {
		case 0: invalid.rebuilt = archive->roots[4]->target; break;
		case 1: invalid.previous = archive->phases[2]; break;
		case 2: head.policy = &pg_beta_policy; break;
		case 3: invalid.children[1] = invalid.children[0]; invalid.children[0] = NULL; break;
		}
		const struct pg_reduction_phase *root = &invalid;
		file = tmpfile();
		assert(file && !pg_reduction_records_write(file, 0, NULL, 1, &root, &pg_builtin_graph_codec, NULL));
		rewind(file);
		assert(pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &phase_only) && !phase_only);
		assert(!fclose(file));
	}
	/* A consistent partial history is not completed NF. Origin, result and
	 * canonical normality links must also describe the same reduction. */
	for (unsigned kind = 0; kind < 9; ++kind) {
		struct pg_reduction_certificate invalid = *archive->roots[4];
		struct pg_reduction_phase final = *invalid.phases;
		const struct pg_term *other = pg_reference(&graph, pg_binder(&graph));
		switch (kind) {
		case 0: invalid.source = other; break;
		case 1: invalid.target = other; break;
		case 2:
			invalid.phases = archive->phases[0];
			invalid.source = invalid.phases->head->source;
			invalid.target = invalid.phases->rebuilt;
			break;
		case 3: final.previous = NULL; invalid.phases = &final; break;
		case 4: invalid = *archive->roots[1]; invalid.policy = &pg_beta_policy; break;
		case 5: invalid = *archive->roots[1]; invalid.kind = PG_REDUCTION_WHNF; break;
		case 6: invalid = *archive->roots[1]; invalid.source = other; break;
		case 7: invalid = *archive->roots[1]; invalid.phases = &final; break;
		case 8: invalid.kind = PG_REDUCTION_WHNF; break;
		}
		const struct pg_reduction_certificate *root = &invalid;
		file = tmpfile();
		assert(file && !pg_reduction_records_write(file, 1, &root, 0, NULL, &pg_builtin_graph_codec, NULL));
		rewind(file);
		assert(pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &phase_only) && !phase_only);
		assert(!fclose(file));
	}
	/* Root counts cannot reinterpret a certificate root as a phase root. */
	file = tmpfile();
	assert(file && !pg_reduction_archive_write(file, archive, &pg_builtin_graph_codec, NULL));
	assert(!fseek(file, 16, SEEK_SET) && !pg_wire_write_u64(file, 0) && !pg_wire_write_u64(file, 8));
	rewind(file);
	assert(pg_reduction_records_read(file, &graph, 10000, 100, &pg_builtin_graph_codec, NULL, &phase_only) && !phase_only);
	assert(!fclose(file));
	pg_graph_destroy(&graph);
	puts("Reduction archive: receipt/phase sharing, policy identity and acyclic relocation passed (not evidence admission)");
}

static void discovery_progress(int framed)
{
	uint64_t total = 0;
	unsigned positions = 0;
	for (uint64_t cut = 0; ; ++cut) {
		struct pg_graph graph, arena = {0};
		assert(!pg_graph_init(&graph));
		const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
		const struct pg_term *other = pg_reference(&graph, pg_binder(&graph));
		const struct pg_object *captured = pg_binder(&graph);
		if (framed) value = pg_application(&graph, pg_reference(&graph, &pg_thunk_operation), value);
		const struct pg_term *argument = framed ? pg_reference(&graph, captured) : value;
		const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
		const struct pg_term *source = pg_lambda(&graph, x, pg_lambda(&graph, y, pg_reference(&graph, x)));
		const struct pg_term *term = pg_identity_action(&graph, source);
		for (size_t i = 0; i < 6; ++i) term = pg_application(&graph, term, i < 3 ? argument : other);
		if (framed) term = pg_application(&graph, pg_lambda(&graph, captured,
			pg_application(&graph, pg_reference(&graph, &pg_force_operation), term)), value);
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph, term);
		if (pg_eval_advance(&machine, cut) == PG_EVAL_WHNF) {
			assert(machine.steps == total);
			pg_eval_destroy(&machine);
			pg_graph_destroy(&graph);
			break;
		}
		if (machine.task && machine.task->operation == &pg_action_scope_operation) {
			assert((machine.frames != NULL) == framed);
			struct action_scope_work *work = machine.task->state;
			positions |= 1u << work->position;
			for (unsigned round = 0; round < 2; ++round) {
				struct pg_eval_configuration inputs[] = {{machine.current, machine.arguments}, {{value, NULL}, NULL}};
				struct work_frames context = {.arena = &arena, .frames = machine.frames, .current = inputs[0]};
				FILE *file = tmpfile();
				assert(file);
				if (framed) assert(!pg_action_scope_work_write_with(file, work, 2, inputs, work_frames_write, &context));
				else assert(!pg_action_scope_work_write(file, work, 2, inputs, &pg_builtin_graph_codec, NULL));
				uint64_t steps = machine.steps;
				int ready = machine.head_ready;
				pg_eval_destroy(&machine);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t n;
				const struct pg_eval_configuration *restored;
				context.frames = NULL;
				if (framed) {
					assert(!pg_action_scope_work_read_with(file, &arena, &graph, 10000, 100,
						work_frames_read, &context, &work, &n, &restored));
					assert(context.current.arguments == restored[0].arguments);
					assert(context.current.head.term == restored[0].head.term);
					assert(restored[0].arguments->next->value.environment == context.frames->arguments->value.environment);
				} else assert(!pg_action_scope_work_read(file, &arena, &graph, 10000, 100,
					&pg_builtin_graph_codec, NULL, &work, &n, &restored));
				assert(n == 2 && !work->scope.bindings);
				assert(work->scope.source == restored[0].arguments->value.term);
				const struct pg_argument *tail = restored[0].arguments->next;
				for (size_t i = 0; i < 3 * work->scope.count; ++i) tail = tail->next;
				assert(work->arguments == tail);
				assert(!fseek(file, 24, SEEK_SET) && !pg_wire_write_u64(file, 1));
				rewind(file);
				struct action_scope_work *rejected;
				const struct pg_eval_configuration *unused;
				if (framed) {
					struct work_frames bad = {.arena = &arena};
					assert(pg_action_scope_work_read_with(file, &arena, &graph, 10000, 100,
						work_frames_read, &bad, &rejected, &n, &unused));
					assert(bad.frames);
					pg_materialize_destroy(&bad.frames->answer);
				} else assert(pg_action_scope_work_read(file, &arena, &graph, 10000, 100,
					&pg_builtin_graph_codec, NULL, &rejected, &n, &unused));
				assert(!rejected && !n && !unused && !fclose(file));
				value = restored[1].head.term;
				pg_computation_eval_init(&machine, &graph, restored[0].head.term);
				machine.current = restored[0].head;
				machine.arguments = restored[0].arguments;
				machine.steps = steps;
				machine.head_ready = ready;
				machine.frames = context.frames;
				assert(!pg_eval_defer(&machine, &pg_action_scope_operation, work));
			}
		}
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == (framed ? value->as.application.argument : value));
		if (!cut) total = machine.steps;
		assert(machine.steps == total);
		pg_eval_destroy(&machine);
		pg_graph_destroy(&arena);
		pg_graph_destroy(&graph);
	}
	assert(positions == 7);
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
				assert(!fseek(file, 72, SEEK_SET) && !pg_wire_write_u64(file, 16));
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
		size_t scope_count;
		struct action_scope *const *scopes;
		const struct pg_term *const *roots;
		assert(pg_action_body_read(file, arena, output, 10000, 100, &codec, NULL, &rejected, &scope_count, &scopes, &count, &roots));
		assert(!rejected && !scope_count && !scopes && !count && !roots);
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
			work->scope.bindings = pg_alloc(&arena, work->scope.count * sizeof(*work->scope.bindings));
			assert(work->scope.bindings);
			const struct pg_term *source_cursor = work->scope.source;
			for (size_t i = 0; i < work->scope.count; ++i) {
				const struct pg_object *binder = source_cursor->as.lambda.binder;
				work->scope.bindings[i] = (struct action_binding){binder, {binder, binder, binder}};
				source_cursor = source_cursor->as.lambda.body;
			}
			for (size_t i = 0; i < cut; ++i) assert(pg_action_body_operation.poll(work) == (i + 1 == total));
			phases |= 1u << work->phase;
			const struct pg_term *initial[] = {request, expected};
			const struct pg_term *const *roots = initial;
			struct action_scope distinct = work->scope;
			const struct action_scope *owned[] = {&work->scope, &distinct, &work->scope, NULL};
			int prepared = work->scope.bindings[0].arguments[0] != NULL;
			for (unsigned round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				assert(file && !pg_action_body_write(file, work, 4, owned, 2, roots, &codec, NULL));
				invalid_headers(file, &arena, &graph);
				pg_action_body_operation.destroy(work);
				pg_graph_destroy(&arena);
				pg_graph_destroy(&graph);
				assert(!pg_graph_init(&graph));
				rewind(file);
				size_t count;
				size_t scope_count;
				struct action_scope *const *scopes;
				assert(!pg_action_body_read(file, &arena, &graph, 10000, 100, &codec, NULL,
					&work, &scope_count, &scopes, &count, &roots));
				assert(count == 2);
				assert(scope_count == 4 && scopes[0] == &work->scope && scopes[2] == scopes[0]);
				assert(scopes[1] != scopes[0] && !scopes[3]);
				assert(scopes[1]->bindings == work->scope.bindings);
				for (size_t i = 0; i < work->scope.count; ++i) {
					const struct action_binding *binding = &work->scope.bindings[i];
					int initialized = prepared || work->phase != BODY_COLLECT || i < work->position;
					assert((binding->source != NULL) == initialized);
					for (size_t j = 0; j < 3; ++j)
						assert(binding->arguments[j] == (prepared ? binding->source : NULL));
				}
				for (size_t i = 0; i < 4; ++i) owned[i] = scopes[i];
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
	int status = pg_action_body_write(file, work, 0, NULL, count, changed, &codec, NULL);
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

static void write_whnf_file(const char *path, int materializing)
{
	struct pg_graph graph;
	struct pg_whnf_work baseline, work;
	assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&baseline, &graph) && !pg_whnf_work_init(&work, &graph));
	const struct pg_term *expected, *term = whnf_fixture(&graph, materializing, &expected);
	struct pg_whnf_job *whole = pg_whnf_request(&baseline, &pg_pure_policy, term);
	assert(pg_whnf_advance(whole, 10000) == PG_EVAL_WHNF && pg_alpha_equal(pg_whnf_result(whole), expected) == 1);
	uint64_t total = pg_whnf_steps(whole);
	pg_whnf_work_destroy(&baseline);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, term);
	for (;;) {
		if (materializing) {
			if (job->output.readback.steps >= 3) break;
		} else if (job->machine.task && job->machine.task->operation == &pg_action_result_operation) break;
		assert(pg_whnf_advance(job, 1) == PG_EVAL_PENDING && pg_whnf_steps(job) < total);
	}
	struct whnf_image image = {job, expected};
	FILE *file = fopen(path, "wb");
	assert(file && !pg_wire_write_u64(file, total) && !pg_wire_write_u64(file, (uint64_t)materializing));
	assert(!pg_graph_image_write(file, "APGWTST", &pg_builtin_graph_codec, NULL, whnf_image_write, &image));
	assert(!fclose(file));
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
}

static void read_whnf_file(const char *path, const char *resave)
{
	struct pg_graph graph;
	struct pg_whnf_work work;
	struct pg_whnf_job job;
	assert(!pg_graph_init(&graph) && !pg_whnf_work_init(&work, &graph));
	struct whnf_image image = {&job, NULL};
	FILE *file = fopen(path, "rb");
	uint64_t total, materializing;
	assert(file && !pg_wire_read_u64(file, &total) && !pg_wire_read_u64(file, &materializing) && materializing <= 1);
	assert(!pg_graph_image_read(file, "APGWTST", &graph, 10000, 100,
		&pg_builtin_graph_codec, NULL, whnf_image_read, &image));
	assert(!fclose(file) && job.request.policy == &pg_pure_policy && !job.request.work && !job.certificate);
	assert(job.steps < total && job.status == PG_EVAL_PENDING);
	if (materializing) assert(job.machine.status == PG_EVAL_WHNF && job.output.readback.steps >= 3);
	else assert(job.machine.task && job.machine.task->operation == &pg_action_result_operation && job.machine.frames);
	/* Resave before attachment or any execution, including unfinished readback. */
	if (resave) {
		file = fopen(resave, "wb");
		assert(file && !pg_wire_write_u64(file, total) && !pg_wire_write_u64(file, materializing));
		assert(!pg_graph_image_write(file, "APGWTST", &pg_builtin_graph_codec, NULL, whnf_image_write, &image));
		assert(!fclose(file));
	}
	/* The script supplies a known local fixture; this is not evidence admission. */
	assert(!pg_whnf_job_attach(&work, &job));
	while (pg_whnf_status(&job) == PG_EVAL_PENDING) {
		assert(pg_whnf_steps(&job) < total);
		pg_whnf_advance(&job, 1);
	}
	assert(pg_whnf_status(&job) == PG_EVAL_WHNF && pg_whnf_steps(&job) == total);
	assert(pg_alpha_equal(pg_whnf_result(&job), image.expected) == 1);
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
}

static void write_machine_file(const char *path)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *value;
	const struct pg_term *term = result_frame_fixture(&graph, &value);
	struct pg_eval baseline, machine;
	pg_computation_eval_init(&baseline, &graph, term);
	assert(pg_eval_advance(&baseline, 10000) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&baseline, &graph) == value);
	uint64_t total = baseline.steps;
	pg_eval_destroy(&baseline);
	pg_computation_eval_init(&machine, &graph, term);
	while (!machine.task || machine.task->operation != &pg_action_result_operation) {
		assert(pg_eval_advance(&machine, 1) == PG_EVAL_PENDING);
		assert(machine.steps < total);
	}
	assert(machine.frames);
	struct pg_eval_configuration roots[] = {{{value, NULL}, NULL}, {machine.current, machine.arguments}};
	FILE *file = fopen(path, "wb");
	assert(file && !pg_wire_write_u64(file, total));
	assert(!pg_computation_machine_write(file, &machine, &pg_pure_policy, 2, roots, &pg_builtin_graph_codec, NULL));
	assert(!fclose(file));
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

static void read_machine_file(const char *path, const char *resave)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	FILE *file = fopen(path, "rb");
	uint64_t total;
	assert(file && !pg_wire_read_u64(file, &total));
	struct pg_eval machine;
	const struct pg_eval_policy *policy;
	size_t count;
	const struct pg_eval_configuration *roots;
	assert(!pg_computation_machine_read(file, &machine, &graph, 10000, 100,
		&pg_builtin_graph_codec, NULL, &policy, &count, &roots));
	assert(!fclose(file) && count == 2 && policy == &pg_pure_policy);
	assert(machine.task && machine.task->operation == &pg_action_result_operation && machine.frames);
	assert(machine.status == PG_EVAL_PENDING && machine.steps < total);
	assert(machine.current.term == roots[1].head.term);
	assert(machine.current.environment == roots[1].head.environment && machine.arguments == roots[1].arguments);
	assert(machine.arguments->next->value.term == roots[0].head.term);
	if (resave) {
		file = fopen(resave, "wb");
		assert(file && !pg_wire_write_u64(file, total));
		assert(!pg_computation_machine_write(file, &machine, policy, count, roots, &pg_builtin_graph_codec, NULL));
		assert(!fclose(file));
	}
	while (machine.status == PG_EVAL_PENDING) {
		assert(machine.steps < total);
		pg_eval_advance(&machine, 1);
	}
	assert(machine.status == PG_EVAL_WHNF && machine.steps == total);
	assert(pg_eval_readback(&machine, &graph) == roots[0].head.term);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	if (argc == 3 && (!strcmp(argv[1], "write-whnf-eval") || !strcmp(argv[1], "write-whnf-readback"))) {
		write_whnf_file(argv[2], !strcmp(argv[1], "write-whnf-readback"));
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "read-whnf")) {
		read_whnf_file(argv[2], NULL);
		return 0;
	}
	if (argc == 4 && !strcmp(argv[1], "resave-whnf")) {
		read_whnf_file(argv[2], argv[3]);
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "write-machine")) {
		write_machine_file(argv[2]);
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "read-machine")) {
		read_machine_file(argv[2], NULL);
		return 0;
	}
	if (argc == 4 && !strcmp(argv[1], "resave-machine")) {
		read_machine_file(argv[2], argv[3]);
		return 0;
	}
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
	policy_names();
	comparison_scope_owner();
	machine_envelope();
	work_names();
	visit_forest();
	shadow_forest();
	scope_indexes();
	family_discovery();
	family_resume();
	family_result_failure();
	handlers();
	scopes();
	scope_sharing();
	result_ownership();
	result_frames();
	machine_forest();
	whnf_progress();
	reduction_records();
	reduction_leaf_checks();
	discovery_progress(0);
	discovery_progress(1);
	continuation_frames();
	force_frames();
	fold_progress();
	symmetry_progress();
	prefix_progress(0);
	prefix_progress(1);
	higher_progress();
	scope_frames();
	all_cuts();
	configurations();
	configuration_failure();
	assert(machine_tasks == 0xfff);
	puts("Machine payload: all twelve deferred work kinds resume without manual machine restoration");
	puts("Identity ownership: scope/frame sharing and body work resume through the original evaluator");
	return 0;
}
