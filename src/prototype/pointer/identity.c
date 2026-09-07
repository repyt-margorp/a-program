#include "identity.h"
#include "classifier.h"
#include "computation.h"

static const struct pg_object_class identity_class = {"identity-action"};
static const struct pg_object identity_action = {PG_SEMANTIC_OBJECT, &identity_class};

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

struct action_binding {
	const struct pg_object *source;
	const struct pg_object *arguments[3];
};
struct action_scope {
	const struct pg_term *source;
	const struct pg_term *body;
	size_t count;
	struct action_binding *bindings;
};

/* A single direction may act on several curried variables. Their triples
 * belong to the same direction, not to iterated applications of refl. */
static int action_scope(struct pg_eval *machine, const struct pg_term *source, struct action_scope *scope)
{
	*scope = (struct action_scope){source, source, 0, NULL};
	while (scope->body->kind == PG_LAMBDA) {
		++scope->count;
		scope->body = scope->body->as.lambda.body;
	}
	if (scope->count > SIZE_MAX / sizeof(*scope->bindings)) return -1;
	if (scope->count && !pg_eval_argument(machine, 3 * scope->count)) return 1;
	return 0;
}

static int prepare_bindings(struct pg_eval *machine, struct action_scope *scope)
{
	if (!scope->count) return 0;
	scope->bindings = pg_alloc(&machine->temporary, scope->count * sizeof(*scope->bindings));
	if (!scope->bindings) return -1;
	const struct pg_term *source = scope->source;
	for (size_t i = 0; i < scope->count; ++i) {
		scope->bindings[i].source = source->as.lambda.binder;
		for (size_t j = 0; j < 3; ++j) {
			scope->bindings[i].arguments[j] = pg_binder(machine->output);
			if (!scope->bindings[i].arguments[j]) return -1;
		}
		source = source->as.lambda.body;
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

static int enter_action(struct pg_eval *machine, const struct action_scope *scope,
	const struct pg_term *result, size_t discard)
{
	if (!scope->count) return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1 + discard);
	/* Administrative lambdas reuse the evaluator's capture-avoiding closure
 * substitution. Boundary arguments are not evaluated to build the action. */
	for (size_t i = 0; i < discard; ++i) result = pg_lambda(machine->output, pg_binder(machine->output), result);
	for (size_t i = scope->count; i; --i)
		for (size_t j = 3; j; --j)
			result = pg_lambda(machine->output, scope->bindings[i - 1].arguments[j - 1], result);
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1);
}

static int right_endpoint(struct pg_eval *machine, const struct pg_term *right)
{
	struct action_scope scope;
	int status = action_scope(machine, pg_eval_argument(machine, 0)->term, &scope);
	if (status) return status;
	const struct pg_term *content;
	if (!pg_return_type_view(scope.body, &content)) return -1;
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

static int left_endpoint(struct pg_eval *machine, const struct pg_term *left)
{
	struct action_scope scope;
	int status = action_scope(machine, pg_eval_argument(machine, 0)->term, &scope);
	if (status) return status;
	const struct pg_term *content;
	if (!pg_return_type_view(scope.body, &content)) return -1;
	if (!unary_argument(left, &pg_return_operation)) return 1;
	return pg_eval_demand(machine, 2 + 3 * scope.count, right_endpoint);
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

static int action_source(struct pg_eval *machine, const struct pg_term *source)
{
	struct action_scope scope;
	int status = action_scope(machine, source, &scope);
	if (status) return status;
	const struct pg_term *body = scope.body;
	if (body->kind == PG_REFERENCE) {
		size_t center = 0;
		const struct pg_term *binder = source;
		for (size_t i = 0; i < scope.count; ++i) {
			if (binder->as.lambda.binder == body->as.reference) center = 3 * (i + 1);
			binder = binder->as.lambda.body;
		}
		if (center) return pg_eval_enter(machine, *pg_eval_argument(machine, center), 1 + 3 * scope.count);
		if (!scope.count) return 1;
		const struct pg_term *result = pg_identity_action(machine->output, body);
		return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1 + 3 * scope.count);
	}
	const struct pg_term *argument = unary_argument(body, &pg_return_operation);
	if (!argument) argument = unary_argument(body, &pg_thunk_operation);
	if (!argument) argument = unary_argument(body, &pg_force_operation);
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
	if (pg_return_type_view(body, &content)) {
		if (!pg_eval_argument(machine, 2 + 3 * scope.count)) return 1;
		return pg_eval_demand(machine, 1 + 3 * scope.count, left_endpoint);
	}
	const struct pg_term *head = body;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	if (head->kind == PG_REFERENCE && head->as.reference == &identity_action) return 1;
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

int pg_identity_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &identity_action) return 1;
	if (!pg_eval_argument(machine, 0)) return 1;
	return pg_eval_demand(machine, 0, action_source);
}
