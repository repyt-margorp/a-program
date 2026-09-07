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

static const struct pg_object *endpoint_operation(const struct pg_term *type, const struct pg_term **content)
{
	if (pg_return_type_view(type, content)) return &pg_return_operation;
	if (pg_thunk_type_view(type, content)) return &pg_thunk_operation;
	return NULL;
}

static int right_endpoint(struct pg_eval *machine, const struct pg_term *right)
{
	const struct pg_term *type = pg_eval_argument(machine, 0)->term;
	const struct pg_term *content;
	const struct pg_object *operation = endpoint_operation(type, &content);
	if (!operation) return -1;
	const struct pg_term *r = unary_argument(right, operation);
	if (!r) return 1;
	const struct pg_term *l = unary_argument(pg_eval_argument(machine, 1)->term, operation);
	if (!l) return -1;
	const struct pg_term *family = pg_identity_action(machine->output, content);
	const struct pg_term *inner = pg_identity_instance(machine->output, family, l, r);
	const struct pg_term *result = pg_application(machine->output, type->as.application.function, inner);
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 3);
}

static int left_endpoint(struct pg_eval *machine, const struct pg_term *left)
{
	const struct pg_term *content;
	const struct pg_object *operation = endpoint_operation(pg_eval_argument(machine, 0)->term, &content);
	if (!operation) return -1;
	if (!unary_argument(left, operation)) return 1;
	return pg_eval_demand(machine, 2, right_endpoint);
}

static int action_source(struct pg_eval *machine, const struct pg_term *source)
{
	const struct pg_term *argument = unary_argument(source, &pg_return_operation);
	if (!argument) argument = unary_argument(source, &pg_thunk_operation);
	if (argument) {
		const struct pg_term *acted = pg_identity_action(machine->output, argument);
		const struct pg_term *result = pg_application(machine->output, source->as.application.function, acted);
		return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1);
	}
	const struct pg_term *content;
	if (!endpoint_operation(source, &content)) return 1;
	if (!pg_eval_argument(machine, 2)) return 1;
	return pg_eval_demand(machine, 1, left_endpoint);
}

int pg_identity_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &identity_action) return 1;
	if (!pg_eval_argument(machine, 0)) return 1;
	return pg_eval_demand(machine, 0, action_source);
}
