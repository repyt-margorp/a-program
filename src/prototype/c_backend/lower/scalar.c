#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include "host.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct expression;
struct binding {
	const struct binding *parent;
	const struct pg_object *binder;
	struct expression *value;
};

/* Source coordinates memoize translation, not evaluation or accepted facts.
 * value forwards administrative nodes to their scalar C definition. */
struct expression {
	struct pg_index_entry index;
	const struct pg_term *term, *head, *body;
	const struct binding *environment, *locals;
	size_t count, width, parameter;
	const struct pg_term **arguments;
	struct expression **inputs, *value;
	uint64_t bits;
	int computation;
	char arithmetic;
};

struct function {
	struct pg_dag order;
	struct pg_index expressions;
	struct expression **parameters, *body;
	size_t count, width;
	const char *error;
};

static size_t width(const struct pg_object *type)
{
	if (type == pg_host_type("Int32")) return 32;
	if (type == pg_host_type("Int64")) return 64;
	return 0;
}

static const struct pg_term *unary(const struct pg_term *term, const struct pg_object *operation)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *f = term->as.application.function;
	return f->kind == PG_REFERENCE && f->as.reference == operation ? term->as.application.argument : NULL;
}

static const struct pg_term *callable(const struct pg_term *term)
{
	const struct pg_term *argument, *body;
	while ((argument = unary(term, &pg_force_operation)) && (body = unary(argument, &pg_thunk_operation))) term = body;
	return term;
}

static struct expression *expression(struct function *f, const struct pg_term *term,
	const struct binding *environment)
{
	uint64_t hash = (uintptr_t)term ^ ((uint64_t)(uintptr_t)environment * UINT64_C(0x9e3779b97f4a7c15));
	for (struct pg_index_entry *i = pg_index_candidates(&f->expressions, hash); i; i = i->next) {
		struct expression *e = (struct expression *)i;
		if (e->term == term && e->environment == environment) return e;
	}
	struct expression *e = pg_alloc(&f->order.storage, sizeof(*e));
	if (!e) return NULL;
	*e = (struct expression){.term = term, .environment = environment, .locals = environment, .head = term};
	while (e->head->kind == PG_APPLICATION) {
		const struct pg_term *function = callable(e->head);
		if (function->kind == PG_LAMBDA) { e->head = function; break; }
		++e->count;
		e->head = e->head->as.application.function;
	}
	if (e->count >= SIZE_MAX / sizeof(*e->inputs)) return NULL;
	e->arguments = pg_alloc(&f->order.storage, e->count * sizeof(*e->arguments));
	e->inputs = pg_alloc(&f->order.storage, (e->count + 1) * sizeof(*e->inputs));
	if (!e->arguments || !e->inputs) return NULL;
	for (size_t i = e->count; i; --i, term = term->as.application.function) e->arguments[i - 1] = term->as.application.argument;
	if (pg_index_insert(&f->expressions, &e->index, hash)) return NULL;
	return e;
}

static const struct binding *bind(struct function *f, const struct binding *parent,
	const struct pg_object *binder, struct expression *value)
{
	struct binding *b = pg_alloc(&f->order.storage, sizeof(*b));
	if (b) *b = (struct binding){parent, binder, value};
	return b;
}

static int child(struct function *f, struct expression *e, size_t slot,
	const struct pg_term *term, const struct binding *environment, const void **out)
{
	e->inputs[slot] = expression(f, term, environment);
	*out = e->inputs[slot];
	return *out ? 1 : -1;
}

static int forward(struct expression *e, struct expression *input, int computation)
{
	if (!input || !input->value) return -1;
	e->value = input->value;
	e->computation = computation;
	return 0;
}

static char arithmetic(const struct pg_object *object)
{
	const char *name = pg_host_function_descriptor(object);
	if (!name) return 0;
	static const char *const names[] = {"host/int32/add/v1", "host/int32/sub/v1", "host/int32/mul/v1", "host/int32/neg/v1",
		"host/int64/add/v1", "host/int64/sub/v1", "host/int64/mul/v1", "host/int64/neg/v1"};
	for (size_t i = 0; i < 8; ++i) if (!strcmp(name, names[i])) return "+-*~"[i % 4];
	return 0;
}

/* pg_dag supplies the iterative postorder stack. A child's scalar definition
 * is available when constructing the next lexical body dependency. */
static int lower_child(void *owner, const void *key, size_t slot, const void **out)
{
	struct function *f = owner;
	struct expression *e = (struct expression *)key;
	f->error = "unsupported scalar expression or operand representation";
	if (e->term->kind == PG_REFERENCE) {
		const struct pg_object *object = e->term->as.reference, *type;
		if (object->kind == PG_BINDER) {
			for (const struct binding *b = e->environment; b; b = b->parent)
				if (b->binder == object) return forward(e, b->value, 0);
			return -1;
		}
		size_t count;
		const unsigned char *bytes;
		if (!pg_host_literal_view(object, &type, &count, &bytes) || !(e->width = width(type)) || count != e->width / 8) return -1;
		for (size_t i = 0; i < count; ++i) e->bits = (e->bits << 8) | bytes[i];
		e->value = e;
		return 0;
	}
	if (!e->count) return -1;
	const struct pg_object *operation = e->head->kind == PG_REFERENCE ? e->head->as.reference : NULL;
	if (operation == &pg_return_operation || operation == &pg_total_result_operation) {
		if (e->count != 1) return -1;
		if (!slot) return child(f, e, 0, e->arguments[0], e->environment, out);
		int returned = operation == &pg_return_operation;
		if (e->inputs[0]->computation == returned) return -1;
		return forward(e, e->inputs[0], returned);
	}
	if (operation == &pg_force_operation) {
		if (e->count != 1) return -1;
		const struct pg_term *body = unary(e->arguments[0], &pg_thunk_operation);
		if (!body) return -1;
		if (!slot) return child(f, e, 0, body, e->environment, out);
		return forward(e, e->inputs[0], e->inputs[0]->computation);
	}
	if (operation == &pg_fold_operation) {
		if (e->count != 2) return -1;
		const struct pg_term *continuation = callable(e->arguments[1]);
		if (continuation->kind != PG_LAMBDA) return -1;
		if (!slot) return child(f, e, 0, e->arguments[0], e->environment, out);
		if (slot == 1) {
			if (!e->inputs[0]->computation) return -1;
			e->locals = bind(f, e->environment, continuation->as.lambda.binder, e->inputs[0]->value);
			return e->locals ? child(f, e, 1, continuation->as.lambda.body, e->locals, out) : -1;
		}
		if (!e->inputs[1]->computation) return -1;
		return forward(e, e->inputs[1], 1);
	}
	if (operation && (e->arithmetic = arithmetic(operation))) {
		const struct pg_object *domain, *result;
		size_t arity;
		if (!pg_host_function_view(operation, &domain, &result, &arity) || arity != e->count) return -1;
		if (slot < arity) return child(f, e, slot, e->arguments[slot], e->environment, out);
		e->width = width(result);
		for (size_t i = 0; i < arity; ++i)
			if (e->inputs[i]->computation || e->inputs[i]->value->width != width(domain)) return -1;
		e->value = e; e->computation = 1;
		return e->width ? 0 : -1;
	}
	/* Known first-order lambdas are specialized with scalar C definitions. No
	 * closure argument, dynamic call, recursive beta evaluator or source rewrite. */
	if (!slot) e->body = callable(e->head);
	if (slot && slot <= e->count) {
		if (e->inputs[slot - 1]->computation) return -1;
		e->locals = bind(f, e->locals, e->body->as.lambda.binder, e->inputs[slot - 1]->value);
		if (!e->locals) return -1;
		e->body = e->body->as.lambda.body;
	}
	if (slot < e->count) {
		if (e->body->kind != PG_LAMBDA) return -1;
		return child(f, e, slot, e->arguments[slot], e->environment, out);
	}
	if (slot == e->count) return child(f, e, slot, e->body, e->locals, out);
	return forward(e, e->inputs[e->count], e->inputs[e->count]->computation);
}

static int prepare(struct function *f, const struct pg_occurrence *subject)
{
	if (pg_dag_init(&f->order, lower_child, f) || pg_index_init(&f->expressions)) return -1;
	f->error = "expected closed fixed-width values or pure total first-order functions";
	if (!subject || subject->context || !subject->core || !subject->classifier) return -1;
	const struct pg_term *term = subject->core, *type = subject->classifier, *content;
	int computation = subject->judgement == PG_JUDGEMENT_COMPUTATION;
	if (subject->judgement == PG_JUDGEMENT_VALUE) {
		if (pg_thunk_type_view(type, &content)) {
			term = unary(term, &pg_thunk_operation);
			if (!term) return -1;
			type = content; computation = 1;
		}
	} else if (!computation) return -1;
	const struct pg_term *domain, *codomain, *signature = type;
	const struct pg_object *binder;
	while (pg_pi_view(signature, &domain, &binder, &codomain)) { ++f->count; signature = codomain; }
	if (f->count > SIZE_MAX / sizeof(*f->parameters)) return -1;
	f->parameters = pg_alloc(&f->order.storage, f->count * sizeof(*f->parameters));
	if (!f->parameters) return -1;
	const struct binding *environment = NULL;
	for (size_t i = 0; i < f->count; ++i) {
		if (!computation || !pg_pi_view(type, &domain, &binder, &codomain) || domain->kind != PG_REFERENCE) return -1;
		term = callable(term);
		if (term->kind != PG_LAMBDA) return -1;
		struct expression *parameter = pg_alloc(&f->order.storage, sizeof(*parameter));
		if (!parameter) return -1;
		*parameter = (struct expression){.parameter = i + 1, .width = width(domain->as.reference)};
		if (!parameter->width) return -1;
		parameter->value = parameter;
		f->parameters[i] = parameter;
		environment = bind(f, environment, term->as.lambda.binder, parameter);
		if (!environment) return -1;
		term = term->as.lambda.body; type = codomain;
	}
	if (computation) {
		enum pg_totality totality;
		if (!pg_pure_computation_type_view(type, &totality, &content) || totality != PG_TOTALITY_TOTAL) return -1;
		type = content;
	}
	if (type->kind != PG_REFERENCE || !(f->width = width(type->as.reference))) return -1;
	f->body = expression(f, term, environment);
	if (!f->body || pg_dag_add(&f->order, f->body)) return -1;
	return f->body->computation == computation && f->body->value->width == f->width ? 0 : -1;
}

static void parameters(FILE *out, const struct function *f)
{
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "int%zu_t a%zu, ", f->parameters[i]->width, i + 1);
	fprintf(out, "int%zu_t *out", f->width);
}

static void arguments(FILE *out, const struct function *f)
{
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "a%zu, ", i + 1);
	fputs("out", out);
}

static void value(FILE *out, const struct function *f, const struct expression *e)
{
	e = e->value;
	if (e->parameter) fprintf(out, "((uint64_t)(uint%zu_t)a%zu)", e->width, e->parameter);
	else fprintf(out, "v%zu", pg_dag_find(&f->order, e)->id);
}

static void emit_function(FILE *out, const struct function *f, size_t id)
{
	fprintf(out, "static int c%zu(", id); parameters(out, f);
	fputs(")\n{\n\tif (!out) return 1;\n", out);
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "\t(void)a%zu;\n", i + 1);
	for (const struct pg_dag_node *n = f->order.first; n; n = n->next) {
		const struct expression *e = n->key;
		if (e->value != e) continue;
		fprintf(out, "\tuint64_t v%zu = (uint%zu_t)(", n->id, e->width);
		if (!e->arithmetic) fprintf(out, "UINT64_C(0x%" PRIx64 ")", e->bits);
		else if (e->arithmetic == '~') { fputs("UINT64_C(0) - ", out); value(out, f, e->inputs[0]); }
		else { value(out, f, e->inputs[0]); fprintf(out, " %c ", e->arithmetic); value(out, f, e->inputs[1]); }
		fprintf(out, ");\n\t(void)v%zu;\n", n->id);
	}
	fputs("\tuint64_t result = ", out); value(out, f, f->body);
	fprintf(out, ";\n\t*out = result <= INT%zu_MAX ? (int%zu_t)result : -1 - (int%zu_t)(UINT%zu_MAX - result);\n\treturn 0;\n}\n\n",
		f->width, f->width, f->width, f->width);
}

int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error)
{
	if (!source || !header || !error) return -1;
	*error = "invalid scalar exports or entry";
	if (!pg_c_export_names(count, exports) || (entry != SIZE_MAX && entry >= count)) return -1;
	struct pg_dag roots;
	if (pg_dag_init(&roots, NULL, NULL)) return -1;
	struct function *functions = NULL;
	int status = -1;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&roots, exports[i].subject)) goto done;
	functions = calloc(roots.count, sizeof(*functions));
	if (!functions) goto done;
	for (const struct pg_dag_node *n = roots.first; n; n = n->next) {
		if (!prepare(&functions[n->id - 1], n->key)) continue;
		*error = functions[n->id - 1].error ? functions[n->id - 1].error : "scalar lowering allocation failed";
		goto done;
	}
	if (entry != SIZE_MAX && functions[pg_dag_find(&roots, exports[entry].subject)->id - 1].count) {
		*error = "native executable entry cannot require arguments"; goto done;
	}
	fputs("/* A Program scalar_direct_v1: fixed-width pure total scalar realization. */\n#include <stdint.h>\n#include <limits.h>\n"
		"_Static_assert(sizeof(int) <= sizeof(uint32_t), \"unsupported integer promotion model\");\n\n", source);
	fputs("/* Scalar ABI 1: 0 success; 1 null output. Output must name writable storage. */\n#include <stdint.h>\n"
		"#ifndef AP_C_SCALAR_ABI\n#define AP_C_SCALAR_ABI 1\n#elif AP_C_SCALAR_ABI != 1\n#error incompatible_A_Program_scalar_ABI\n#endif\n"
		"#ifdef __cplusplus\nextern \"C\" {\n#endif\n", header);
	for (const struct pg_dag_node *n = roots.first; n; n = n->next) emit_function(source, &functions[n->id - 1], n->id);
	for (size_t i = 0; i < count; ++i) {
		size_t id = pg_dag_find(&roots, exports[i].subject)->id;
		const struct function *f = &functions[id - 1];
		fprintf(header, "int ap_export_%s(", exports[i].alias); parameters(header, f); fputs(");\n", header);
		fprintf(source, "int ap_export_%s(", exports[i].alias); parameters(source, f);
		fprintf(source, ")\n{\n\treturn c%zu(", id); arguments(source, f); fputs(");\n}\n\n", source);
	}
	fputs("#ifdef __cplusplus\n}\n#endif\n", header);
	if (entry != SIZE_MAX) {
		const struct function *f = &functions[pg_dag_find(&roots, exports[entry].subject)->id - 1];
		fprintf(source, "int main(void)\n{\n\tint%zu_t result;\n\treturn ap_export_%s(&result);\n}\n", f->width, exports[entry].alias);
	}
	*error = "cannot write native C output";
	status = ferror(source) || ferror(header) ? -1 : 0;
done:
	if (functions) for (size_t i = 0; i < roots.count; ++i) {
		pg_index_destroy(&functions[i].expressions);
		pg_dag_destroy(&functions[i].order);
	}
	free(functions);
	pg_dag_destroy(&roots);
	return status;
}
