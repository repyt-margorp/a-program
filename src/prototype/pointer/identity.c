#include "identity_internal.h"
#include "classifier.h"
#include "computation.h"
#include "iadt.h"
#include <string.h>

static const struct pg_object_class identity_class = {"identity-action"};
static const struct pg_object identity_action = {PG_SEMANTIC_OBJECT, &identity_class};
static const struct pg_object_class field_class = {"identity-field"};
static const struct pg_object identity_fields[] = {
	{PG_SEMANTIC_OBJECT, &field_class}, {PG_SEMANTIC_OBJECT, &field_class},
	{PG_SEMANTIC_OBJECT, &field_class}, {PG_SEMANTIC_OBJECT, &field_class}
};

static const struct {
	const struct pg_object *object;
	const char *name;
} descriptors[] = {
	{&identity_action, "kernel/identity/action/v1"},
	{&identity_fields[0], "kernel/identity/transport-right/v1"},
	{&identity_fields[1], "kernel/identity/transport-left/v1"},
	{&identity_fields[2], "kernel/identity/lift-right/v1"},
	{&identity_fields[3], "kernel/identity/lift-left/v1"}
};

const char *pg_identity_name(const struct pg_object *object)
{
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
		if (object == descriptors[i].object) return descriptors[i].name;
	return NULL;
}

const struct pg_object *pg_identity_resolve(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
		if (!strcmp(name, descriptors[i].name)) return descriptors[i].object;
	return NULL;
}

static int field_index(const struct pg_object *object)
{
	for (size_t i = 0; i < sizeof(identity_fields) / sizeof(*identity_fields); ++i)
		if (object == &identity_fields[i]) return (int)i;
	return -1;
}

static const struct pg_term *identity_field(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction, int lift)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	const struct pg_term *field = pg_reference(graph, &identity_fields[2 * lift + direction]);
	return pg_application(graph, pg_application(graph, field, family), value);
}

const struct pg_term *pg_identity_transport(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction)
{
	return identity_field(graph, family, value, direction, 0);
}

const struct pg_term *pg_identity_lift(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction)
{
	return identity_field(graph, family, value, direction, 1);
}

const struct pg_term *pg_identity_action(struct pg_graph *graph, const struct pg_term *source)
{
	return pg_application(graph, pg_reference(graph, &identity_action), source);
}

const struct pg_term *pg_identity_instance(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *left, const struct pg_term *right)
{
	return pg_application(graph, pg_application(graph, family, left), right);
}

const struct pg_term *pg_identity_apply(struct pg_graph *graph, const struct pg_term *function,
	const struct pg_term *left, const struct pg_term *right, const struct pg_term *witness)
{
	const struct pg_term *family = pg_identity_action(graph, function);
	return pg_application(graph, pg_identity_instance(graph, family, left, right), witness);
}

int pg_identity_action_view(const struct pg_term *term, const struct pg_term **source)
{
	if (!term || term->kind != PG_APPLICATION) return 0;
	const struct pg_term *head = term->as.application.function;
	if (head->kind != PG_REFERENCE) return 0;
	if (head->as.reference != &identity_action) return 0;
	*source = term->as.application.argument;
	return 1;
}

int pg_identity_view(const struct pg_term *term, const struct pg_term **type,
	const struct pg_term **left, const struct pg_term **right)
{
	if (!term || term->kind != PG_APPLICATION) return 0;
	const struct pg_term *prefix = term->as.application.function;
	if (prefix->kind != PG_APPLICATION) return 0;
	const struct pg_term *family = prefix->as.application.function;
	if (!pg_identity_action_view(family, type)) return 0;
	*left = prefix->as.application.argument;
	*right = term->as.application.argument;
	return 1;
}

static const struct pg_term *unary_argument(const struct pg_term *term, const struct pg_object *operation)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = term->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != operation) return NULL;
	return term->as.application.argument;
}

static int action_source_scoped(struct pg_eval *, const struct action_scope *, size_t);

static int action_scope_poll(void *opaque)
{
	struct action_scope_work *work = opaque;
	struct action_scope *scope = &work->scope;
	if (scope->body->kind == PG_REFERENCE) {
		if (work->position == scope->count) return 1;
		++work->position;
		if (work->cursor->as.lambda.binder == scope->body->as.reference)
			work->center = 3 * work->position;
		work->cursor = work->cursor->as.lambda.body;
		return 0;
	}
	if (scope->body->kind != PG_LAMBDA) return 1;
	/* A partial triple does not consume another source binder. */
	for (size_t i = 0; i < 3; ++i) if (!pg_eval_next_argument(&work->arguments)) return 1;
	if (scope->count == SIZE_MAX / sizeof(*scope->bindings)) return -1;
	++scope->count;
	scope->body = scope->body->as.lambda.body;
	return 0;
}

static int action_scope_resume(struct pg_eval *machine, void *opaque)
{
	struct action_scope_work *work = opaque;
	return action_source_scoped(machine, &work->scope, work->center);
}

static void arena_work_destroy(void *opaque)
{
	(void)opaque; /* This work owns no storage outside the evaluator arena. */
}

const struct pg_eval_work_operation pg_action_scope_operation = {
	action_scope_poll, action_scope_resume, arena_work_destroy, "identity/action_scope/v1"
};

static int with_action_scope(struct pg_eval *machine, const struct pg_term *source)
{
	struct action_scope_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	work->scope = (struct action_scope){source, source, 0, NULL};
	work->arguments = machine->arguments;
	pg_eval_next_argument(&work->arguments);
	work->cursor = source;
	work->position = 0;
	work->center = 0;
	return pg_eval_defer(machine, &pg_action_scope_operation, work);
}

static int prepare_bindings(struct pg_eval *machine, struct action_scope *scope)
{
	if (!scope->count) return 0;
	const struct pg_term *source = NULL;
	if (!scope->bindings) {
		if (scope->count > SIZE_MAX / sizeof(*scope->bindings)) return -1;
		scope->bindings = pg_alloc(&machine->temporary, scope->count * sizeof(*scope->bindings));
		if (!scope->bindings) return -1;
		source = scope->source;
	}
	for (size_t i = 0; i < scope->count; ++i) {
		if (source) {
			scope->bindings[i].source = source->as.lambda.binder;
			source = source->as.lambda.body;
		}
		for (size_t j = 0; j < 3; ++j) {
			scope->bindings[i].arguments[j] = pg_binder(machine->output);
			if (!scope->bindings[i].arguments[j]) return -1;
		}
	}
	return 0;
}

static const struct pg_term *abstract_body(struct pg_graph *graph,
	const struct action_scope *scope, const struct pg_term *body)
{
	for (size_t i = scope->count; i; --i) body = pg_lambda(graph, scope->bindings[i - 1].source, body);
	return body;
}

static const struct pg_term *acted_body(struct pg_graph *graph,
	const struct action_scope *scope, const struct pg_term *body)
{
	const struct pg_term *result = pg_identity_action(graph, abstract_body(graph, scope, body));
	for (size_t i = 0; i < scope->count; ++i)
		for (size_t j = 0; j < 3; ++j)
			result = pg_application(graph, result, pg_reference(graph, scope->bindings[i].arguments[j]));
	return result;
}

static const struct pg_term *body_endpoint(struct pg_graph *graph,
	const struct action_scope *scope, const struct pg_term *body, size_t side)
{
	const struct pg_term *result = abstract_body(graph, scope, body);
	for (size_t i = 0; i < scope->count; ++i)
		result = pg_application(graph, result, pg_reference(graph, scope->bindings[i].arguments[side]));
	return result;
}

static int action_result_poll(void *opaque)
{
	struct action_result_work *work = opaque;
	if (work->discard) {
		--work->discard;
		work->result = pg_lambda(work->graph, pg_binder(work->graph), work->result);
	} else if (work->remaining) {
		const struct action_binding *binding = &work->bindings[--work->remaining];
		for (size_t j = 3; j; --j)
			work->result = pg_lambda(work->graph, binding->arguments[j - 1], work->result);
	} else return 1;
	return work->result ? 0 : -1;
}

static int action_result_resume(struct pg_eval *machine, void *opaque)
{
	struct action_result_work *work = opaque;
	return pg_eval_enter(machine, (struct pg_closure){work->result, NULL}, 1);
}

const struct pg_eval_work_operation pg_action_result_operation = {
	action_result_poll, action_result_resume, arena_work_destroy, "identity/action_result/v1"
};

static int enter_action(struct pg_eval *machine, const struct action_scope *scope,
	const struct pg_term *result, size_t discard)
{
	if (!scope->count) return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1 + discard);
	/* Administrative lambdas reuse the evaluator's capture-avoiding closure
	 * substitution. Boundary arguments are not evaluated to build the action. */
	struct action_result_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	*work = (struct action_result_work){machine->output, scope->bindings, result, scope->count, discard};
	return pg_eval_defer(machine, &pg_action_result_operation, work);
}

static int action_source_body(struct pg_eval *machine, const struct action_scope *scope, const struct pg_term *head);

/* Orient ap f (refl a) toward refl (f a), never the converse for a neutral
 * application. Four administrative binders preserve all incoming closures. */
static int diagonal_result(struct pg_eval *machine)
{
	struct pg_graph *graph = machine->output;
	const struct pg_object *f = pg_binder(graph), *a = pg_binder(graph), *ignored = pg_binder(graph);
	const struct pg_term *result = pg_identity_action(graph,
		pg_application(graph, pg_reference(graph, f), pg_reference(graph, a)));
	result = pg_lambda(graph, f, pg_lambda(graph, a,
		pg_lambda(graph, ignored, pg_lambda(graph, ignored, result))));
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 0);
}

static int diagonal_argument(struct pg_eval *machine)
{
	const struct pg_argument *cursor = machine->arguments;
	pg_eval_next_argument(&cursor);
	const struct pg_closure *left = pg_eval_next_argument(&cursor);
	const struct pg_closure *right = pg_eval_next_argument(&cursor);
	const struct pg_closure *path = pg_eval_next_argument(&cursor);
	if (!path) return 1;
	if (left->term != right->term || left->environment != right->environment) return 1;
	if (left->environment != path->environment) return 1;
	const struct pg_term *value;
	if (!pg_identity_action_view(path->term, &value) || value != left->term) return 1;
	return diagonal_result(machine);
}

static int diagonal_right(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)answer; (void)unused;
	const struct pg_closure *path = pg_eval_argument(machine, 3);
	const struct pg_term *source;
	if (!path || path->environment || !pg_identity_action_view(path->term, &source)) return 1;
	/* Demanded closures have been materialized. Compare their bound structure
	 * explicitly: readback may freshen Lambda binders, without interning alpha. */
	for (size_t i = 1; i < 3; ++i) {
		const struct pg_closure *endpoint = pg_eval_argument(machine, i);
		if (endpoint->environment) return 1;
		int equal = pg_alpha_equal(endpoint->term, source);
		if (equal != 1) return equal < 0 ? -1 : 1;
	}
	return diagonal_result(machine);
}

static const struct pg_eval_continuation diagonal_right_continuation = {
	"identity/diagonal_right/v1", diagonal_right
};

static int diagonal_left(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)answer; (void)unused;
	return pg_eval_demand(machine, 2, &diagonal_right_continuation, NULL);
}

static const struct pg_eval_continuation diagonal_left_continuation = {
	"identity/diagonal_left/v1", diagonal_left
};

static int diagonal_path(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_term *source;
	if (!pg_identity_action_view(answer, &source)) return 1;
	return pg_eval_demand(machine, 1, &diagonal_left_continuation, NULL);
}

static const struct pg_eval_continuation diagonal_path_continuation = {
	"identity/diagonal_path/v1", diagonal_path
};

/* Only a stuck, fully applied action observes its path. Scope pruning runs
 * first, preserving constant families that ignore even divergent arguments.
 * Demand replaces each argument by its checked reduction, not by an assumed
 * reflexivity proof; opaque loops and mismatched endpoints remain neutral. */
static int diagonal_fallback(struct pg_eval *machine)
{
	return pg_eval_argument(machine, 3)
		? pg_eval_demand(machine, 3, &diagonal_path_continuation, NULL) : 1;
}

static int right_endpoint_scoped(struct pg_eval *machine, const struct action_scope *prepared, const struct pg_term *right)
{
	struct action_scope scope = *prepared;
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(scope.body, &totality, &content)) return -1;
	const struct pg_term *r = unary_argument(right, &pg_return_operation);
	if (!r) return 1;
	const struct pg_term *l = unary_argument(pg_eval_argument(machine, 1 + 3 * scope.count)->term, &pg_return_operation);
	if (!l) return -1;
	if (prepare_bindings(machine, &scope) != 0) return -1;
	const struct pg_term *family = acted_body(machine->output, &scope, content);
	const struct pg_term *inner = pg_identity_instance(machine->output, family, l, r);
	const struct pg_term *result = pg_application(machine->output, scope.body->as.application.function, inner);
	return enter_action(machine, &scope, result, 2);
}

static int right_endpoint(struct pg_eval *machine, const struct pg_term *right, const void *state);
static const struct pg_eval_continuation right_endpoint_continuation = {
	"identity/right_endpoint/v1", right_endpoint
};

static int right_endpoint(struct pg_eval *machine, const struct pg_term *right, const void *state)
{
	return right_endpoint_scoped(machine, state, right);
}

static int left_endpoint_scoped(struct pg_eval *machine, const struct action_scope *prepared, const struct pg_term *left)
{
	struct action_scope scope = *prepared;
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(scope.body, &totality, &content)) return -1;
	if (!unary_argument(left, &pg_return_operation)) return 1;
	return pg_eval_demand(machine, 2 + 3 * scope.count, &right_endpoint_continuation, prepared);
}

static int left_endpoint(struct pg_eval *machine, const struct pg_term *left, const void *state);
static const struct pg_eval_continuation left_endpoint_continuation = {
	"identity/left_endpoint/v1", left_endpoint
};

static int left_endpoint(struct pg_eval *machine, const struct pg_term *left, const void *state)
{
	return left_endpoint_scoped(machine, state, left);
}

static int thunk_type_action(struct pg_eval *machine, struct action_scope *scope,
	const struct pg_term *content)
{
	if (!pg_eval_argument(machine, 2 + 3 * scope->count)) return 1;
	if (prepare_bindings(machine, scope) != 0) return -1;
	struct pg_graph *graph = machine->output;
	const struct pg_object *x0 = pg_binder(graph), *x1 = pg_binder(graph);
	const struct pg_term *force = pg_reference(graph, &pg_force_operation);
	const struct pg_term *inner = pg_identity_instance(graph, acted_body(graph, scope, content),
		pg_application(graph, force, pg_reference(graph, x0)),
		pg_application(graph, force, pg_reference(graph, x1)));
	const struct pg_term *result = pg_application(graph, scope->body->as.application.function, inner);
	/* Construct observations, without demanding or executing either endpoint. */
	return enter_action(machine, scope, pg_lambda(graph, x0, pg_lambda(graph, x1, result)), 0);
}

static int pi_action(struct pg_eval *machine, struct action_scope *scope, const struct pg_term *domain,
	const struct pg_object *binder, const struct pg_term *codomain)
{
	if (!pg_eval_argument(machine, 2 + 3 * scope->count)) return 1;
	if (prepare_bindings(machine, scope) != 0) return -1;
	struct pg_graph *graph = machine->output;
	const struct pg_object *f0 = pg_binder(graph), *f1 = pg_binder(graph);
	const struct pg_object *x0 = pg_binder(graph), *x1 = pg_binder(graph), *p = pg_binder(graph);
	const struct pg_term *l = pg_reference(graph, x0), *r = pg_reference(graph, x1);
	const struct pg_term *path = pg_reference(graph, p);
	const struct pg_term *path_type = pg_identity_instance(graph, acted_body(graph, scope, domain), l, r);
	const struct pg_term *family = acted_body(graph, scope, pg_lambda(graph, binder, codomain));
	family = pg_application(graph, pg_identity_instance(graph, family, l, r), path);
	const struct pg_term *result = pg_identity_instance(graph, family,
		pg_application(graph, pg_reference(graph, f0), l),
		pg_application(graph, pg_reference(graph, f1), r));
	result = pg_pi(graph, body_endpoint(graph, scope, domain, 0), x0,
		pg_pi(graph, body_endpoint(graph, scope, domain, 1), x1, pg_pi(graph, path_type, p, result)));
	/* Administrative Core lambdas retain endpoint closures without running them.
	 * They are not extra value binders in an accepted CBPV context. */
	result = pg_lambda(graph, f0, pg_lambda(graph, f1, result));
	return enter_action(machine, scope, result, 0);
}

static uint64_t scope_visit_hash(const struct pg_term *term, const struct scope_shadow *shadow)
{
	return (uint64_t)(uintptr_t)term ^ ((uint64_t)(uintptr_t)shadow * UINT64_C(1099511628211));
}

static const struct scope_visit *scope_visit_find(const struct pg_index *index,
	const struct pg_term *term, const struct scope_shadow *shadow)
{
	uint64_t hash = scope_visit_hash(term, shadow);
	for (struct pg_index_entry *entry = pg_index_candidates(index, hash); entry; entry = entry->next) {
		const struct scope_visit *visit = (const struct scope_visit *)entry;
		if (visit->term == term && visit->shadow == shadow) return visit;
	}
	return NULL;
}

static int scope_push(struct pg_graph *arena, struct pg_index *seen, struct scope_visit **pending,
	const struct pg_term *term, const struct scope_shadow *shadow)
{
	if (scope_visit_find(seen, term, shadow)) return 0;
	struct scope_visit *visit = pg_alloc(arena, sizeof(*visit));
	if (!visit) return -1;
	*visit = (struct scope_visit){.term = term, .shadow = shadow, .next = *pending};
	*pending = visit;
	return 0;
}

/* Residual action carries a binder-to-triple assignment. Exchange only this
 * administrative environment, never accepted dependent contexts or cube axes. */
static uint64_t scope_binding_hash(const struct pg_object *binder)
{
	uint64_t pointer = (uintptr_t)binder;
	return (pointer >> 4) ^ (pointer >> 17);
}

static struct scope_binding_index *scope_binding_find(const struct pg_index *index, const struct pg_object *binder)
{
	for (struct pg_index_entry *entry = pg_index_candidates(index, scope_binding_hash(binder)); entry; entry = entry->next) {
		struct scope_binding_index *candidate = (struct scope_binding_index *)entry;
		if (candidate->binder == binder) return candidate;
	}
	return NULL;
}

int pg_scope_indexes_restore(struct scope_work *work, size_t visit_count, struct scope_visit *const *visits,
	size_t source_count, struct scope_binding_index *const *sources)
{
	if (!work || work->seen.buckets || work->sources.buckets) return -1;
	if ((visit_count && !visits) || (source_count && !sources)) return -1;
	if (pg_index_init(&work->seen) || pg_index_init(&work->sources)) goto failure;
	for (size_t i = 0; i < visit_count; ++i) {
		struct scope_visit *visit = visits[i];
		if (!visit || !visit->term || scope_visit_find(&work->seen, visit->term, visit->shadow)) goto failure;
		if (pg_index_insert(&work->seen, &visit->index, scope_visit_hash(visit->term, visit->shadow))) goto failure;
	}
	for (size_t i = 0; i < source_count; ++i) {
		struct scope_binding_index *source = sources[i];
		if (!source || !source->binder || source->binder->kind != PG_BINDER) goto failure;
		if (source->position >= work->scope.count || scope_binding_find(&work->sources, source->binder)) goto failure;
		if (pg_index_insert(&work->sources, &source->index, scope_binding_hash(source->binder))) goto failure;
	}
	return 0;
failure:
	pg_index_destroy(&work->seen);
	pg_index_destroy(&work->sources);
	return -1;
}

static int action_body(struct pg_eval *machine, const struct pg_term *answer, const void *unused);

static int scope_visit_poll(struct scope_work *work)
{
	if (work->count == work->scope.count) return 1;
	if (work->reference) {
		if (work->shadow) {
			if (work->shadow->binder == work->reference->as.reference) work->reference = NULL;
			work->shadow = work->shadow->parent;
			return 0;
		}
		work->reference = NULL;
		size_t i = work->reference_position;
		work->used[i] = 1;
		if (i != work->count) work->changed = 1;
		work->order[work->count++] = i;
		return 0;
	}
	if (!work->pending) return 1;
	struct scope_visit *visit = work->pending;
	work->pending = visit->next;
	/* Mark on visitation, not scheduling: an argument also reached through
	 * the function must receive its first position on the function path. */
	if (scope_visit_find(&work->seen, visit->term, visit->shadow)) return 0;
	if (pg_index_insert(&work->seen, &visit->index, scope_visit_hash(visit->term, visit->shadow)) != 0) return -1;
	const struct pg_term *term = visit->term;
	int status = 0;
	switch (term->kind) {
	case PG_LAMBDA: {
		struct scope_shadow *shadow = pg_alloc(work->arena, sizeof(*shadow));
		if (!shadow) return -1;
		*shadow = (struct scope_shadow){term->as.lambda.binder, visit->shadow};
		status = scope_push(work->arena, &work->seen, &work->pending, term->as.lambda.body, shadow);
		break;
	}
	case PG_APPLICATION:
		status = scope_push(work->arena, &work->seen, &work->pending, term->as.application.argument, visit->shadow);
		if (!status) status = scope_push(work->arena, &work->seen, &work->pending, term->as.application.function, visit->shadow);
		break;
	case PG_REFERENCE: {
		/* Only an undiscovered source binder can affect first-use order.
		 * Ambient and semantic references need no shadow-chain traversal. */
		const struct scope_binding_index *source = scope_binding_find(&work->sources, term->as.reference);
		if (!source || work->used[source->position]) break;
		work->reference = term;
		work->shadow = visit->shadow;
		work->reference_position = source->position;
		break;
	}
	}
	return status;
}

static int scope_poll(void *opaque)
{
	struct scope_work *work = opaque;
	struct action_scope *scope = &work->scope;
	switch (work->phase) {
	case SCOPE_SOURCES:
		if (work->position == scope->count) { work->phase = SCOPE_HEAD; return 0; }
		const struct pg_object *binder = work->cursor->as.lambda.binder;
		struct scope_binding_index *source = scope_binding_find(&work->sources, binder);
		if (!source) {
			source = pg_alloc(work->arena, sizeof(*source));
			if (!source) return -1;
			source->binder = binder;
			if (pg_index_insert(&work->sources, &source->index, scope_binding_hash(binder)) != 0) return -1;
		}
		/* Only the transient index changes; the innermost source declaration wins. */
		source->position = work->position;
		scope->bindings[work->position++].source = binder;
		work->cursor = work->cursor->as.lambda.body;
		return 0;
	case SCOPE_HEAD:
		if (work->cursor->kind == PG_APPLICATION) {
			work->cursor = work->cursor->as.application.function;
			return 0;
		}
		work->canonical = work->cursor->kind == PG_REFERENCE && work->cursor->as.reference == &identity_action;
		work->head = work->cursor;
		work->phase = SCOPE_VISIT;
		return 0;
	case SCOPE_VISIT: {
		int status = scope_visit_poll(work);
		if (status != 1) return status;
		if (work->count != scope->count) {
			work->changed = 1;
			work->position = 0;
			work->phase = SCOPE_FILTER;
			work->result = scope->body;
			return 0;
		}
		if (!work->canonical) work->changed = 0;
		work->phase = work->changed ? SCOPE_ABSTRACT : SCOPE_READY;
		work->position = scope->count;
		work->result = scope->body;
		return 0;
	}
	case SCOPE_FILTER:
		if (work->position == scope->count) {
			work->position = work->count;
			work->phase = SCOPE_ABSTRACT;
			return 0;
		}
		if (work->used[work->position]) work->order[work->selected++] = work->position;
		++work->position;
		return 0;
	case SCOPE_ABSTRACT:
		if (work->position) {
			const struct pg_object *binder = scope->bindings[work->order[--work->position]].source;
			work->result = pg_lambda(work->output, binder, work->result);
		} else {
			work->result = pg_identity_action(work->output, work->result);
			work->phase = SCOPE_APPLY;
		}
		return work->result ? 0 : -1;
	case SCOPE_APPLY:
		if (work->position == work->count) {
			work->position = scope->count;
			work->phase = SCOPE_WRAP;
			return 0;
		}
		struct action_binding *binding = &scope->bindings[work->order[work->position++]];
		for (size_t j = 0; j < 3; ++j) {
			binding->arguments[j] = pg_binder(work->output);
			if (!binding->arguments[j]) return -1;
			work->result = pg_application(work->output, work->result, pg_reference(work->output, binding->arguments[j]));
			if (!work->result) return -1;
		}
		return 0;
	case SCOPE_WRAP:
		if (!work->position) { work->phase = SCOPE_READY; return 0; }
		--work->position;
		for (size_t j = 3; j; --j) {
			if (!scope->bindings[work->position].arguments[j - 1])
				scope->bindings[work->position].arguments[j - 1] = pg_binder(work->output);
			if (!scope->bindings[work->position].arguments[j - 1]) return -1;
			work->result = pg_lambda(work->output, scope->bindings[work->position].arguments[j - 1], work->result);
			if (!work->result) return -1;
		}
		return 0;
	case SCOPE_READY: return 1;
	}
	return -1;
}

static void scope_destroy(void *opaque)
{
	struct scope_work *work = opaque;
	pg_index_destroy(&work->seen);
	pg_index_destroy(&work->sources);
}

static int scope_resume(struct pg_eval *machine, void *opaque)
{
	struct scope_work *work = opaque;
	if (!work->changed) return action_source_body(machine, &work->scope, work->head);
	return pg_eval_enter(machine, (struct pg_closure){work->result, NULL}, 1);
}

const struct pg_eval_work_operation pg_scope_operation = {
	scope_poll, scope_resume, scope_destroy, "identity/scope_analysis/v1"
};

static int analyze_scope(struct pg_eval *machine, const struct action_scope *scope)
{
	if (!scope->count) return action_source_body(machine, scope, NULL);
	struct scope_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	work->arena = &machine->temporary;
	work->output = machine->output;
	work->scope = *scope;
	work->scope.bindings = pg_alloc(work->arena, scope->count * sizeof(*scope->bindings));
	work->cursor = scope->source;
	work->order = pg_alloc(work->arena, scope->count * sizeof(*work->order));
	work->used = pg_alloc(work->arena, scope->count);
	if (!work->scope.bindings || !work->order || !work->used || pg_index_init(&work->seen) != 0) return -1;
	if (pg_index_init(&work->sources) != 0) { scope_destroy(work); return -1; }
	int status = scope_push(work->arena, &work->seen, &work->pending, scope->body, NULL);
	if (!status) status = pg_eval_defer(machine, &pg_scope_operation, work);
	if (status) scope_destroy(work);
	return status;
}

static int action_body_poll(void *opaque)
{
	struct action_body_work *work = opaque;
	switch (work->phase) {
	case BODY_COMPARE: {
		enum pg_comparison_status status = pg_comparison_advance(&work->comparison, 1);
		if (status == PG_COMPARISON_PENDING) return 0;
		if (status == PG_COMPARISON_ERROR) return -1;
		if (status == PG_COMPARISON_EQUAL) { work->phase = BODY_READY; return 0; }
		work->scope.bindings = pg_alloc(work->arena, work->scope.count * sizeof(*work->scope.bindings));
		if (!work->scope.bindings) return -1;
		work->cursor = work->scope.source;
		work->phase = BODY_COLLECT;
		return 0;
	}
	case BODY_COLLECT:
		if (work->position == work->scope.count) { work->phase = BODY_WRAP; return 0; }
		work->scope.bindings[work->position++].source = work->cursor->as.lambda.binder;
		work->cursor = work->cursor->as.lambda.body;
		return 0;
	case BODY_WRAP:
		if (work->position) {
			work->answer = pg_lambda(work->output, work->scope.bindings[--work->position].source, work->answer);
		} else {
			work->answer = pg_identity_action(work->output, work->answer);
			work->phase = BODY_READY;
		}
		return work->answer ? 0 : -1;
	case BODY_READY: return 1;
	}
	return -1;
}

static void action_body_destroy(void *opaque)
{
	struct action_body_work *work = opaque;
	pg_comparison_destroy(&work->comparison);
}

static int action_body_resume(struct pg_eval *machine, void *opaque)
{
	struct action_body_work *work = opaque;
	/* An unchanged open body does not make its applied boundary neutral:
	 * the supplied path can still compute to reflexivity. */
	if (pg_comparison_status(&work->comparison) == PG_COMPARISON_EQUAL) return diagonal_fallback(machine);
	return pg_eval_enter(machine, (struct pg_closure){work->answer, NULL}, 1);
}

const struct pg_eval_work_operation pg_action_body_operation = {
	action_body_poll, action_body_resume, action_body_destroy, "identity/action_body/v2"
};

static int action_body_scoped(struct pg_eval *machine, const struct action_scope *prepared, const struct pg_term *answer)
{
	struct action_body_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	work->answer = answer;
	work->arena = &machine->temporary;
	work->output = machine->output;
	work->scope = *prepared;
	if (pg_comparison_init(&work->comparison, work->scope.body, answer, NULL, NULL) != 0) return -1;
	int status = pg_eval_defer(machine, &pg_action_body_operation, work);
	if (status) action_body_destroy(work);
	return status;
}

static int action_body(struct pg_eval *machine, const struct pg_term *answer, const void *state);
static const struct pg_eval_continuation action_body_continuation = {
	"identity/action_body/v1", action_body
};

static int action_body(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	return action_body_scoped(machine, state, answer);
}

static int action_source_scoped(struct pg_eval *machine, const struct action_scope *prepared, size_t center)
{
	struct action_scope scope = *prepared;
	const struct pg_term *source = scope.source;
	int status;
	if (!scope.count) {
		status = pg_data_action(machine, source);
		if (status != 1) return status;
	}
	const struct pg_term *body = scope.body;
	const struct pg_term *contracted = pg_computation_eta(machine->output, body);
	if (contracted) {
		if (prepare_bindings(machine, &scope) != 0) return -1;
		return enter_action(machine, &scope, acted_body(machine->output, &scope, contracted), 0);
	}
	if (body->kind == PG_REFERENCE) {
		if (center) return pg_eval_enter(machine, *pg_eval_argument(machine, center), 1 + 3 * scope.count);
		if (!scope.count) return diagonal_fallback(machine);
		const struct pg_term *result = pg_identity_action(machine->output, body);
		return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1 + 3 * scope.count);
	}
	return analyze_scope(machine, prepared);
}

static int higher_scope_poll(void *opaque)
{
	struct higher_scope_work *work = opaque;
	if (!work->body) {
		const struct pg_term *inner;
		if (!work->collecting && pg_identity_action_view(work->cursor, &inner)) {
			if (work->arity > SIZE_MAX / 3) return -1;
			work->arity *= 3;
			work->cursor = inner;
			return 0;
		}
		work->collecting = 1;
		if (work->cursor->kind == PG_LAMBDA) {
			if (work->lambda_count == SIZE_MAX) return -1;
			++work->lambda_count;
			work->cursor = work->cursor->as.lambda.body;
			return 0;
		}
		if (!work->lambda_count) return 1;
		if (work->arity > SIZE_MAX / work->lambda_count) return -1;
		work->arity *= work->lambda_count;
		if (work->arity > SIZE_MAX / sizeof(*work->binders)) return -1;
		work->binders = pg_alloc(work->arena, work->arity * sizeof(*work->binders));
		if (!work->binders) return -1;
		work->body = work->source;
		return 0;
	}
	if (!work->wrapping) {
		if (work->position == work->arity) { work->wrapping = 1; return 0; }
		const struct pg_object *binder = pg_binder(work->graph);
		if (!binder) return -1;
		work->binders[work->position++] = binder;
		work->body = pg_application(work->graph, work->body, pg_reference(work->graph, binder));
		return work->body ? 0 : -1;
	}
	if (!work->position) return 1;
	work->body = pg_lambda(work->graph, work->binders[--work->position], work->body);
	return work->body ? 0 : -1;
}

static int higher_scope_resume(struct pg_eval *machine, void *opaque)
{
	struct higher_scope_work *work = opaque;
	return with_action_scope(machine, work->body ? work->body : work->source);
}

const struct pg_eval_work_operation pg_higher_scope_operation = {
	higher_scope_poll, higher_scope_resume, arena_work_destroy, "identity/higher_scope/v1"
};

static int action_source(struct pg_eval *machine, const struct pg_term *source, const void *unused);
static const struct pg_eval_continuation action_source_continuation = {
	"identity/action_source/v1", action_source
};

static int action_source(struct pg_eval *machine, const struct pg_term *source, const void *unused)
{
	(void)unused;
	const struct pg_term *inner;
	if (pg_eval_argument(machine, 3) && pg_identity_action_view(source, &inner)) {
		/* Expose the boundary arguments of a known iterated function action.
		 * Opaque sources are not inferred to be functions from APP arity. */
		struct higher_scope_work *work = pg_alloc(&machine->temporary, sizeof(*work));
		if (!work) return -1;
		*work = (struct higher_scope_work){.graph = machine->output, .arena = &machine->temporary,
			.source = source, .cursor = source, .arity = 1};
		return pg_eval_defer(machine, &pg_higher_scope_operation, work);
	}
	return with_action_scope(machine, source);
}

static int action_source_body(struct pg_eval *machine, const struct action_scope *prepared, const struct pg_term *head)
{
	struct action_scope scope = *prepared;
	const struct pg_term *body = scope.body;
	const struct pg_term *argument = unary_argument(body, &pg_return_operation);
	if (!argument) argument = unary_argument(body, &pg_thunk_operation);
	if (!argument && scope.count) argument = unary_argument(body, &pg_force_operation);
	if (argument) {
		if (prepare_bindings(machine, &scope) != 0) return -1;
		const struct pg_term *acted = acted_body(machine->output, &scope, argument);
		const struct pg_term *result = pg_application(machine->output, body->as.application.function, acted);
		return enter_action(machine, &scope, result, 0);
	}
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (pg_pi_view(body, &domain, &binder, &codomain)) return pi_action(machine, &scope, domain, binder, codomain);
	const struct pg_term *content;
	if (pg_thunk_type_view(body, &content)) return thunk_type_action(machine, &scope, content);
	enum pg_totality totality;
	if (pg_pure_computation_type_view(body, &totality, &content)) {
		if (!pg_eval_argument(machine, 2 + 3 * scope.count)) return 1;
		return pg_eval_demand(machine, 1 + 3 * scope.count, &left_endpoint_continuation, prepared);
	}
	/* With no varying binder, congruence would rebuild ap f (refl a) and
	 * loop with diagonal_argument. Keep refl of a neutral APP as its form. */
	if (!scope.count) return diagonal_fallback(machine);
	if (head->kind == PG_REFERENCE) {
		if (head->as.reference == &identity_action) {
			return pg_eval_demand_closure(machine, (struct pg_closure){body, NULL}, &action_body_continuation, prepared);
		}
		/* Uniform higher fields need their own boundary rules, not ordinary
		 * Pi congruence applied to an untyped field reference. */
		if (field_index(head->as.reference) >= 0) return 1;
	}
	if (body->kind != PG_APPLICATION) return 1;
	if (prepare_bindings(machine, &scope) != 0) return -1;
	struct pg_graph *graph = machine->output;
	const struct pg_term *function = acted_body(graph, &scope, body->as.application.function);
	argument = body->as.application.argument;
	const struct pg_term *result = pg_identity_instance(graph, function,
		body_endpoint(graph, &scope, argument, 0), body_endpoint(graph, &scope, argument, 1));
	result = pg_application(graph, result, acted_body(graph, &scope, argument));
	return enter_action(machine, &scope, result, 0);
}

static int family_scope_poll(void *opaque)
{
	struct family_scope_work *work = opaque;
	const struct pg_term *source;
	if (!work->scope.source) {
		if (pg_identity_action_view(work->cursor, &source)) {
			if (!work->supplied || work->supplied % 3) return 1;
			work->scope = (struct action_scope){source, source, 0, NULL};
			return 0;
		}
		if (work->cursor->kind != PG_APPLICATION) return 1;
		if (work->supplied == SIZE_MAX) return -1;
		++work->supplied;
		work->cursor = work->cursor->as.application.function;
		return 0;
	}
	if (work->scope.body->kind == PG_LAMBDA) {
		if (work->scope.count == work->supplied / 3) return 1;
		++work->scope.count;
		work->scope.body = work->scope.body->as.lambda.body;
		return 0;
	}
	if (work->scope.count == work->supplied / 3)
		pg_thunk_type_view(work->scope.body, &work->content);
	return 1;
}

static int with_thunk_family(struct pg_eval *machine, const struct pg_term *family,
	const struct pg_term *value, const struct pg_eval_work_operation *operation)
{
	struct family_scope_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	*work = (struct family_scope_work){.cursor = family, .value = value};
	return pg_eval_defer(machine, operation, work);
}

static int family_result_poll(void *opaque)
{
	struct family_result_work *work = opaque;
	switch (work->phase) {
	case FAMILY_COLLECT:
		if (!work->position) {
			work->phase = FAMILY_WRAP;
			return 0;
		}
		work->arguments[--work->position] = work->family->as.application.argument;
		work->family = work->family->as.application.function;
		return 0;
	case FAMILY_WRAP: {
		int status = action_result_poll(&work->closure);
		if (status != 1) return status;
		work->phase = FAMILY_APPLY;
		return 0;
	}
	case FAMILY_APPLY:
		if (work->position == work->count) return 1;
		work->closure.result = pg_application(work->closure.graph, work->closure.result,
			work->arguments[work->position++]);
		return work->closure.result ? 0 : -1;
	}
	return -1;
}

static int close_family(struct pg_eval *machine, const struct action_scope *scope,
	const struct pg_term *family, const struct pg_term *result,
	const struct pg_eval_work_operation *operation)
{
	struct family_result_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work || scope->count > SIZE_MAX / 3 / sizeof(*work->arguments)) return -1;
	work->count = 3 * scope->count;
	work->arguments = pg_alloc(&machine->temporary, work->count * sizeof(*work->arguments));
	if (!work->arguments) return -1;
	work->closure = (struct action_result_work){machine->output, scope->bindings, result, scope->count, 0};
	work->family = family;
	work->position = work->count;
	work->phase = FAMILY_COLLECT;
	return pg_eval_defer(machine, operation, work);
}

static int force_family_result(struct pg_eval *machine, void *opaque)
{
	struct family_result_work *work = opaque;
	return pg_eval_apply(machine, (struct pg_closure){work->closure.result, NULL}, *pg_eval_argument(machine, 1), 2);
}

const struct pg_eval_work_operation pg_force_family_result_operation = {
	family_result_poll, force_family_result, arena_work_destroy, "identity/force_family_result/v1"
};

static int field_family_result(struct pg_eval *machine, void *opaque)
{
	struct family_result_work *work = opaque;
	return pg_eval_enter(machine, (struct pg_closure){work->closure.result, NULL}, 2);
}

const struct pg_eval_work_operation pg_field_family_result_operation = {
	family_result_poll, field_family_result, arena_work_destroy, "identity/field_family_result/v1"
};

static int force_family_scoped(struct pg_eval *machine, void *opaque)
{
	struct family_scope_work *work = opaque;
	const struct pg_term *computation = work->content, *value = work->value;
	if (!computation) return 1;
	const struct pg_term *prefix = value->as.application.function;
	const struct pg_term *head = prefix->as.application.function;
	int field = field_index(head->as.reference);
	const struct pg_term *family = prefix->as.application.argument;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	struct action_scope scope = work->scope;
	if (!pg_pi_view(computation, &domain, &binder, &codomain)) return 1;
	if (prepare_bindings(machine, &scope) != 0) return -1;
	struct pg_graph *graph = machine->output;
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *target = pg_reference(graph, y);
	enum pg_identity_direction direction = (enum pg_identity_direction)field;
	enum pg_identity_direction reverse = field ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
	const struct pg_term *domain_path = acted_body(graph, &scope, domain);
	const struct pg_term *input = pg_identity_transport(graph, domain_path, target, reverse);
	const struct pg_term *lift = pg_identity_lift(graph, domain_path, target, reverse);
	/* Reuse value transport on U(codomain), then observe it with FORCE.
	 * Returning and further function results use their ordinary U rules. */
	const struct pg_term *quoted_codomain = pg_application(graph, scope.body->as.application.function, codomain);
	const struct pg_term *result_path = acted_body(graph, &scope, pg_lambda(graph, binder, quoted_codomain));
	result_path = pg_application(graph,
		pg_identity_instance(graph, result_path, field ? target : input, field ? input : target), lift);
	const struct pg_term *call = pg_application(graph,
		pg_application(graph, pg_reference(graph, &pg_force_operation), value->as.application.argument), input);
	const struct pg_term *result = pg_application(graph, pg_reference(graph, &pg_thunk_operation), call);
	result = pg_application(graph, pg_reference(graph, &pg_force_operation),
		pg_identity_transport(graph, result_path, result, direction));
	return close_family(machine, &scope, family, pg_lambda(graph, y, result), &pg_force_family_result_operation);
}

const struct pg_eval_work_operation pg_force_family_scope_operation = {
	family_scope_poll, force_family_scoped, arena_work_destroy, "identity/force_family_scope/v1"
};

int pg_identity_force(struct pg_eval *machine, const struct pg_term *value)
{
	if (!pg_eval_argument(machine, 1) || value->kind != PG_APPLICATION) return 1;
	const struct pg_term *prefix = value->as.application.function;
	if (prefix->kind != PG_APPLICATION) return 1;
	const struct pg_term *head = prefix->as.application.function;
	if (head->kind != PG_REFERENCE) return 1;
	int field = field_index(head->as.reference);
	if (field < 0 || field > 1) return 1;
	return with_thunk_family(machine, prefix->as.application.argument, value, &pg_force_family_scope_operation);
}

static int thunk_return_scoped(struct pg_eval *machine, const struct action_scope *prepared,
	const struct pg_term *computation, const struct pg_term *value)
{
	const struct pg_term *body = unary_argument(value, &pg_thunk_operation);
	const struct pg_term *payload = body ? unary_argument(body, &pg_return_operation) : NULL;
	int field = field_index(machine->current.term->as.reference);
	if (!payload && field >= 2) return 1;
	struct action_scope scope = *prepared;
	const struct pg_term *content, *family = pg_eval_argument(machine, 0)->term;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(computation, &totality, &content)) return -1;
	if (prepare_bindings(machine, &scope) != 0) return -1;
	struct pg_graph *graph = machine->output;
	const struct pg_object *binder = payload ? NULL : pg_binder(graph);
	const struct pg_term *result = identity_field(graph, acted_body(graph, &scope, content),
		payload ? payload : pg_reference(graph, binder), (enum pg_identity_direction)(field % 2), field / 2);
	result = pg_application(graph, pg_reference(graph, &pg_return_operation), result);
	if (binder) {
		/* Suspend the map. The input computation occurs once, and is not run
		 * while transporting its thunk. General dependent lifting is separate. */
		const struct pg_term *source = pg_application(graph, pg_reference(graph, &pg_force_operation), value);
		result = pg_application(graph,
			pg_application(graph, pg_reference(graph, &pg_fold_operation), source), pg_lambda(graph, binder, result));
	}
	result = pg_application(graph, pg_reference(graph, &pg_thunk_operation), result);
	return close_family(machine, &scope, family, result, &pg_field_family_result_operation);
}

static int thunk_return_field(struct pg_eval *machine, const struct pg_term *value, const void *state);
static const struct pg_eval_continuation thunk_return_field_continuation = {
	"identity/thunk_return_field/v1", thunk_return_field
};

static int thunk_return_field(struct pg_eval *machine, const struct pg_term *value, const void *state)
{
	const struct action_scope *scope = state;
	const struct pg_term *computation;
	if (!pg_thunk_type_view(scope->body, &computation)) return -1;
	return thunk_return_scoped(machine, scope, computation, value);
}

static int field_family_scoped(struct pg_eval *machine, void *opaque)
{
	struct family_scope_work *work = opaque;
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(work->content, &totality, &content)) return 1;
	return pg_eval_demand(machine, 1, &thunk_return_field_continuation, &work->scope);
}

const struct pg_eval_work_operation pg_field_family_scope_operation = {
	family_scope_poll, field_family_scoped, arena_work_destroy, "identity/field_family_scope/v1"
};

static int field_answer(struct pg_eval *machine, const struct pg_term *family, const void *unused);
static const struct pg_eval_continuation field_answer_continuation = {
	"identity/field_answer/v1", field_answer
};

static int field_answer(struct pg_eval *machine, const struct pg_term *family, const void *unused)
{
	(void)unused;
	const struct pg_term *type;
	if (!pg_identity_action_view(family, &type))
		return with_thunk_family(machine, family, NULL, &pg_field_family_scope_operation);
	struct pg_closure value = *pg_eval_argument(machine, 1);
	if (field_index(machine->current.term->as.reference) < 2)
		return pg_eval_enter(machine, value, 2);
	struct pg_closure action = {pg_reference(machine->output, &identity_action), NULL};
	return pg_eval_apply(machine, action, value, 2);
}

const struct pg_eval_work_operation *pg_identity_work_resolve(const char *name)
{
	static const struct pg_eval_work_operation *const entries[] = {
		&pg_action_scope_operation, &pg_action_result_operation, &pg_scope_operation,
		&pg_action_body_operation, &pg_higher_scope_operation,
		&pg_force_family_result_operation, &pg_field_family_result_operation,
		&pg_force_family_scope_operation, &pg_field_family_scope_operation
	};
	return pg_eval_work_find(name, sizeof(entries) / sizeof(*entries), entries);
}

const struct pg_eval_continuation *pg_identity_continuation_resolve(const char *name)
{
	static const struct pg_eval_continuation *const entries[] = {
		&right_endpoint_continuation, &left_endpoint_continuation, &action_body_continuation, &action_source_continuation, &thunk_return_field_continuation, &field_answer_continuation,
		&diagonal_path_continuation, &diagonal_left_continuation, &diagonal_right_continuation
	};
	return pg_eval_continuation_find(name, sizeof(entries) / sizeof(*entries), entries);
}

int pg_identity_continuation_uses_scope(const struct pg_eval_continuation *continuation)
{
	return continuation == &right_endpoint_continuation
		|| continuation == &left_endpoint_continuation
		|| continuation == &action_body_continuation
		|| continuation == &thunk_return_field_continuation;
}

int pg_identity_dispatch(struct pg_eval *machine)
{
	if (field_index(machine->current.term->as.reference) >= 0) {
		if (!pg_eval_argument(machine, 1)) return 1;
		return pg_eval_demand(machine, 0, &field_answer_continuation, NULL);
	}
	if (machine->current.term->as.reference != &identity_action) return 1;
	if (!pg_eval_argument(machine, 0)) return 1;
	int diagonal = diagonal_argument(machine);
	if (diagonal != 1) return diagonal;
	return pg_eval_demand(machine, 0, &action_source_continuation, NULL);
}
