#include "view.h"
#include "computation.h"
#include "typing.h"

static const struct pg_term *unary(const struct pg_term *t, const struct pg_object *operation)
{
	if (!t || t->kind != PG_APPLICATION) return NULL;
	const struct pg_term *f = t->as.application.function;
	return f->kind == PG_REFERENCE && f->as.reference == operation ? t->as.application.argument : NULL;
}

struct pg_c_indexed_operand pg_c_indexed_bound(struct pg_c_indexed_operand operand)
{
	for (size_t step = 0; step < 1024 && operand.term && operand.term->kind == PG_REFERENCE; ++step) {
		const struct pg_c_indexed_binding *binding = operand.environment;
		for (; binding && binding->binder != operand.term->as.reference; binding = binding->parent) {}
		if (!binding) return operand;
		operand = binding->value;
	}
	return operand;
}

int pg_c_indexed_head(struct pg_graph *storage, struct pg_c_indexed_operand operand,
	struct pg_c_indexed_operand *head, size_t *count, struct pg_c_indexed_operand *arguments)
{
	struct pg_c_indexed_operand stack[16]; size_t pending = 0;
	if (!storage || !head || !count || !arguments) return -1;
	for (size_t step = 0; step < 1024 && operand.term; ++step) {
		operand = pg_c_indexed_bound(operand);
		const struct pg_term *t = operand.term, *inner, *body;
		/* These exact pure carrier wrappers expose known syntax only. No Fold,
			* Request, arithmetic, source substitution or WHNF machine runs here. */
		if ((inner = unary(t,&pg_total_result_operation)) || (inner = unary(t,&pg_return_operation))) {
			operand.term = inner; continue;
		}
		if ((inner = unary(t,&pg_force_operation)) && (body = unary(inner,&pg_thunk_operation))) {
			operand.term = body; continue;
		}
		if (t->kind == PG_APPLICATION) {
			if (pending == 16) return -1;
			stack[pending++] = (struct pg_c_indexed_operand){t->as.application.argument,operand.environment};
			operand.term = t->as.application.function; continue;
		}
		if (t->kind == PG_LAMBDA && pending) {
			struct pg_c_indexed_binding *binding = pg_alloc(storage,sizeof(*binding)); if (!binding) return -1;
			*binding = (struct pg_c_indexed_binding){operand.environment,t->as.lambda.binder,stack[--pending]};
			operand = (struct pg_c_indexed_operand){t->as.lambda.body,binding}; continue;
		}
		if (t->kind != PG_REFERENCE) return -1;
		*head = operand; *count = pending;
		for (size_t i = 0; i < pending; ++i) arguments[i] = stack[pending-1-i];
		return 0;
	}
	return -1;
}

int pg_c_indexed_view_read(struct pg_graph *storage, const struct pg_term *term, struct pg_c_indexed_view *out)
{
	struct pg_c_indexed_operand head; size_t count;
	if (!out || pg_c_indexed_head(storage,(struct pg_c_indexed_operand){term,NULL},&head,&count,out->arguments)) return -1;
	const struct pg_data_declaration *d = pg_data_declaration_view(head.term->as.reference); if (!d) return -1;
	const struct pg_context *prefix = pg_data_declaration_parameters(d); size_t parameters;
	if (!prefix || pg_context_extension_size(prefix->parent,NULL,&parameters) || count != parameters) return -1;
	/* Preserve the exact source terms and their lexical bindings; no new
		* canonical type, synthesized classifier or nominal family is made. */
	out->declaration = d; out->environment = head.environment; out->count = count; return 0;
}
