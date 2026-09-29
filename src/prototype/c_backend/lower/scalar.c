#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include "host.h"
#include "support.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct expression;
struct binding {
	const struct binding *parent;
	const struct pg_object *binder;
	struct expression *value;
};

/* Arguments retain their lexical scope when Fold exposes another function.
 * Native parameters have value instead of term; these are target operands. */
struct argument {
	struct pg_index_entry index;
	const struct pg_term *term;
	const struct binding *environment;
	struct expression *value;
	const struct argument *next;
};

struct expression {
	struct pg_index_entry index;
	struct function *function, *callee;
	const struct pg_term *term, *head;
	const struct binding *environment;
	const struct argument *pending, *arguments, *cursor;
	size_t count, width, parameter, capture_count, id;
	struct expression **inputs, *value, *next;
	const struct binding **captures;
	uint64_t bits;
	int computation;
	char arithmetic;
};

struct function {
	struct pg_index_entry index;
	struct module *module;
	struct function *next;
	const struct pg_term *lambda;
	struct pg_index expressions;
	struct expression **parameters, *body, *first, *last;
	const struct pg_object **captures;
	size_t id, count, capture_count, definitions;
};

struct module {
	struct pg_dag order;
	struct pg_index functions, arguments;
	struct function *first, *last;
	size_t count;
	const char *error;
};

static uint64_t mix(uint64_t hash, uintptr_t value)
{
	return (hash ^ value) * UINT64_C(1099511628211);
}

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

static const struct argument *argument(struct module *m, const struct pg_term *term,
	const struct binding *environment, struct expression *value, const struct argument *next)
{
	uint64_t hash = mix(mix(mix((uintptr_t)term, (uintptr_t)environment), (uintptr_t)value), (uintptr_t)next);
	for (struct pg_index_entry *i = pg_index_candidates(&m->arguments, hash); i; i = i->next) {
		struct argument *a = (struct argument *)i;
		if (a->term == term && a->environment == environment && a->value == value && a->next == next) return a;
	}
	struct argument *a = pg_alloc(&m->order.storage, sizeof(*a));
	if (!a) return NULL;
	*a = (struct argument){.term = term, .environment = environment, .value = value, .next = next};
	return pg_index_insert(&m->arguments, &a->index, hash) ? NULL : a;
}

static struct expression *expression(struct function *f, const struct pg_term *term,
	const struct binding *environment, const struct argument *pending)
{
	uint64_t hash = mix(mix((uintptr_t)term, (uintptr_t)environment), (uintptr_t)pending);
	for (struct pg_index_entry *i = pg_index_candidates(&f->expressions, hash); i; i = i->next) {
		struct expression *e = (struct expression *)i;
		if (e->term == term && e->environment == environment && e->pending == pending) return e;
	}
	struct expression *e = pg_alloc(&f->module->order.storage, sizeof(*e));
	if (!e) return NULL;
	*e = (struct expression){.function = f, .term = term, .head = term,
		.environment = environment, .pending = pending, .arguments = pending};
	while (e->head->kind == PG_APPLICATION) {
		const struct pg_term *function = callable(e->head);
		if (function->kind == PG_LAMBDA) { e->head = function; break; }
		e->arguments = argument(f->module, e->head->as.application.argument, environment, NULL, e->arguments);
		if (!e->arguments) return NULL;
		e->head = e->head->as.application.function;
	}
	for (const struct argument *a = e->arguments; a; a = a->next) {
		if (e->count >= SIZE_MAX / sizeof(*e->inputs) - 1) return NULL;
		++e->count;
	}
	e->cursor = e->arguments;
	e->inputs = pg_alloc(&f->module->order.storage, (e->count + 1) * sizeof(*e->inputs));
	if (!e->inputs || pg_index_insert(&f->expressions, &e->index, hash)) return NULL;
	return e;
}

static const struct binding *bind(struct function *f, const struct binding *parent,
	const struct pg_object *binder, struct expression *value)
{
	struct binding *b = pg_alloc(&f->module->order.storage, sizeof(*b));
	if (b) *b = (struct binding){parent, binder, value};
	return b;
}

static int child(struct expression *e, size_t slot, const struct pg_term *term,
	const struct binding *environment, const struct argument *pending, const void **out)
{
	e->inputs[slot] = expression(e->function, term, environment, pending);
	*out = e->inputs[slot];
	return *out ? 1 : -1;
}

static int operand(struct expression *e, size_t slot, const struct argument *a, const void **out)
{
	if (!a->value) return child(e, slot, a->term, a->environment, NULL, out);
	e->inputs[slot] = a->value;
	return 2;
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

static struct function *function(struct module *m, size_t count)
{
	if (count > SIZE_MAX / sizeof(struct expression *) || m->count == SIZE_MAX) return NULL;
	struct function *f = pg_alloc(&m->order.storage, sizeof(*f));
	if (!f) return NULL;
	f->module = m; f->id = ++m->count;
	if (m->last) m->last->next = f;
	else m->first = f;
	m->last = f;
	if (pg_index_init(&f->expressions)) return NULL;
	f->count = count;
	f->parameters = pg_alloc(&m->order.storage, count * sizeof(*f->parameters));
	if (!f->parameters) return NULL;
	for (size_t i = 0; i < count; ++i) {
		struct expression *p = pg_alloc(&m->order.storage, sizeof(*p));
		if (!p) return NULL;
		*p = (struct expression){.function = f, .parameter = i + 1};
		p->value = p; f->parameters[i] = p;
	}
	return f;
}

/* Capture only free source binders, in stable lexical order. The existing
 * immutable support trie answers membership without rescanning the body. */
static int captures(struct expression *e)
{
	size_t capacity = 0;
	for (const struct binding *b = e->environment; b; b = b->parent) ++capacity;
	if (capacity > SIZE_MAX / sizeof(*e->captures)) return -1;
	e->captures = pg_alloc(&e->function->module->order.storage, capacity * sizeof(*e->captures));
	if (!e->captures) return -1;
	struct pg_dag seen;
	if (pg_dag_init(&seen, NULL, NULL)) return -1;
	int status = -1;
	for (const struct binding *b = e->environment; b; b = b->parent) {
		int member = pg_support_contains(e->head, b->binder);
		if (member < 0) goto done;
		if (!member || pg_dag_find(&seen, b->binder)) continue;
		if (pg_dag_add(&seen, b->binder)) goto done;
		e->captures[e->capture_count++] = b;
	}
	status = 0;
done:
	pg_dag_destroy(&seen);
	return status;
}

static struct function *callee(struct expression *e)
{
	struct module *m = e->function->module;
	if (captures(e)) return NULL;
	uint64_t hash = mix((uintptr_t)e->head, e->count);
	for (size_t i = 0; i < e->count; ++i) hash = mix(hash, e->inputs[i]->value->width);
	for (size_t i = 0; i < e->capture_count; ++i)
		hash = mix(mix(hash, (uintptr_t)e->captures[i]->binder), e->captures[i]->value->width);
	for (struct pg_index_entry *entry = pg_index_candidates(&m->functions, hash); entry; entry = entry->next) {
		struct function *f = (struct function *)entry;
		if (f->lambda != e->head || f->capture_count != e->capture_count || f->count != e->count + e->capture_count) continue;
		size_t i = 0;
		while (i < e->count && f->parameters[i]->width == e->inputs[i]->value->width) ++i;
		if (i != e->count) continue;
		i = 0;
		while (i < e->capture_count && f->captures[i] == e->captures[i]->binder &&
			f->parameters[e->count + i]->width == e->captures[i]->value->width) ++i;
		if (i == e->capture_count) return f;
	}
	if (e->count > SIZE_MAX - e->capture_count) return NULL;
	struct function *f = function(m, e->count + e->capture_count);
	if (!f) return NULL;
	f->lambda = e->head; f->capture_count = e->capture_count;
	f->captures = pg_alloc(&m->order.storage, e->capture_count * sizeof(*f->captures));
	if (!f->captures) return NULL;
	const struct binding *environment = NULL;
	for (size_t i = 0; i < e->capture_count; ++i) {
		struct expression *p = f->parameters[e->count + i];
		p->width = e->captures[i]->value->width;
		f->captures[i] = e->captures[i]->binder;
		environment = bind(f, environment, f->captures[i], p);
		if (!environment) return NULL;
	}
	const struct pg_term *body = e->head;
	size_t consumed = 0;
	for (size_t i = 0; i < e->count; ++i) {
		f->parameters[i]->width = e->inputs[i]->value->width;
		if (body->kind != PG_LAMBDA) continue;
		environment = bind(f, environment, body->as.lambda.binder, f->parameters[i]);
		if (!environment) return NULL;
		body = body->as.lambda.body; ++consumed;
	}
	const struct argument *pending = NULL;
	for (size_t i = e->count; i > consumed; --i) {
		pending = argument(m, NULL, NULL, f->parameters[i - 1], pending);
		if (!pending) return NULL;
	}
	f->body = expression(f, body, environment, pending);
	if (!f->body || pg_index_insert(&m->functions, &f->index, hash)) return NULL;
	return f;
}

/* Dependencies include private callee bodies, so the same iterative walker
 * orders local scalar definitions and rejects recursive target specializations. */
static int lower(struct expression *e, size_t slot, const void **out)
{
	if (e->head->kind == PG_REFERENCE && !e->count) {
		const struct pg_object *object = e->head->as.reference, *type;
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
	const struct argument *a = e->arguments;
	if (operation == &pg_return_operation || operation == &pg_total_result_operation) {
		if (e->count != 1) return -1;
		if (!slot) return operand(e, 0, a, out);
		int returned = operation == &pg_return_operation;
		if (e->inputs[0]->computation == returned) return -1;
		return forward(e, e->inputs[0], returned);
	}
	if (operation == &pg_force_operation) {
		const struct pg_term *body = a->term ? unary(a->term, &pg_thunk_operation) : NULL;
		if (!body) return -1;
		if (!slot) return child(e, 0, body, a->environment, a->next, out);
		return forward(e, e->inputs[0], e->inputs[0]->computation);
	}
	if (operation == &pg_fold_operation) {
		if (e->count < 2 || !a->next->term) return -1;
		const struct argument *k = a->next;
		const struct pg_term *continuation = callable(k->term);
		if (continuation->kind != PG_LAMBDA) return -1;
		if (!slot) return operand(e, 0, a, out);
		if (slot == 1) {
			if (!e->inputs[0]->computation) return -1;
			const struct binding *local = bind(e->function, k->environment, continuation->as.lambda.binder, e->inputs[0]->value);
			return local ? child(e, 1, continuation->as.lambda.body, local, k->next, out) : -1;
		}
		if (!e->inputs[1]->computation) return -1;
		return forward(e, e->inputs[1], 1);
	}
	if (operation && (e->arithmetic = arithmetic(operation))) {
		const struct pg_object *domain, *result;
		size_t arity;
		if (!pg_host_function_view(operation, &domain, &result, &arity) || arity != e->count) return -1;
		if (slot < arity) {
			a = e->cursor; e->cursor = a->next;
			return operand(e, slot, a, out);
		}
		e->width = width(result);
		for (size_t i = 0; i < arity; ++i)
			if (e->inputs[i]->computation || e->inputs[i]->value->width != width(domain)) return -1;
		e->value = e; e->computation = 1;
		return e->width ? 0 : -1;
	}
	if (e->head->kind != PG_LAMBDA) return -1;
	if (slot < e->count) {
		a = e->cursor; e->cursor = a->next;
		return operand(e, slot, a, out);
	}
	if (slot == e->count) {
		for (size_t i = 0; i < e->count; ++i) if (e->inputs[i]->computation) return -1;
		e->callee = callee(e);
		if (!e->callee) return -1;
		*out = e->callee->body;
		return 1;
	}
	if (!e->callee->body->computation) return -1;
	e->width = e->callee->body->value->width;
	e->value = e; e->computation = 1;
	return 0;
}

static int lower_child(void *owner, const void *key, size_t slot, const void **out)
{
	struct module *m = owner;
	struct expression *e = (struct expression *)key;
	m->error = "unsupported scalar expression, argument or capture representation";
	int status = lower(e, slot, out);
	if (status == 0 && e->value == e) {
		struct function *f = e->function;
		if (f->definitions == SIZE_MAX) return -1;
		e->id = ++f->definitions;
		if (f->last) f->last->next = e;
		else f->first = e;
		f->last = e;
	}
	return status;
}

static struct function *prepare(struct module *m, const struct pg_occurrence *subject)
{
	m->error = "expected closed fixed-width values or pure total first-order functions";
	if (!subject || subject->context || !subject->core || !subject->classifier) return NULL;
	const struct pg_term *term = subject->core, *type = subject->classifier, *content;
	int computation = subject->judgement == PG_JUDGEMENT_COMPUTATION;
	if (subject->judgement == PG_JUDGEMENT_VALUE) {
		if (pg_thunk_type_view(type, &content)) {
			term = unary(term, &pg_thunk_operation);
			if (!term) return NULL;
			type = content; computation = 1;
		}
	} else if (!computation) return NULL;
	const struct pg_term *domain, *codomain, *signature = type;
	const struct pg_object *binder;
	size_t count = 0;
	while (pg_pi_view(signature, &domain, &binder, &codomain)) { ++count; signature = codomain; }
	struct function *f = function(m, count);
	if (!f) return NULL;
	for (size_t i = 0; i < count; ++i) {
		if (!computation || !pg_pi_view(type, &domain, &binder, &codomain) || domain->kind != PG_REFERENCE) return NULL;
		if (!(f->parameters[i]->width = width(domain->as.reference))) return NULL;
		type = codomain;
	}
	if (computation) {
		enum pg_totality totality;
		if (!pg_pure_computation_type_view(type, &totality, &content) || totality != PG_TOTALITY_TOTAL) return NULL;
		type = content;
	}
	if (type->kind != PG_REFERENCE || !width(type->as.reference)) return NULL;
	const struct argument *pending = NULL;
	for (size_t i = count; i; --i) {
		pending = argument(m, NULL, NULL, f->parameters[i - 1], pending);
		if (!pending) return NULL;
	}
	f->body = expression(f, term, NULL, pending);
	if (!f->body || pg_dag_add(&m->order, f->body)) return NULL;
	return f->body->computation == computation && f->body->value->width == width(type->as.reference) ? f : NULL;
}

static void parameters(FILE *out, const struct function *f)
{
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "int%zu_t a%zu, ", f->parameters[i]->width, i + 1);
	fprintf(out, "int%zu_t *out", f->body->value->width);
}

static void value(FILE *out, const struct expression *e)
{
	e = e->value;
	if (e->parameter) fprintf(out, "a%zu", e->parameter);
	else fprintf(out, "v%zu", e->id);
}

static void signature(FILE *out, const struct function *f)
{
	fprintf(out, "static uint64_t c%zu(", f->id);
	if (!f->count) fputs("void", out);
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "%suint64_t a%zu", i ? ", " : "", i + 1);
	fputc(')', out);
}

static void emit_function(FILE *out, const struct function *f)
{
	signature(out, f); fputs("\n{\n", out);
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "\t(void)a%zu;\n", i + 1);
	for (const struct expression *e = f->first; e; e = e->next) {
		size_t id = e->id;
		fprintf(out, "\tuint64_t v%zu = (uint%zu_t)(", id, e->width);
		if (e->callee) {
			fprintf(out, "c%zu(", e->callee->id);
			for (size_t i = 0; i < e->count; ++i) { if (i) fputs(", ", out); value(out, e->inputs[i]); }
			for (size_t i = 0; i < e->capture_count; ++i) { fputs(", ", out); value(out, e->captures[i]->value); }
			fputc(')', out);
		} else if (!e->arithmetic) fprintf(out, "UINT64_C(0x%" PRIx64 ")", e->bits);
		else if (e->arithmetic == '~') { fputs("UINT64_C(0) - ", out); value(out, e->inputs[0]); }
		else { value(out, e->inputs[0]); fprintf(out, " %c ", e->arithmetic); value(out, e->inputs[1]); }
		fprintf(out, ");\n\t(void)v%zu;\n", id);
	}
	fputs("\treturn ", out); value(out, f->body); fputs(";\n}\n\n", out);
}

int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error)
{
	if (!source || !header || !error) return -1;
	*error = "invalid scalar exports or entry";
	if (!pg_c_export_names(count, exports) || (entry != SIZE_MAX && entry >= count)) return -1;
	struct module m = {0};
	struct pg_dag roots;
	if (pg_dag_init(&roots, NULL, NULL)) return -1;
	struct function **functions = NULL;
	int status = -1;
	if (pg_dag_init(&m.order, lower_child, &m) || pg_index_init(&m.functions) || pg_index_init(&m.arguments)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&roots, exports[i].subject)) goto done;
	functions = calloc(roots.count, sizeof(*functions));
	if (!functions) goto done;
	for (const struct pg_dag_node *n = roots.first; n; n = n->next) {
		functions[n->id - 1] = prepare(&m, n->key);
		if (functions[n->id - 1]) continue;
		*error = m.error;
		goto done;
	}
	if (entry != SIZE_MAX && functions[pg_dag_find(&roots, exports[entry].subject)->id - 1]->count) {
		*error = "native executable entry cannot require arguments"; goto done;
	}
	fputs("/* A Program scalar_direct_v1: fixed-width pure total scalar realization. */\n#include <stdint.h>\n#include <limits.h>\n"
		"_Static_assert(sizeof(int) <= sizeof(uint32_t), \"unsupported integer promotion model\");\n\n", source);
	fputs("/* Scalar ABI 1: 0 success; 1 null output. Output must name writable storage. */\n#include <stdint.h>\n"
		"#ifndef AP_C_SCALAR_ABI\n#define AP_C_SCALAR_ABI 1\n#elif AP_C_SCALAR_ABI != 1\n#error incompatible_A_Program_scalar_ABI\n#endif\n"
		"#ifdef __cplusplus\nextern \"C\" {\n#endif\n", header);
	for (const struct function *f = m.first; f; f = f->next) { signature(source, f); fputs(";\n", source); }
	fputc('\n', source);
	for (const struct function *f = m.first; f; f = f->next) emit_function(source, f);
	for (size_t i = 0; i < count; ++i) {
		const struct function *f = functions[pg_dag_find(&roots, exports[i].subject)->id - 1];
		fprintf(header, "int ap_export_%s(", exports[i].alias); parameters(header, f); fputs(");\n", header);
		fprintf(source, "int ap_export_%s(", exports[i].alias); parameters(source, f);
		fprintf(source, ")\n{\n\tif (!out) return 1;\n\tuint64_t result = c%zu(", f->id);
		for (size_t j = 0; j < f->count; ++j)
			fprintf(source, "%s(uint%zu_t)a%zu", j ? ", " : "", f->parameters[j]->width, j + 1);
		size_t w = f->body->value->width;
		fprintf(source, ");\n\t*out = result <= INT%zu_MAX ? (int%zu_t)result : -1 - (int%zu_t)(UINT%zu_MAX - result);\n\treturn 0;\n}\n\n", w, w, w, w);
	}
	fputs("#ifdef __cplusplus\n}\n#endif\n", header);
	if (entry != SIZE_MAX) {
		const struct function *f = functions[pg_dag_find(&roots, exports[entry].subject)->id - 1];
		fprintf(source, "int main(void)\n{\n\tint%zu_t result;\n\treturn ap_export_%s(&result);\n}\n", f->body->value->width, exports[entry].alias);
	}
	*error = "cannot write native C output";
	status = ferror(source) || ferror(header) ? -1 : 0;
done:
	for (struct function *f = m.first; f; f = f->next) pg_index_destroy(&f->expressions);
	free(functions);
	pg_index_destroy(&m.functions); pg_index_destroy(&m.arguments);
	pg_dag_destroy(&m.order); pg_dag_destroy(&roots);
	return status;
}
