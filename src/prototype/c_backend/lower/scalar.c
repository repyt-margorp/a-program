#include "scalar.h"
#include "representation.h"
#include "nodes.h"
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
	const struct pg_term *term, *head, *suspended;
	const struct binding *environment;
	const struct argument *pending, *arguments, *cursor;
	size_t count, parameter, capture_count, id;
	const struct pg_c_representation *type, *match, *expected;
	const struct pg_object *nominal;
	const struct pg_c_constructor_representation *construction;
	struct expression *container, *foreign;
	size_t constructor, field;
	struct expression **branches;
	struct expression **inputs, *value, *next;
	const struct binding **captures;
	uint64_t bits;
	int computation;
	char arithmetic;
	int recursive_template, delayed;
	struct function *recursion;
};

struct function {
	struct pg_index_entry index;
	struct module *module;
	struct function *next;
	const struct pg_term *lambda;
	struct pg_index expressions;
	struct expression **parameters, *body, *first, *last;
	const struct pg_object **captures;
	struct expression **static_captures;
	size_t id, count, capture_count, definitions, source_count;
	const struct pg_c_representation *result;
	int recursive;
};

struct predicate {
	struct predicate *next;
	struct pg_c_representation type;
};

struct module {
	struct pg_dag order;
	struct pg_index functions, arguments, results;
	struct pg_c_representations representations;
	struct function *first, *last;
	size_t count;
	const char *error;
	int callbacks;
	int native_predicates;
	struct predicate *predicates;
};

struct result_entry {
	struct pg_index_entry index;
	const struct pg_term *term;
	const struct pg_c_representation *type;
};

static const struct pg_c_representation *result_type(const struct module *m, const struct pg_term *term)
{
	for (struct pg_index_entry *i = pg_index_candidates(&m->results, (uintptr_t)term); i; i = i->next) {
		const struct result_entry *r = (const struct result_entry *)i;
		if (r->term == term) return r->type;
	}
	return NULL;
}

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused;
	const struct pg_occurrence *s = key;
	if (slot < s->operand_count) { *out = s->operands[slot]; return *out ? 1 : 2; }
	slot -= s->operand_count;
	if (!slot) { *out = s->type; return *out ? 1 : 2; }
	if (slot == 1) { *out = s->origin; return *out ? 1 : 2; }
	slot -= 2;
	const struct pg_context_map *const *maps = pg_occurrence_maps(s);
	for (size_t i = 0; i <= s->map_count; ++i) {
		const struct pg_context_map *map = i ? maps[i - 1] : s->map;
		if (!map) continue;
		if (slot < map->count) { *out = map->images[slot]; return *out ? 1 : 2; }
		slot -= map->count;
	}
	return 0;
}

/* Read existing descriptive result classifiers of admitted induction inputs.
	* This avoids guessing an inner fold's result from its caller's result. */
static int induction_results(struct module *m, size_t count, const struct pg_c_export *exports)
{
	struct pg_dag subjects;
	if (pg_dag_init(&subjects, subject_child, NULL)) return -1;
	int status = -1;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&subjects, exports[i].subject)) goto done;
	for (const struct pg_dag_node *n = subjects.first; n; n = n->next) {
		const struct pg_occurrence *s = n->key;
		const struct pg_term *content;
		enum pg_totality totality;
		if (!s->induction || !s->core || !s->classifier) continue;
		const struct pg_term *classifier = s->classifier, *domain, *codomain;
		const struct pg_object *binder;
		while (pg_pi_view(classifier, &domain, &binder, &codomain)) {
			if (!pg_c_representation_term(&m->representations, domain)) break;
			classifier = codomain;
		}
		if (!pg_pure_computation_type_view(classifier, &totality, &content) ||
			totality != PG_TOTALITY_TOTAL) continue;
		const struct pg_c_representation *type = pg_c_representation_term(&m->representations, content);
		if (!type) continue;
		const struct pg_c_representation *old = result_type(m, s->core);
		if (old) { if (old != type) goto done; continue; }
		struct result_entry *r = pg_alloc(&m->order.storage, sizeof(*r));
		if (!r) goto done;
		*r = (struct result_entry){.term = s->core, .type = type};
		if (pg_index_insert(&m->results, &r->index, (uintptr_t)s->core)) goto done;
	}
	status = 0;
done:
	pg_dag_destroy(&subjects);
	return status;
}

static uint64_t mix(uint64_t hash, uintptr_t value)
{
	return (hash ^ value) * UINT64_C(1099511628211);
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

/* A known lambda may be wrapped by Force/Thunk or partially applied. Keep
	* its computation suspended; this inspection does not run its arguments. */
static int known_lambda(const struct pg_term *term)
{
	for (;;) {
		term = callable(term);
		if (term->kind == PG_LAMBDA) return 1;
		if (term->kind != PG_APPLICATION) return 0;
		term = term->as.application.function;
	}
}

/* Recognize exactly the existing erased recursive-Match builder. Admission
	* and totality belong to the source driver, not this target pattern. */
static const struct pg_term *recursive_template(const struct pg_term *term)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *unfold = term->as.application.function;
	if (unfold != term->as.application.argument || unfold->kind != PG_LAMBDA) return NULL;
	const struct pg_term *body = unfold->as.lambda.body;
	if (body->kind != PG_APPLICATION) return NULL;
	const struct pg_term *self = body->as.application.argument, *function = body->as.application.function;
	if (self->kind != PG_APPLICATION || self->as.application.function->kind != PG_REFERENCE ||
		self->as.application.argument->kind != PG_REFERENCE ||
		self->as.application.function->as.reference != unfold->as.lambda.binder ||
		self->as.application.argument->as.reference != unfold->as.lambda.binder ||
		function->kind != PG_LAMBDA || function->as.lambda.body->kind != PG_LAMBDA) return NULL;
	return function;
}

static struct expression *lookup(const struct binding *environment, const struct pg_object *binder)
{
	for (const struct binding *b = environment; b; b = b->parent) if (b->binder == binder) return b->value;
	return NULL;
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
		.environment = environment, .pending = pending, .arguments = pending,
		.expected = result_type(f->module, term)};
	while (e->head->kind == PG_APPLICATION) {
		const struct pg_term *recursive = recursive_template(e->head);
		if (recursive) { e->head = recursive; e->recursive_template = 1; break; }
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
	if (e->inputs[slot] && !e->inputs[slot]->expected) e->inputs[slot]->expected = e->expected;
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

/* Follow known lexical closures to their native dependencies, then retain
	* stable lexical order. Source support membership never evaluates a body. */
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
	/* Each source binder enters this finite queue once. Delayed bodies retain
		* other known closures; recursive targets retain their native captures. */
	for (size_t i = 0; i < e->capture_count; ++i) {
		const struct expression *v = e->captures[i]->value;
		if (!v->delayed && !v->recursion) continue;
		for (const struct binding *b = e->environment; b; b = b->parent) {
			if (pg_dag_find(&seen, b->binder)) continue;
			int member = v->delayed ? pg_support_contains(v->suspended, b->binder) : 0;
			if (member < 0) goto done;
			if (v->recursion) for (size_t j = 0; !member && j < v->recursion->capture_count; ++j)
				if (v->recursion->captures[j] == b->binder) member = 1;
			if (!member) continue;
			if (pg_dag_add(&seen, b->binder)) goto done;
			e->captures[e->capture_count++] = b;
		}
	}
	const struct binding **ordered = pg_alloc(&e->function->module->order.storage,
		capacity * sizeof(*ordered));
	if (!ordered) goto done;
	size_t position = 0;
	for (const struct binding *b = e->environment; b; b = b->parent)
		for (size_t i = 0; i < e->capture_count; ++i)
			if (e->captures[i] == b) { ordered[position++] = b; break; }
	if (position != e->capture_count) goto done;
	e->captures = ordered;
	status = 0;
done:
	pg_dag_destroy(&seen);
	return status;
}

static struct function *callee(struct expression *e)
{
	struct module *m = e->function->module;
	for (size_t i = 0; i < e->count; ++i) if (!e->inputs[i]->value->type) return NULL;
	if (captures(e)) return NULL;
	uint64_t hash = mix((uintptr_t)e->head, e->count);
	for (size_t i = 0; i < e->count; ++i) hash = mix(hash, (uintptr_t)e->inputs[i]->value->type);
	size_t native_captures = 0;
	for (size_t i = 0; i < e->capture_count; ++i) {
		const struct expression *v = e->captures[i]->value;
		if (!v->type && !v->recursion && !v->delayed && !v->nominal) return NULL;
		if (v->type) ++native_captures;
		hash = mix(mix(hash, (uintptr_t)e->captures[i]->binder), (uintptr_t)(v->type ? (const void *)v->type : (const void *)v));
	}
	for (struct pg_index_entry *entry = pg_index_candidates(&m->functions, hash); entry; entry = entry->next) {
		struct function *f = (struct function *)entry;
		if (f->lambda != e->head || f->capture_count != e->capture_count || f->count != e->count + native_captures) continue;
		size_t i = 0;
		while (i < e->count && f->parameters[i]->type == e->inputs[i]->value->type) ++i;
		if (i != e->count) continue;
		i = 0;
		size_t p = e->count;
		while (i < e->capture_count && f->captures[i] == e->captures[i]->binder) {
			if (e->captures[i]->value->type && f->parameters[p++]->type != e->captures[i]->value->type) break;
			if (!e->captures[i]->value->type && f->static_captures[i] != e->captures[i]->value) break;
			++i;
		}
		if (i == e->capture_count) return f;
	}
	if (e->count > SIZE_MAX - native_captures) return NULL;
	struct function *f = function(m, e->count + native_captures);
	if (!f) return NULL;
	f->lambda = e->head; f->capture_count = e->capture_count;
	f->captures = pg_alloc(&m->order.storage, e->capture_count * sizeof(*f->captures));
	f->static_captures = pg_alloc(&m->order.storage, e->capture_count * sizeof(*f->static_captures));
	if (!f->captures || !f->static_captures) return NULL;
	const struct binding *environment = NULL;
	size_t p = e->count;
	for (size_t i = 0; i < e->capture_count; ++i) {
		struct expression *v = e->captures[i]->value;
		f->static_captures[i] = v->type ? NULL : v;
		if (v->type) { v = f->parameters[p++]; v->type = e->captures[i]->value->type; }
		f->captures[i] = e->captures[i]->binder;
		environment = bind(f, environment, f->captures[i], v);
		if (!environment) return NULL;
	}
	const struct pg_term *body = e->head;
	size_t consumed = 0;
	for (size_t i = 0; i < e->count; ++i) {
		f->parameters[i]->type = e->inputs[i]->value->type;
		if (body->kind != PG_LAMBDA) continue;
		environment = bind(f, environment, body->as.lambda.binder, f->parameters[i]);
		if (!environment) return NULL;
		body = body->as.lambda.body; ++consumed;
	}
	for (size_t i = 0; i < e->capture_count; ++i) if (e->captures[i]->value->delayed) {
		struct expression *thunk = pg_alloc(&m->order.storage, sizeof(*thunk));
		if (!thunk) return NULL;
		*thunk = *e->captures[i]->value; thunk->value = thunk; thunk->environment = environment;
		environment = bind(f, environment, e->captures[i]->binder, thunk);
		if (!environment) return NULL;
	}
	const struct argument *pending = NULL;
	for (size_t i = e->count; i > consumed; --i) {
		pending = argument(m, NULL, NULL, f->parameters[i - 1], pending);
		if (!pending) return NULL;
	}
	f->body = expression(f, body, environment, pending);
	if (f->body && !f->body->expected) f->body->expected = e->expected;
	if (!f->body || pg_index_insert(&m->functions, &f->index, hash)) return NULL;
	return f;
}

static struct function *recursive_function(struct expression *e)
{
	if (!e->count || !e->expected || captures(e)) return NULL;
	struct module *m = e->function->module;
	size_t native_captures = 0;
	for (size_t i = 0; i < e->capture_count; ++i) {
		const struct expression *v = e->captures[i]->value;
		if (v->type) ++native_captures;
		/* Thunk creation admits only known lambdas or direct IH calls;
			* recursive target identities stay private, never C parameters. */
		else if (!v->delayed && !v->recursion && !v->nominal) return NULL;
	}
	if (e->count > SIZE_MAX - native_captures) return NULL;
	struct function *f = function(m, e->count + native_captures);
	if (!f) return NULL;
	f->recursive = 1; f->result = e->expected; f->capture_count = e->capture_count; f->source_count = e->count;
	f->captures = pg_alloc(&m->order.storage, f->capture_count * sizeof(*f->captures));
	f->static_captures = pg_alloc(&m->order.storage, f->capture_count * sizeof(*f->static_captures));
	if (!f->captures || !f->static_captures) return NULL;
	const struct binding *environment = NULL;
	size_t p = e->count;
	for (size_t i = 0; i < f->capture_count; ++i) {
		const struct binding *b = e->captures[i];
		struct expression *v = b->value;
		f->captures[i] = b->binder; f->static_captures[i] = v->type ? NULL : v;
		if (v->type) { v = f->parameters[p++]; v->type = b->value->type; }
		environment = bind(f, environment, b->binder, v);
		if (!environment) return NULL;
	}
	struct expression *recursion = pg_alloc(&m->order.storage, sizeof(*recursion));
	if (!recursion) return NULL;
	*recursion = (struct expression){.recursion = f}; recursion->value = recursion;
	environment = bind(f, environment, e->head->as.lambda.binder, recursion);
	const struct pg_term *lambda = e->head->as.lambda.body;
	for (size_t i = 0; i < e->count; ++i) {
		f->parameters[i]->type = e->inputs[i]->value->type;
		if (!f->parameters[i]->type) return NULL;
	}
	if (!f->parameters[0]->type->recursive && !f->parameters[0]->type->natural) return NULL;
	environment = bind(f, environment, lambda->as.lambda.binder, f->parameters[0]);
	if (!environment) return NULL;
	for (size_t i = 0; i < f->capture_count; ++i) if (f->static_captures[i]) {
		struct expression *thunk = pg_alloc(&m->order.storage, sizeof(*thunk));
		if (!thunk) return NULL;
		*thunk = *f->static_captures[i]; thunk->value = thunk; thunk->environment = environment;
		environment = bind(f, environment, f->captures[i], thunk);
		if (!environment) return NULL;
	}
	const struct argument *pending = NULL;
	for (size_t i = e->count; i > 1; --i) {
		pending = argument(m, NULL, NULL, f->parameters[i - 1], pending);
		if (!pending) return NULL;
	}
	f->body = expression(f, lambda->as.lambda.body, environment, pending);
	if (f->body) f->body->expected = f->result;
	return f->body ? f : NULL;
}

/* Known IH thunks are lexical target closures. They are forced only at their
	* source use, never evaluated eagerly or passed through the public ABI. */
static int inline_call(struct expression *e, const void **out)
{
	const struct pg_term *body = e->head;
	const struct binding *environment = e->environment;
	size_t consumed = 0;
	while (consumed < e->count && body->kind == PG_LAMBDA) {
		environment = bind(e->function, environment, body->as.lambda.binder, e->inputs[consumed++]->value);
		if (!environment) return -1;
		body = body->as.lambda.body;
	}
	const struct argument *pending = NULL;
	for (size_t i = e->count; i > consumed; --i) {
		pending = argument(e->function->module, NULL, NULL, e->inputs[i - 1]->value, pending);
		if (!pending) return -1;
	}
	return child(e, e->count, body, environment, pending, out);
}

/* Keep branch computations inside private callees: visiting their graphs must
	* not turn conditional execution into eager evaluation of every branch. */
static int match(struct expression *e, const struct pg_c_representation *type,
	size_t slot, const void **out)
{
	if (e->count <= type->count) return -1;
	size_t extra = e->count - type->count - 1;
	if (!slot) return operand(e, 0, e->arguments, out);
	if (slot == 1) {
		if (e->inputs[0]->computation || e->inputs[0]->value->type != type) return -1;
		e->match = type;
		e->branches = pg_alloc(&e->function->module->order.storage, type->count * sizeof(*e->branches));
		if (!e->branches) return -1;
		e->cursor = e->arguments->next;
		for (size_t i = 0; i < type->count; ++i) e->cursor = e->cursor->next;
	}
	if (slot <= extra) {
		const struct argument *a = e->cursor;
		e->cursor = a->next;
		return operand(e, slot, a, out);
	}
	if (slot == extra + 1) {
		for (size_t i = 1; i <= extra; ++i) if (e->inputs[i]->computation) return -1;
		e->cursor = e->arguments->next;
	}
	size_t branch = slot - extra - 1;
	if (branch < type->count) {
		const struct argument *a = e->cursor;
		e->cursor = a->next;
		if (!a->term) return -1;
		struct expression *call = pg_alloc(&e->function->module->order.storage, sizeof(*call));
		if (!call) return -1;
		const struct pg_c_constructor_representation *c = type->constructors ? &type->constructors[branch] : NULL;
		size_t fields = c ? c->count : 0;
		if (fields > SIZE_MAX - extra || fields + extra > SIZE_MAX / sizeof(*call->inputs)) return -1;
		*call = (struct expression){.function = e->function, .head = callable(a->term),
			.environment = a->environment, .count = fields + extra, .expected = e->expected};
		call->inputs = pg_alloc(&e->function->module->order.storage, call->count * sizeof(*call->inputs));
		if (!call->inputs) return -1;
		for (size_t i = 0; i < fields; ++i) {
			struct expression *field = pg_alloc(&e->function->module->order.storage, sizeof(*field));
			if (!field) return -1;
			*field = (struct expression){.type = c->fields[i], .container = e->inputs[0], .constructor = branch, .field = i};
			field->value = field; call->inputs[i] = field;
		}
		for (size_t i = 0; i < extra; ++i) call->inputs[fields + i] = e->inputs[i + 1];
		e->branches[branch] = call;
		call->callee = callee(call);
		if (!call->callee) return -1;
		*out = call->callee->body;
		return 1;
	}
	struct expression *first = e->branches[0]->callee->body;
	for (size_t i = 1; i < type->count; ++i) {
		struct expression *body = e->branches[i]->callee->body;
		if (body->computation != first->computation || body->value->type != first->value->type) return -1;
	}
	e->type = first->value->type; e->computation = first->computation; e->value = e;
	return 0;
}

static int constructor(struct expression *e, const struct pg_c_representation *type,
	size_t position, size_t arity, size_t slot, const void **out)
{
	if (!type || position >= type->count || arity != e->count) return -1;
	const struct pg_c_constructor_representation *c = type->constructors ? &type->constructors[position] : NULL;
	if (c ? c->count != arity : arity != 0) return -1;
	if (slot < arity) {
		const struct argument *a = e->cursor; e->cursor = a->next;
		return operand(e, slot, a, out);
	}
	for (size_t i = 0; i < arity; ++i)
		if (e->inputs[i]->computation || e->inputs[i]->value->type != c->fields[i]) return -1;
	e->type = type; e->bits = position; e->construction = c; e->value = e;
	return 0;
}

/* Dependencies include private callee bodies. Direct calls to an already
	* recognized recursive target use its represented result without a DAG cycle. */
static int lower(struct expression *e, size_t slot, const void **out)
{
	const struct pg_c_representations *representations = &e->function->module->representations;
	if (e->head->kind == PG_REFERENCE) {
		const struct pg_data_layout *layout;
		size_t position, arity;
		if (pg_data_constructor_view(e->head->as.reference, &layout, &position, &arity))
			return constructor(e, pg_c_representation_find(representations, pg_data_matcher(layout)), position, arity, slot, out);
	}
	if (e->head->kind == PG_REFERENCE && !e->count) {
		const struct pg_object *object = e->head->as.reference, *type;
		if (object->kind == PG_BINDER) {
			return forward(e, lookup(e->environment, object), 0);
		}
		/* A selected type constant stays a private identity token. Bind it
			* through known lambdas; no runtime or public Universe ABI exists. */
		if (pg_c_representation_find(representations, object) &&
			(pg_data_declaration_view(object) || pg_host_type_name(object))) {
			e->nominal = object; e->value = e;
			return 0;
		}
		size_t count;
		const unsigned char *bytes;
		if (!pg_host_literal_view(object, &type, &count, &bytes)) return -1;
		e->type = pg_c_representation_find(representations, type);
		if (!e->type || e->type->layout || count != e->type->width / 8) return -1;
		for (size_t i = 0; i < count; ++i) e->bits = (e->bits << 8) | bytes[i];
		e->value = e;
		return 0;
	}
	if (!e->count) return -1;
	const struct pg_object *operation = e->head->kind == PG_REFERENCE ? e->head->as.reference : NULL;
	const struct argument *a = e->arguments;
	struct expression *known = operation && operation->kind == PG_BINDER ? lookup(e->environment, operation) : NULL;
	if (known && known->recursion) {
		struct function *f = known->recursion;
		if (e->count != f->source_count) return -1;
		if (slot < e->count) { a = e->cursor; e->cursor = a->next; return operand(e, slot, a, out); }
		for (size_t i = 0; i < e->count; ++i)
			if (e->inputs[i]->computation || e->inputs[i]->value->type != f->parameters[i]->type) return -1;
		e->callee = f; e->type = f->result; e->computation = 1; e->value = e;
		e->capture_count = f->capture_count;
		e->captures = pg_alloc(&e->function->module->order.storage, f->capture_count * sizeof(*e->captures));
		if (!e->captures) return -1;
		size_t p = f->source_count;
		for (size_t i = 0; i < f->capture_count; ++i) {
			const struct binding *b = e->environment;
			while (b && b->binder != f->captures[i]) b = b->parent;
			if (!b) return -1;
			if (f->static_captures[i]) {
				const struct expression *capture = f->static_captures[i];
				if (capture->nominal) {
					if (b->value->nominal != capture->nominal) return -1;
				} else if (capture->recursion) {
					if (b->value->recursion != capture->recursion) return -1;
				} else if (!b->value->delayed || b->value->suspended != capture->suspended) return -1;
			} else if (b->value->type != f->parameters[p++]->type) return -1;
			e->captures[i] = b;
		}
		return 0;
	}
	if (operation == &pg_thunk_operation) {
		/* Retain known function values and direct single-tail IH thunks. */
		const struct pg_term *body = e->count == 1 ? a->term : NULL;
		if (!body) return -1;
		if (!known_lambda(body)) {
			if (body->kind != PG_APPLICATION || body->as.application.function->kind != PG_REFERENCE) return -1;
			struct expression *r = lookup(a->environment, body->as.application.function->as.reference);
			if (!r || !r->recursion) return -1;
		}
		e->delayed = 1; e->suspended = body; e->value = e;
		return 0;
	}
	if (operation && pg_data_layout_view(operation)) {
		const struct pg_c_representation *type = pg_c_representation_find(representations, operation);
		return type ? match(e, type, slot, out) : -1;
	}
	if (operation == &pg_return_operation || operation == &pg_total_result_operation) {
		if (e->count != 1) return -1;
		if (!slot) return operand(e, 0, a, out);
		int returned = operation == &pg_return_operation;
		if (e->inputs[0]->computation == returned) return -1;
		return forward(e, e->inputs[0], returned);
	}
	if (operation == &pg_force_operation) {
		const struct pg_term *body = a->term ? unary(a->term, &pg_thunk_operation) : NULL;
		struct expression *thunk = a->value;
		if (!thunk && a->term && a->term->kind == PG_REFERENCE)
			thunk = lookup(a->environment, a->term->as.reference);
		if (thunk && thunk->value && thunk->value->type && thunk->value->type->callback) {
			const struct pg_c_representation *type = thunk->value->type;
			size_t arity = (size_t)type->callback;
			if (e->count != arity + 1) return -1;
			if (slot < arity) {
				const struct argument *argument = a->next;
				for (size_t i = 0; i < slot; ++i) argument = argument->next;
				return operand(e, slot, argument, out);
			}
			const struct pg_c_representation *input = type->callback_domain ? type->callback_domain :
				pg_c_representation_find(representations, pg_host_type(type->width == 32 ? "Int32" : "Int64"));
			e->type = type->callback_result ? type->callback_result : input;
			for (size_t i = 0; i < arity; ++i)
				if (e->inputs[i]->computation || e->inputs[i]->value->type != input) return -1;
			e->foreign = thunk->value; e->value = e; e->computation = 1;
			return 0;
		}
		if (thunk && thunk->delayed) {
			body = thunk->suspended;
			if (!slot) return child(e, 0, body, thunk->environment, a->next, out);
		} else {
			if (!body) return -1;
			if (!slot) return child(e, 0, body, a->environment, a->next, out);
		}
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
		e->type = pg_c_representation_find(representations, result);
		for (size_t i = 0; i < arity; ++i)
			if (e->inputs[i]->computation || e->inputs[i]->value->type != pg_c_representation_find(representations, domain)) return -1;
		e->value = e; e->computation = 1;
		return e->type ? 0 : -1;
	}
	if (e->head->kind != PG_LAMBDA) return -1;
	if (slot < e->count) {
		a = e->cursor; e->cursor = a->next;
		return operand(e, slot, a, out);
	}
	if (slot == e->count) {
		for (size_t i = 0; i < e->count; ++i) if (e->inputs[i]->computation) return -1;
		for (size_t i = 0; i < e->count; ++i)
			if (e->inputs[i]->value->delayed || e->inputs[i]->value->nominal) return inline_call(e, out);
		e->callee = e->recursive_template ? recursive_function(e) : callee(e);
		if (!e->callee) return -1;
		*out = e->callee->body;
		return 1;
	}
	if (!e->callee) return forward(e, e->inputs[e->count], e->inputs[e->count]->computation);
	if (!e->callee->body->computation) return -1;
	if (e->callee->recursive && e->callee->body->value->type != e->callee->result) return -1;
	e->type = e->callee->body->value->type;
	e->value = e; e->computation = 1;
	return 0;
}

static int lower_child(void *owner, const void *key, size_t slot, const void **out)
{
	struct module *m = owner;
	struct expression *e = (struct expression *)key;
	m->error = "unsupported native expression, argument or capture representation";
	int status = lower(e, slot, out);
	if (status == 0 && e->value == e && !e->delayed && !e->nominal) {
		struct function *f = e->function;
		if (f->definitions == SIZE_MAX) return -1;
		e->id = ++f->definitions;
		if (f->last) f->last->next = e;
		else f->first = e;
		f->last = e;
	}
	return status;
}

static const struct pg_c_representation *parameter_type(struct module *m, const struct pg_term *type)
{
	const struct pg_c_representation *represented = pg_c_representation_term(&m->representations, type);
	if (represented || !m->callbacks) return represented;
	const struct pg_term *computation, *domain, *codomain, *result;
	const struct pg_object *binder;
	enum pg_totality totality;
	if (!pg_thunk_type_view(type, &computation)) return NULL;
	const struct pg_c_representation *input = NULL;
	size_t arity = 0;
	while (pg_pi_view(computation, &domain, &binder, &codomain)) {
		if (arity >= (size_t)m->callbacks || !pg_pi_constant_codomain(computation)) return NULL;
		const struct pg_c_representation *next = pg_c_representation_term(&m->representations, domain);
		if (!next || next->callback || (input && next != input)) return NULL;
		if (m->native_predicates ? !next->natural : next->layout || next->constructors) return NULL;
		input = next; ++arity; computation = codomain;
	}
	if (!arity || !pg_pure_computation_type_view(computation, &totality, &result) ||
		totality != PG_TOTALITY_TOTAL) return NULL;
	const struct pg_c_representation *output = pg_c_representation_term(&m->representations, result);
	if (m->native_predicates) {
		if (!output || !output->layout || output->constructors || output->natural || output->count != 2) return NULL;
		for (struct predicate *p = m->predicates; p; p = p->next)
			if (p->type.callback == (int)arity && p->type.callback_domain == input && p->type.callback_result == output) return &p->type;
		struct predicate *p = pg_alloc(&m->order.storage, sizeof(*p));
		if (!p) return NULL;
		*p = (struct predicate){.next = m->predicates, .type = {.width = 32, .callback = (int)arity,
			.callback_domain = input, .callback_result = output}};
		m->predicates = p;
		return &p->type;
	}
	if (input != output) return NULL;
	static const struct pg_c_representation types[2][2] = {
		{{.width = 32, .callback = 1}, {.width = 64, .callback = 1}},
		{{.width = 32, .callback = 2}, {.width = 64, .callback = 2}}
	};
	return input->width == 32 ? &types[arity - 1][0] : input->width == 64 ? &types[arity - 1][1] : NULL;
}

static struct function *prepare(struct module *m, const struct pg_occurrence *subject)
{
	m->error = m->native_predicates ? "expected pure total native functions with borrowed unary/binary Nat32 predicates" :
		m->callbacks == 2 ? "expected pure total scalar functions with borrowed unary/binary scalar callbacks" :
		m->callbacks ? "expected pure total scalar functions with borrowed unary scalar callbacks" :
		"expected closed represented values or pure total first-order functions";
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
		if (!computation || !pg_pi_view(type, &domain, &binder, &codomain)) return NULL;
		if (!(f->parameters[i]->type = parameter_type(m, domain))) return NULL;
		type = codomain;
	}
	if (computation) {
		enum pg_totality totality;
		if (!pg_pure_computation_type_view(type, &totality, &content) || totality != PG_TOTALITY_TOTAL) return NULL;
		type = content;
	}
	const struct pg_c_representation *result = pg_c_representation_term(&m->representations, type);
	if (!result) return NULL;
	const struct argument *pending = NULL;
	for (size_t i = count; i; --i) {
		pending = argument(m, NULL, NULL, f->parameters[i - 1], pending);
		if (!pending) return NULL;
	}
	f->body = expression(f, term, NULL, pending);
	if (f->body) f->body->expected = result;
	if (!f->body || pg_dag_add(&m->order, f->body)) return NULL;
	return f->body->computation == computation && f->body->value->type == result ? f : NULL;
}

static void parameters(FILE *out, const struct function *f)
{
	if (f->module->representations.arena_alias) fputs("struct ap_c_arena *arena, ", out);
	for (size_t i = 0; i < f->count; ++i) {
		pg_c_representation_type(out, f->parameters[i]->type);
		fprintf(out, " a%zu, ", i + 1);
	}
	pg_c_representation_type(out, f->body->value->type);
	fputs(" *out", out);
}

static void value(FILE *out, const struct expression *e)
{
	e = e->value;
	if (e->container) {
		if (e->container->value->type->natural) {
			fputc('(', out); value(out, e->container); fputs(" - UINT32_C(1))", out); return;
		}
		int bits = !e->type->constructors || e->type->natural;
		if (bits) fprintf(out, "(uint%zu_t)(", e->type->width);
		value(out, e->container);
		fprintf(out, "%sfields.c%zu.f%zu%s", e->container->value->type->recursive ? "->" : ".", e->constructor, e->field,
			e->type->layout && !e->type->constructors ? ".tag" : "");
		if (bits) fputc(')', out);
	} else if (e->parameter) fprintf(out, "a%zu", e->parameter);
	else fprintf(out, "v%zu", e->id);
}

/* Convert unsigned arithmetic bits without implementation-defined signed
	* overflow/conversion. Tagged fields use their explicitly selected type. */
static void public_value(FILE *out, const struct pg_c_representation *type, const struct expression *e)
{
	if (type->constructors || type->callback) { value(out, e); return; }
	if (type->layout) {
		fputc('(', out); pg_c_representation_type(out, type); fputs("){(uint32_t)(", out);
		value(out, e); fputs(")}", out); return;
	}
	fputc('(', out); value(out, e); fprintf(out, " <= INT%zu_MAX ? (int%zu_t)", type->width, type->width);
	value(out, e); fprintf(out, " : -1 - (int%zu_t)(UINT%zu_MAX - ", type->width, type->width);
	value(out, e); fputs("))", out);
}

static void signature(FILE *out, const struct function *f)
{
	fputs("static ", out); pg_c_representation_private_type(out, f->body->value->type); fprintf(out, " c%zu(", f->id);
	int arena = f->module->representations.arena_alias != NULL;
	if (arena) fputs("struct ap_c_arena *arena", out);
	if (!f->count && !arena) fputs("void", out);
	for (size_t i = 0; i < f->count; ++i) {
		if (i || arena) fputs(", ", out);
		pg_c_representation_private_type(out, f->parameters[i]->type); fprintf(out, " a%zu", i + 1);
	}
	fputc(')', out);
}

static void emit_call(FILE *out, const struct expression *e)
{
	fprintf(out, "c%zu(", e->callee->id);
	int comma = e->function->module->representations.arena_alias != NULL;
	if (comma) fputs("arena", out);
	for (size_t i = 0; i < e->count; ++i) { if (comma) fputs(", ", out); value(out, e->inputs[i]); comma = 1; }
	for (size_t i = 0; i < e->capture_count; ++i) {
		if (!e->captures[i]->value->type) continue;
		if (comma) fputs(", ", out);
		value(out, e->captures[i]->value);
		comma = 1;
	}
	fputc(')', out);
}

static void failure_return(FILE *out, const struct function *f)
{
	if (f->recursive) fputs("--arena->depth; ", out);
	fputs("return ", out);
	if (f->body->value->type->constructors && !f->body->value->type->recursive && !f->body->value->type->natural) {
		fputc('(', out); pg_c_representation_private_type(out, f->body->value->type); fputs("){0}", out);
	} else fputs("0", out);
	fputs(";", out);
}

static void allocation_guard(FILE *out, const struct function *f)
{
	if (!f->module->representations.arena_alias) return;
	fputs("\tif (arena->status) { ", out); failure_return(out, f); fputs(" }\n", out);
}

static void value_validators(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i];
		if (!r->constructors || r->natural || r->recursive) continue;
		fprintf(out, "static int ap_valid_value_%s(struct ap_data_%s input)\n{\n\tswitch (input.tag) {\n", r->alias, r->alias);
		for (size_t j = 0; j < r->count; ++j) {
			fprintf(out, "\tcase %zu:\n", j);
			const struct pg_c_constructor_representation *c = &r->constructors[j];
			for (size_t k = 0; k < c->count; ++k) {
				const struct pg_c_representation *f = c->fields[k];
				if (!f->layout || f->natural) continue;
				if (f->constructors) fprintf(out, "\t\tif (!ap_valid_value_%s(input.fields.c%zu.f%zu)) return 0;\n", f->alias, j, k);
				else fprintf(out, "\t\tif ((uint64_t)input.fields.c%zu.f%zu.tag >= UINT64_C(%zu)) return 0;\n", j, k, f->count);
			}
			fputs("\t\treturn 1;\n", out);
		}
		fputs("\tdefault: return 0;\n\t}\n}\n\n", out);
	}
}

static void emit_function(FILE *out, const struct function *f)
{
	signature(out, f); fputs("\n{\n", out);
	if (f->module->representations.arena_alias) fputs("\t(void)arena;\n", out);
	if (f->recursive) {
		fputs("\tif (arena->depth >= (arena->depth_limit ? arena->depth_limit : 256)) { arena->status = 4; ", out);
		/* No depth was entered on this path. */
		if (f->body->value->type->constructors && !f->body->value->type->recursive && !f->body->value->type->natural) {
			fputs("return (", out); pg_c_representation_private_type(out, f->body->value->type); fputs("){0};", out);
		} else fputs("return 0;", out);
		fputs(" }\n\t++arena->depth;\n", out);
	}
	for (size_t i = 0; i < f->count; ++i) fprintf(out, "\t(void)a%zu;\n", i + 1);
	for (const struct expression *e = f->first; e; e = e->next) {
		size_t id = e->id;
		if (e->match) {
			fputc('\t', out); pg_c_representation_private_type(out, e->type);
			fprintf(out, " v%zu;\n\tswitch (", id); value(out, e->inputs[0]);
			if (e->match->natural) fprintf(out, " == 0 ? %zu : %zu) {\n", e->match->zero, 1 - e->match->zero);
			else fputs(e->match->recursive ? "->tag) {\n" : e->match->constructors ? ".tag) {\n" : ") {\n", out);
			for (size_t i = 0; i < e->match->count; ++i) {
				fprintf(out, "\tcase %zu: v%zu = ", i, id); emit_call(out, e->branches[i]); fputs("; break;\n", out);
			}
			fprintf(out, "\tdefault: abort();\n\t}\n\t(void)v%zu;\n", id);
			allocation_guard(out, f);
			continue;
		}
		if (e->type->constructors && !e->type->natural) {
			fputc('\t', out); pg_c_representation_private_type(out, e->type); fprintf(out, " v%zu = ", id);
			if (e->callee) emit_call(out, e);
			else {
				if (e->type->recursive) fprintf(out, "ap_allocate(arena, sizeof(struct ap_data_%s));\n\tif (!v%zu) { ", e->type->alias, id);
				if (e->type->recursive) {
					failure_return(out, f); fprintf(out, " }\n\t*(struct ap_data_%s *)v%zu = (struct ap_data_%s)", e->type->alias, id, e->type->alias);
				}
				fprintf(out, "{.tag = UINT32_C(%" PRIu64 ")", e->bits);
				for (size_t i = 0; i < e->construction->count; ++i) {
					fprintf(out, ", .fields.c%" PRIu64 ".f%zu = ", e->bits, i);
					public_value(out, e->construction->fields[i], e->inputs[i]);
				}
				fputc('}', out);
			}
			fprintf(out, ";\n\t(void)v%zu;\n", id); allocation_guard(out, f); continue;
		}
		fprintf(out, "\tuint64_t v%zu = (uint%zu_t)(", id, e->type->width);
		if (e->foreign) {
			const struct pg_c_representation *type = e->foreign->type;
			value(out, e->foreign); fputs(".call(", out);
			value(out, e->foreign); fputs(".context", out);
			for (size_t i = 0; i < (size_t)type->callback; ++i) {
				fputs(", ", out); public_value(out, type->callback_domain ? type->callback_domain : e->type, e->inputs[i]);
			}
			fputc(')', out);
			if (type->callback_result) fputs(".tag", out);
		} else if (e->callee) emit_call(out, e);
		else if (e->type->natural) {
			if (e->construction->count) { value(out, e->inputs[0]); fputs(" + UINT64_C(1)", out); }
			else fputc('0', out);
		}
		else if (!e->arithmetic) fprintf(out, "UINT64_C(0x%" PRIx64 ")", e->bits);
		else if (e->arithmetic == '~') { fputs("UINT64_C(0) - ", out); value(out, e->inputs[0]); }
		else { value(out, e->inputs[0]); fprintf(out, " %c ", e->arithmetic); value(out, e->inputs[1]); }
		fprintf(out, ");\n\t(void)v%zu;\n", id);
		if (e->foreign && e->foreign->type->callback_result) {
			fprintf(out, "\tif (v%zu >= UINT64_C(%zu)) { arena->status = 2; ", id, e->type->count);
			failure_return(out, f); fputs(" }\n", out);
		}
		if (e->type->natural && !e->callee && e->construction->count) {
			fputs("\tif (", out); value(out, e->inputs[0]); fputs(" == UINT32_MAX) { arena->status = 5; ", out);
			failure_return(out, f); fputs(" }\n", out);
		}
		allocation_guard(out, f);
	}
	if (f->recursive) fputs("\t--arena->depth;\n", out);
	fputs("\treturn ", out); value(out, f->body); fputs(";\n}\n\n", out);
}

static void predicate_declarations(FILE *out, const struct module *m)
{
	for (const struct predicate *p = m->predicates; p; p = p->next) {
		const struct pg_c_representation *type = &p->type;
		/* Lengths keep distinct alias pairs unambiguous even with underscores. */
		fprintf(out, "#ifndef AP_C_PREDICATE%d_D%zu_%s_R%zu_%s_TYPES\n#define AP_C_PREDICATE%d_D%zu_%s_R%zu_%s_TYPES 1\n",
			type->callback, strlen(type->callback_domain->alias), type->callback_domain->alias,
			strlen(type->callback_result->alias), type->callback_result->alias,
			type->callback, strlen(type->callback_domain->alias), type->callback_domain->alias,
			strlen(type->callback_result->alias), type->callback_result->alias);
		fputs("/* Borrowed code/context implements the admitted pure-total predicate;\n"
			"\t* it must outlive the synchronous call. Invalid result tags return 2. */\n", out);
		pg_c_representation_type(out, type); fputs(" { void *context; ", out);
		pg_c_representation_type(out, type->callback_result); fputs(" (*call)(void *", out);
		for (int i = 0; i < type->callback; ++i) { fputs(", ", out); pg_c_representation_type(out, type->callback_domain); }
		fputs("); };\n#endif\n", out);
	}
}

static int emit(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t natural_count,
	const struct pg_c_export *naturals, size_t data_count,
	const struct pg_c_export *data, int native, int callbacks, struct pg_c_native_contract *contract, const char **error)
{
	if (!source || !header || !error) return -1;
	*error = "invalid scalar exports or entry";
	if (!pg_c_export_names(count, exports) || (entry != SIZE_MAX && entry >= count)) return -1;
	struct module m = {.callbacks = callbacks, .native_predicates = native && callbacks};
	struct pg_dag roots;
	if (pg_dag_init(&roots, NULL, NULL)) return -1;
	struct function **functions = NULL;
	int status = -1;
	if (pg_dag_init(&m.order, lower_child, &m) || pg_index_init(&m.functions) || pg_index_init(&m.arguments) || pg_index_init(&m.results)) goto done;
	*error = "representations require unique closed declarations; fields need scalar, enum, prior selected value data or direct Self contracts";
	if (pg_c_representations_native(&m.representations, &m.order.storage, enum_count, enums, natural_count, naturals, data_count, data)) goto done;
	*error = "cannot inspect existing induction result classifiers";
	if (induction_results(&m, count, exports)) goto done;
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
	fputs("/* A Program native realization of admitted pure total computations. */\n#include <stdint.h>\n#include <limits.h>\n"
		"_Static_assert(sizeof(int) <= sizeof(uint32_t), \"unsupported integer promotion model\");\n\n", source);
	if (enum_count || natural_count || data_count) fputs("#include <stdlib.h>\n", source);
	const char *abi = m.native_predicates ? "PREDICATE_NATIVE" : callbacks == 2 ? "CALLBACK2" : callbacks ? "CALLBACK" : native ? "NATIVE" : "SCALAR";
	const char *name = m.native_predicates ? "predicate_native" : callbacks == 2 ? "callback2" : callbacks ? "callback" : native ? "native" : "scalar";
	fputs(m.native_predicates ? "#pragma once\n/* ABI 1: 0 success; 1 null output/arena; 2 invalid input/code/result tag. */\n#include <stdint.h>\n" :
		callbacks ? "#pragma once\n/* ABI 1: 0 success; 1 null output; 2 null callback code. Output must be writable. */\n#include <stdint.h>\n" :
		"#pragma once\n/* ABI 1: 0 success; 1 null output; 2 invalid tag input. Output must be writable. */\n#include <stdint.h>\n", header);
	if (m.representations.arena_alias)
		fputs("/* Recursive profile: 1 also null arena; 2 also null tail/cycle; 3 allocation;\n"
			"\t* 4 depth limit. Failures preserve output and roll back new allocations. */\n", header);
	if (natural_count) fputs("/* nat32: uint32 magnitude; status 5 on successor overflow. */\n", header);
	fprintf(header, "#ifndef AP_C_%s_ABI\n#define AP_C_%s_ABI 1\n#elif AP_C_%s_ABI != 1\n#error incompatible_A_Program_%s_ABI\n#endif\n", abi, abi, abi, name);
	if (callbacks && !native) {
		const char *declarations = "#ifndef AP_C_CALLBACK_TYPES\n#define AP_C_CALLBACK_TYPES 1\n"
			"/* Borrowed code/context must outlive the call and implement the declared\n"
			"\t* pure-total function. Context may be null. Null code returns status 2. */\n"
			"struct ap_c_callback_i32 { void *context; int32_t (*call)(void *, int32_t); };\n"
			"struct ap_c_callback_i64 { void *context; int64_t (*call)(void *, int64_t); };\n#endif\n";
		fputs(declarations, source); fputs(declarations, header);
		if (callbacks == 2) {
			const char *binary = "#ifndef AP_C_CALLBACK2_TYPES\n#define AP_C_CALLBACK2_TYPES 1\n"
				"/* Binary callbacks use the same borrowed pure-total lifetime contract. */\n"
				"struct ap_c_callback2_i32 { void *context; int32_t (*call)(void *, int32_t, int32_t); };\n"
				"struct ap_c_callback2_i64 { void *context; int64_t (*call)(void *, int64_t, int64_t); };\n#endif\n";
			fputs(binary, source); fputs(binary, header);
		}
	}
	pg_c_representation_declarations(source, &m.representations);
	pg_c_representation_declarations(header, &m.representations);
	predicate_declarations(source, &m); predicate_declarations(header, &m);
	pg_c_nodes_declarations(source, &m.representations);
	fputs("#ifdef __cplusplus\nextern \"C\" {\n#endif\n", header);
	pg_c_nodes_declarations(header, &m.representations);
	value_validators(source, &m.representations);
	pg_c_nodes_implementation(source, &m.representations);
	for (const struct function *f = m.first; f; f = f->next) { signature(source, f); fputs(";\n", source); }
	fputc('\n', source);
	for (const struct function *f = m.first; f; f = f->next) emit_function(source, f);
	for (size_t i = 0; i < count; ++i) {
		const struct function *f = functions[pg_dag_find(&roots, exports[i].subject)->id - 1];
		fprintf(header, "int ap_export_%s(", exports[i].alias); parameters(header, f); fputs(");\n", header);
		fprintf(source, "int ap_export_%s(", exports[i].alias); parameters(source, f);
		fputs(")\n{\n\tif (!out) return 1;\n", source);
		for (size_t j = 0; j < m.representations.count; ++j) {
			const struct pg_c_representation *r = m.representations.types[j];
			if (r->constructors && !r->natural && !r->recursive)
				fprintf(source, "\t(void)ap_valid_value_%s;\n", r->alias);
		}
		if (m.representations.arena_alias) {
			fputs("\tif (!arena) return 1;\n\tif (arena->depth || arena->depth_limit > 256) return 4;\n\t(void)ap_allocate;\n", source);
			for (size_t j = 0; j < m.representations.count; ++j) if (m.representations.types[j]->recursive)
				fprintf(source, "\t(void)ap_validate_%s;\n", m.representations.types[j]->alias);
		}
		for (size_t j = 0; j < f->count; ++j) {
			const struct pg_c_representation *r = f->parameters[j]->type;
			if (r->callback) fprintf(source, "\tif (!a%zu.call) return 2;\n", j + 1);
			else if (r->recursive) fprintf(source, "\tif (!ap_validate_%s(a%zu)) return 2;\n", r->alias, j + 1);
			else if (r->constructors && !r->natural) {
				fprintf(source, "\tif (!ap_valid_value_%s(a%zu)) return 2;\n", r->alias, j + 1);
			} else if (r->layout && !r->natural) fprintf(source, "\tif ((uint64_t)a%zu.tag >= UINT64_C(%zu)) return 2;\n", j + 1, r->count);
		}
		if (m.representations.arena_alias)
			fputs("\tstruct ap_c_allocation *mark = arena->first;\n\tarena->status = 0;\n", source);
		fputc('\t', source); pg_c_representation_private_type(source, f->body->value->type); fprintf(source, " result = c%zu(", f->id);
		if (m.representations.arena_alias) fputs("arena", source);
		for (size_t j = 0; j < f->count; ++j) {
			const struct pg_c_representation *r = f->parameters[j]->type;
			if (j || m.representations.arena_alias) fputs(", ", source);
			if (!r->constructors && !r->callback) fprintf(source, "(uint%zu_t)", r->width);
			fprintf(source, "a%zu%s", j + 1, r->layout && !r->constructors ? ".tag" : "");
		}
		fputs(");\n", source);
		if (m.representations.arena_alias)
			fputs("\tif (arena->status) { ap_release(arena, mark); return arena->status; }\n", source);
		const struct pg_c_representation *r = f->body->value->type;
		if (r->constructors) fputs("\t*out = result;\n", source);
		else if (r->layout) fputs("\tout->tag = (uint32_t)result;\n", source);
		else fprintf(source, "\t*out = result <= INT%zu_MAX ? (int%zu_t)result : -1 - (int%zu_t)(UINT%zu_MAX - result);\n", r->width, r->width, r->width, r->width);
		fputs("\treturn 0;\n}\n\n", source);
	}
	fputs("#ifdef __cplusplus\n}\n#endif\n", header);
	if (entry != SIZE_MAX) {
		const struct function *f = functions[pg_dag_find(&roots, exports[entry].subject)->id - 1];
		fputs("int main(void)\n{\n\t", source); pg_c_representation_type(source, f->body->value->type);
		fprintf(source, " result;\n");
		if (m.representations.arena_alias) {
			fprintf(source, "\tstruct ap_c_arena arena = {0};\n\tint status = ap_export_%s(&arena, &result);\n"
				"\tap_arena_%s_destroy(&arena);\n\treturn status;\n}\n", exports[entry].alias, m.representations.arena_alias);
		} else fprintf(source, "\treturn ap_export_%s(&result);\n}\n", exports[entry].alias);
	}
	*error = "cannot write native C output";
	status = ferror(source) || ferror(header) ? -1 : 0;
	if (!status && contract) {
		*contract = (struct pg_c_native_contract){.natural = natural_count != 0};
		for (size_t i = 0; i < m.representations.count; ++i) {
			const struct pg_c_representation *r = m.representations.types[i];
			if (r->recursive) contract->recursive = 1;
			if (r->constructors) for (size_t j = 0; j < r->count; ++j)
				for (size_t k = 0; k < r->constructors[j].count; ++k) {
					const struct pg_c_representation *f = r->constructors[j].fields[k];
					if (f != r && f->constructors && !f->natural) contract->value_fields = 1;
				}
			size_t cell, payload;
			if (pg_c_representation_list(m.representations.types[i], &cell, &payload)) contract->copy_out = 1;
		}
	}
done:
	for (struct function *f = m.first; f; f = f->next) pg_index_destroy(&f->expressions);
	free(functions);
	pg_index_destroy(&m.functions); pg_index_destroy(&m.arguments); pg_index_destroy(&m.results);
	pg_c_representations_destroy(&m.representations);
	pg_dag_destroy(&m.order); pg_dag_destroy(&roots);
	return status;
}

int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error)
{
	return emit(source, header, count, exports, entry, 0, NULL, 0, NULL, 0, NULL, 0, 0, NULL, error);
}

int pg_c_emit_callbacks(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error)
{
	return emit(source, header, count, exports, entry, 0, NULL, 0, NULL, 0, NULL, 0, 1, NULL, error);
}

int pg_c_emit_callbacks2(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error)
{
	return emit(source, header, count, exports, entry, 0, NULL, 0, NULL, 0, NULL, 0, 2, NULL, error);
}

int pg_c_emit_predicate_native(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t natural_count,
	const struct pg_c_export *naturals, size_t data_count,
	const struct pg_c_export *data, struct pg_c_native_contract *contract, const char **error)
{
	return emit(source, header, count, exports, entry, enum_count, enums, natural_count, naturals,
		data_count, data, 1, 2, contract, error);
}

int pg_c_emit_native(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t data_count,
	const struct pg_c_export *data, const char **error)
{
	return emit(source, header, count, exports, entry, enum_count, enums, 0, NULL, data_count, data, 1, 0, NULL, error);
}

int pg_c_emit_native_profile(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t natural_count,
	const struct pg_c_export *naturals, size_t data_count,
	const struct pg_c_export *data, struct pg_c_native_contract *contract, const char **error)
{
	return emit(source, header, count, exports, entry, enum_count, enums, natural_count, naturals,
		data_count, data, 1, 0, contract, error);
}
