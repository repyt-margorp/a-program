#include "function_graph.h"
#include "computation.h"

#include <stdlib.h>

/* Published calls form an immutable prefix. Schema placement belongs to
 * a case's layout, not to the shared source call. */
struct graph_call {
	size_t field, hypothesis;
	size_t field_arity;
	const struct pg_evidence **field_arguments;
	const struct pg_evidence *child;
	const struct pg_evidence **arguments;
	const struct pg_evidence *result_context;
	const struct pg_evidence *helper_source, *helper_environment;
	const struct pg_evidence **helper_arguments;
	size_t helper_arity;
	const struct pg_function_graph_work *helper;
	const struct graph_call *previous;
};

struct graph_call_layout {
	const struct graph_call *call;
	size_t slot;
	const struct pg_evidence *layout_output;
	struct graph_call_layout *field_next;
};

struct graph_case {
	size_t field_count, hypothesis_count, call_count;
	size_t first_call, child_count, leaf;
	const struct pg_evidence **scopes;
	size_t *hypothesis_fields;
	const struct pg_evidence *context, *computation, *output;
	struct pg_whnf_job *normalization;
	const struct graph_call *calls;
	struct graph_call_layout *executed, **ordered;
	const struct pg_function_graph_order *source_order;
	struct graph_continuation *continuations;
	struct graph_case *parent, *children, *pending, *leaf_next, *build_next;
	const struct pg_evidence *origin_map, *refinement, *discriminant, *split_formation;
	const struct pg_evidence *schema_refinement, *schema_base, *schema_map, *schema_input, *base_input, *result;
	const struct pg_object *constructor;
};

struct graph_continuation {
	const struct pg_evidence *function;
	const struct pg_evidence *argument;
	struct graph_continuation *next;
};

/* Suspended helper inspection, not another source/result cache. */
struct helper_cursor {
	struct pg_graph temporary;
	const struct pg_evidence *function, *environment;
	union { const struct pg_evidence *body; struct pg_function_source_cursor source; };
	struct graph_continuation *arguments, *next_argument;
	size_t count, forces;
	enum { HELPER_ARGUMENTS, HELPER_SOURCE, HELPER_BODY, HELPER_APPLY, HELPER_MATCH, HELPER_READY } phase;
};

struct pg_function_graph_state {
	struct pg_graph temporary;
	struct pg_typing *typing;
	struct pg_whnf_work *evaluation;
	const struct pg_evidence *body, *outer_context, *context, *argument_context;
	const struct pg_evidence *recursive_function;
	const struct pg_evidence *source_function;
	const struct pg_evidence *captured_input;
	const struct pg_evidence *specialization;
	struct graph_continuation *head_arguments;
	size_t specialized_arity;
	const struct pg_evidence *domain, *range, *self, *indices;
	const struct pg_evidence *result_context;
	const struct pg_evidence **arguments;
	size_t arity;
	const struct pg_evidence **index_arguments;
	size_t index_count;
	struct pg_inductive_instance input;
	const struct pg_evidence **results;
	struct graph_case *plans;
	struct graph_case *pending, *leaves, **leaf_tail;
	struct graph_case *building, **build_tail;
	struct graph_call *waiting;
	struct pg_typed_query *view;
	union { struct pg_function_source_cursor source; struct helper_cursor helper; };
	size_t leaf_count;
	const struct pg_evidence *formation, *declaration;
	const struct pg_data_schema *schema;
	const struct pg_evidence *packet, *witness, *motive_context, *motive;
	const struct pg_evidence **witness_branches;
	size_t witness_next;
	enum pg_function_graph_status witness_status;
	size_t count, next;
	int cases, induction;
	enum { GRAPH_SOURCE, GRAPH_HEAD, GRAPH_INPUT, GRAPH_PARAMETERS, GRAPH_READY } preparation;
	uint64_t level;
	enum pg_totality totality;
	enum pg_function_graph_status status;
};

static const struct pg_evidence *projection(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	return pg_prove_projection(s->typing, context, proof);
}

static const struct pg_evidence *argument_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument)
{
	if (s->index_count)
		return pg_prove_inductive_motive_substitution(s->typing,
			s->input.formation, s->input.parameters, s->argument_context,
			context, projection(s, context, argument));
	return pg_prove_substitution_pair(s->typing,
		pg_prove_substitution_projection(s->typing, s->context, context), s->argument_context,
		projection(s, context, argument));
}

static const struct pg_evidence *input_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument,
	const struct pg_evidence *const *arguments)
{
	const struct pg_evidence *map = argument_substitution(s, context, argument);
	for (size_t i = 0; map && i < s->arity; ++i)
		map = pg_prove_substitution_pair(s->typing, map, s->arguments[i], projection(s, context, arguments[i]));
	return map;
}

static const struct pg_evidence *map_value(struct pg_function_graph_state *s,
	const struct pg_evidence *map, const struct pg_evidence *value)
{
	if (!map) return NULL;
	return pg_prove_reindex(s->typing, map, projection(s, pg_evidence_premise(map, 0), value));
}

static const struct pg_evidence *apply_relation(struct pg_function_graph_state *s,
	const struct pg_evidence *relation, const struct pg_evidence *map,
	const struct pg_evidence *output)
{
	if (!map) return NULL;
	size_t count = pg_evidence_premise_count(map);
	size_t inputs = s->index_count + s->arity + 1;
	if (count < inputs + 2) return NULL;
	for (size_t i = count - inputs; relation && i < count; ++i)
		relation = pg_prove_family_application(s->typing, relation, pg_evidence_premise(map, i));
	return pg_prove_family_application(s->typing, relation, output);
}

static const struct pg_evidence *parameters_at(struct pg_function_graph_state *s,
	const struct pg_evidence *context)
{
	const struct pg_evidence *map = pg_prove_substitution_projection(s->typing, s->context, context);
	return pg_prove_substitution_compose(s->typing, s->input.parameters, map);
}

static int graph_universe(struct pg_function_graph_state *s, uint64_t *level)
{
	uint64_t a, b;
	if (!pg_universe_level(pg_evidence_classifier(s->domain), &a) ||
		!pg_universe_level(pg_evidence_classifier(s->range), &b)) return -1;
	*level = a > b ? a : b;
	for (size_t i = 0; i < s->index_count; ++i) {
		if (!pg_universe_level(pg_evidence_classifier(pg_evidence_premise(s->index_arguments[i], 1)), &a)) return -1;
		if (a > *level) *level = a;
	}
	for (size_t i = 0; i < s->arity; ++i) {
		if (!pg_universe_level(pg_evidence_classifier(pg_evidence_premise(s->arguments[i], 1)), &a)) return -1;
		if (a > *level) *level = a;
	}
	return 0;
}

static int signature(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *x = s->argument_context;
	const struct pg_evidence *y = pg_prove_context_extension(t, s->result_context, pg_binder(t->graph), s->range);
	const struct pg_evidence *universe = pg_prove_universe(t, y, s->level);
	s->self = pg_prove_family_context_extension(t, s->context, pg_binder(t->graph), y, universe);
	if (!s->self) return -1;
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, s->self);
	for (size_t i = 0; map && i < s->index_count; ++i)
		map = pg_prove_substitution_lift(t, map, s->index_arguments[i], pg_binder(t->graph));
	map = pg_prove_substitution_lift(t, map, x, pg_binder(t->graph));
	for (size_t i = 0; map && i < s->arity; ++i)
		map = pg_prove_substitution_lift(t, map, s->arguments[i], pg_binder(t->graph));
	map = pg_prove_substitution_lift(t, map, y, pg_binder(t->graph));
	s->indices = pg_evidence_premise(map, 1);
	return s->indices ? 0 : -1;
}

static int await_view(struct pg_function_graph_state *s, struct pg_typed_query *view)
{
	if (pg_typed_query_advance(view, 1)) return 0;
	s->view = view;
	return 1;
}

static int application_body(struct pg_function_graph_state *s,
	const struct pg_evidence *function, const struct pg_evidence *argument,
	const struct pg_evidence **body)
{
	struct pg_typed_query *view = pg_application_body_request(s->typing, function, argument);
	if (await_view(s, view)) return 1;
	*body = pg_typed_query_result(view);
	return 0;
}

/* Zero is ready, one is pending, minus one has no supported structural view. */
static int structural_computation_view(struct pg_function_graph_state *s,
	const struct pg_evidence *proof, enum pg_evidence_rule *rule,
	const struct pg_evidence **left, const struct pg_evidence **right)
{
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	const struct pg_term *core = subject->core;
	if (core->kind != PG_APPLICATION) return -1;
	const struct pg_term *terms[] = {core->as.application.function, core->as.application.argument};
	size_t count = 2;
	*rule = PG_APP_ELIM;
	if (terms[0]->kind == PG_REFERENCE) {
		const struct pg_object *operation = terms[0]->as.reference;
		if (operation == &pg_return_operation) *rule = PG_RETURN_INTRO;
		else if (operation == &pg_thunk_operation) *rule = PG_THUNK_INTRO;
		else if (operation == &pg_force_operation) *rule = PG_FORCE_ELIM;
		if (*rule != PG_APP_ELIM) {
			terms[0] = terms[1];
			count = 1;
		}
	} else if (terms[0]->kind == PG_APPLICATION) {
		const struct pg_term *head = terms[0]->as.application.function;
		if (head->kind == PG_REFERENCE && head->as.reference == &pg_fold_operation) {
			*rule = PG_FOLD_ELIM;
			terms[0] = terms[0]->as.application.argument;
		}
	}
	const struct pg_evidence *children[2] = {NULL, NULL};
	for (size_t i = 0; i < count; ++i) {
		struct pg_typed_query *input = pg_typed_input_request(s->typing, proof, i);
		if (await_view(s, input)) return 1;
		children[i] = pg_typed_query_result(input);
		if (!children[i]) return -1;
		const struct pg_occurrence *child = pg_evidence_subject(children[i]);
		if (!child || child->context != subject->context) return -1;
		if (pg_alpha_equal(child->core, terms[i]) != 1) return -1;
	}
	*left = children[0];
	*right = children[1];
	return 0;
}

static int construction_origin(struct pg_function_graph_state *s,
	const struct pg_evidence **proof, const struct pg_evidence **environment)
{
	struct pg_typed_query *origin = pg_construction_origin_request(s->typing, *proof);
	if (await_view(s, origin)) return 1;
	*environment = pg_construction_origin_environment(origin);
	*proof = pg_typed_query_result(origin);
	return *proof ? 0 : -1;
}

static int computation_view(struct pg_function_graph_state *s,
	const struct pg_evidence *proof, enum pg_evidence_rule *rule,
	const struct pg_evidence **left, const struct pg_evidence **right)
{
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	int status = structural_computation_view(s, proof, rule, left, right);
	if (status >= 0) return status;
	const struct pg_evidence *map;
	status = construction_origin(s, &proof, &map);
	if (status) return status;
	*rule = pg_evidence_rule(proof);
	if (*rule == PG_MATCH_ELIM || *rule == PG_INDUCTION_ELIM) {
		*left = map ? pg_prove_elimination_reindex(s->typing, map, proof) : proof;
		*right = NULL;
		return *left ? 0 : -1;
	}
	if (map) proof = pg_prove_reindex(s->typing, map, proof);
	/* Input queries are keyed by the immutable typed subject, not its receipt.
	 * Rechecking the same subject cannot change the failed structural view. */
	if (!proof || pg_evidence_subject(proof) == subject) return -1;
	return structural_computation_view(s, proof, rule, left, right);
}

static int plan_case(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (!s->cases) {
		plan->context = s->result_context;
		plan->computation = projection(s, plan->context, s->body);
		for (size_t i = 0; plan->computation && i < s->arity; ++i)
			plan->computation = pg_prove_application(t, plan->computation,
				pg_prove_variable(t, plan->context, pg_evidence_context(s->arguments[i])->binder));
		return plan->computation ? 0 : -1;
	}
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->input.schema), s->next);
	const struct pg_evidence *fields = pg_data_schema_fields(s->input.schema, constructor);
	const struct pg_context *parameters = pg_evidence_context(pg_data_schema_parameters(s->input.schema));
	if (pg_context_extension_size(pg_evidence_context(fields), parameters, &plan->field_count)) return -1;
	size_t count = plan->field_count;
	if (count > SIZE_MAX / sizeof(*plan->hypothesis_fields)) return -1;
	plan->hypothesis_fields = pg_alloc(&s->temporary, count * sizeof(*plan->hypothesis_fields));
	if (count && !plan->hypothesis_fields) return -1;
	for (size_t i = count; s->induction && i; --i, fields = pg_evidence_premise(fields, 0)) {
		const struct pg_term *type = pg_evidence_context(fields)->declared_type;
		int recursive = pg_data_recursive_field(type, parameters->binder);
		if (recursive < 0) return -1;
		if (!recursive) continue;
		plan->hypothesis_fields[plan->hypothesis_count++] = i - 1;
	}
	for (size_t i = 0; i < plan->hypothesis_count / 2; ++i) {
		size_t j = plan->hypothesis_count - 1 - i, field = plan->hypothesis_fields[i];
		plan->hypothesis_fields[i] = plan->hypothesis_fields[j];
		plan->hypothesis_fields[j] = field;
	}
	if (plan->hypothesis_count > SIZE_MAX - count) return -1;
	count += plan->hypothesis_count;
	if (s->arity > SIZE_MAX - count) return -1;
	count += s->arity;
	if (count > SIZE_MAX / sizeof(*plan->scopes)) return -1;
	plan->scopes = pg_alloc(&s->temporary, count * sizeof(*plan->scopes));
	if (count && !plan->scopes) return -1;
	plan->context = s->argument_context;
	plan->constructor = constructor;
	plan->computation = pg_evidence_premise(s->body, 5 + s->next);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *pi = pg_prove_classifier(t, plan->context, plan->computation);
		const struct pg_evidence *domain = pg_prove_pi_domain(t, pi);
		plan->context = pg_prove_context_extension(t, plan->context, pg_binder(t->graph), domain);
		if (!plan->context) return -1;
		plan->scopes[i] = plan->context;
		const struct pg_evidence *variable = pg_prove_variable(t, plan->context, pg_evidence_context(plan->context)->binder);
		plan->computation = pg_prove_application_body(t, projection(s, plan->context, plan->computation), variable);
		if (!plan->computation) return -1;
	}
	return 0;
}

static int split_case(struct pg_function_graph_state *s, struct graph_case *plan,
	const struct pg_evidence *elimination)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *formation = pg_evidence_premise(elimination, 1);
	const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
	if (!layout) return -1;
	plan->split_formation = formation;
	plan->discriminant = pg_evidence_premise(elimination, 3);
	const struct pg_evidence *type = pg_prove_classifier(t, plan->context, plan->discriminant);
	uint64_t level;
	if (!type || !pg_universe_level(pg_evidence_classifier(type), &level)) return -1;
	if (level > s->level) s->level = level;
	plan->child_count = pg_data_layout_count(layout);
	if (plan->child_count > SIZE_MAX / sizeof(*plan->children)) return -1;
	plan->children = pg_alloc(&s->temporary, plan->child_count * sizeof(*plan->children));
	if (plan->child_count && !plan->children) return -1;
	for (size_t i = 0; i < plan->child_count; ++i) {
		struct graph_case *child = &plan->children[i];
		const struct pg_object *constructor = pg_data_constructor(layout, i);
		const struct pg_evidence *map = pg_prove_constructor_refinement(t,
			plan->context, plan->discriminant, constructor);
		if (!map) return -1;
		*child = (struct graph_case){.parent = plan, .refinement = map, .constructor = constructor,
			.field_count = plan->field_count, .hypothesis_count = plan->hypothesis_count,
			.scopes = plan->scopes, .hypothesis_fields = plan->hypothesis_fields,
			.calls = plan->calls, .call_count = plan->call_count, .first_call = plan->call_count,
			.context = pg_evidence_premise(map, 1)};
		child->origin_map = plan->origin_map ? pg_prove_substitution_compose(t, plan->origin_map,
			pg_prove_substitution_projection(t, pg_evidence_premise(plan->origin_map, 1), plan->context))
			: pg_prove_substitution_projection(t, plan->context, plan->context);
		child->origin_map = pg_prove_substitution_compose(t, child->origin_map, map);
		child->computation = pg_prove_elimination_body(t,
			pg_prove_elimination_reindex(t, map, elimination));
		if (!child->origin_map || !child->computation) return -1;
		struct graph_continuation **tail = &child->continuations;
		for (const struct graph_continuation *old = plan->continuations; old; old = old->next) {
			struct graph_continuation *frame = pg_alloc(&s->temporary, sizeof(*frame));
			if (!frame) return -1;
			frame->function = old->function ? map_value(s, map, old->function) : NULL;
			frame->argument = old->argument ? map_value(s, map, old->argument) : NULL;
			if ((old->function && !frame->function) || (old->argument && !frame->argument)) return -1;
			*tail = frame; tail = &frame->next;
		}
	}
	return 0;
}

static int plan_result(struct pg_function_graph_state *s, struct graph_case *plan,
	const struct pg_evidence *value)
{
	if (!value) return -1;
	if (!plan->continuations) { plan->output = value; return 1; }
	if (!plan->continuations->function) return -1;
	const struct pg_evidence *continuation = projection(s, plan->context, plan->continuations->function);
	plan->continuations = plan->continuations->next;
	plan->computation = pg_prove_application_body(s->typing, continuation, projection(s, plan->context, value));
	return plan->computation ? 0 : -1;
}

static size_t hypothesis_position(struct pg_function_graph_state *s,
	const struct graph_case *plan, const struct pg_term *core)
{
	for (size_t i = 0; i < plan->hypothesis_count; ++i) {
		const struct pg_evidence *scope = plan->scopes[plan->field_count + i];
		const struct pg_evidence *hypothesis = pg_prove_variable(s->typing, scope, pg_evidence_context(scope)->binder);
		if (plan->origin_map) hypothesis = map_value(s, plan->origin_map, hypothesis);
		if (hypothesis && core == pg_evidence_subject(hypothesis)->core) return i;
	}
	return plan->hypothesis_count;
}

/* A callable parameter has no Lambda body to expose. Eta expansion gives
 * the ordinary graph builder a typed source without evaluating the parameter.
 * Use the Pi's binders so repeated requests retain the same source proof. */
static const struct pg_evidence *parameter_source(struct pg_function_graph_state *s,
	const struct pg_evidence *parameter)
{
	if (pg_evidence_rule(parameter) != PG_VARIABLE) return NULL;
	struct pg_typing *t = s->typing;
	const struct pg_evidence *outer = pg_evidence_premise(parameter, 0);
	const struct pg_evidence *body = pg_prove_force(t, parameter);
	if (!body) return NULL;
	struct pg_call_telescope telescope;
	if (pg_prove_call_telescope(t, outer, body, pg_evidence_classifier(body), &telescope)) return NULL;
	if (telescope.context == outer) return NULL;
	return pg_prove_abstract(t, outer, telescope.context, telescope.call);
}

/* Preserve a helper's complete typed call before beta exposure
 * erases its function boundary. Its graph is requested from the same owner
 * that services public @f and *f requests, not generated afresh at each call.
 * Return 2 while its input view is pending, before publishing a helper call. */
static int helper_call(struct pg_function_graph_state *s, struct graph_case *plan,
	const struct pg_evidence *computation)
{
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(pg_evidence_classifier(computation), &totality, &content)) return 0;
	struct helper_cursor *h = &s->helper;
	if (!h->function) h->function = computation;
	int result = 0;
	while (h->phase == HELPER_ARGUMENTS) {
		int status = construction_origin(s, &h->function, &h->environment);
		if (status) { result = status > 0 ? 2 : 0; goto done; }
		enum pg_evidence_rule rule = pg_evidence_rule(h->function);
		if (rule == PG_APP_ELIM) {
			struct graph_continuation *item = pg_alloc(&h->temporary, sizeof(*item));
			if (!item || h->count == SIZE_MAX / sizeof(const struct pg_evidence *)) { result = -1; goto done; }
			item->argument = pg_evidence_premise(h->function, 1);
			if (h->environment) item->argument = map_value(s, h->environment, item->argument);
			item->next = h->arguments; h->arguments = item; ++h->count;
			h->function = pg_evidence_premise(h->function, 0);
			if (h->environment) h->function = map_value(s, h->environment, h->function);
			continue;
		}
		if (rule == PG_FORCE_ELIM || rule == PG_THUNK_COMPUTATION) ++h->forces;
		else if (rule == PG_THUNK_INTRO && h->forces) --h->forces;
		else { h->phase = HELPER_SOURCE; break; }
		h->function = pg_evidence_premise(h->function, 0);
		if (h->environment) h->function = map_value(s, h->environment, h->function);
	}
	if (!h->count) goto done;
	if (h->phase == HELPER_SOURCE && h->forces == 1 && pg_evidence_rule(h->function) == PG_VARIABLE) {
		if (!h->source.function) {
			const struct pg_evidence *current = h->environment ? map_value(s, h->environment, h->function) : h->function;
			if (!current) goto done;
			if (hypothesis_position(s, plan, pg_evidence_subject(current)->core) != plan->hypothesis_count) goto done;
			h->source.function = current;
		}
		int status = pg_function_source_advance(s->typing, &h->source);
		if (!status) { result = 2; goto done; }
		const struct pg_evidence *concrete = status > 0 ? h->source.function : NULL;
		if (concrete) {
			const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(pg_evidence_premise(concrete, 0), 0), 0);
			h->environment = pg_prove_substitution_projection(s->typing, context, plan->context);
			if (!h->environment) goto done;
			h->function = concrete;
		} else h->function = parameter_source(s, h->function);
		if (!h->function) goto done;
		/* The eta graph of an opaque parameter is its leaf, not a request
		 * for another graph of that same parameter. */
		if (pg_evidence_subject(h->function) == pg_evidence_subject(s->source_function)) goto done;
		h->phase = HELPER_READY;
	} else if (h->phase == HELPER_SOURCE) {
		if (h->forces || pg_evidence_rule(h->function) != PG_LAMBDA_INTRO) goto done;
		h->body = h->function;
		h->phase = HELPER_BODY;
	}
	if (h->phase == HELPER_BODY) {
		for (;;) {
			const struct pg_evidence *body_environment;
			int status = construction_origin(s, &h->body, &body_environment);
			if (status) { result = status > 0 ? 2 : 0; goto done; }
			if (pg_evidence_rule(h->body) == PG_LAMBDA_INTRO) h->body = pg_evidence_premise(h->body, 1);
			else if (pg_evidence_rule(h->body) == PG_APP_ELIM) h->body = pg_evidence_premise(h->body, 0);
			else if (pg_evidence_rule(h->body) == PG_FORCE_ELIM || pg_evidence_rule(h->body) == PG_THUNK_COMPUTATION ||
				pg_evidence_rule(h->body) == PG_THUNK_INTRO) h->body = pg_evidence_premise(h->body, 0);
			else break;
		}
		switch (pg_evidence_rule(h->body)) {
		case PG_MATCH_ELIM: {
			/* A split may refine local graph inputs, but must not replace
			 * the fixed parameter telescope. Retain this call when its
			 * discriminant belongs to that telescope. */
			h->body = h->environment ? map_value(s, h->environment, h->function) : h->function;
			h->body = projection(s, plan->context, h->body);
			if (!h->body) goto done;
			h->next_argument = h->arguments;
			h->phase = HELPER_APPLY;
			break;
		}
		case PG_INDUCTION_ELIM: h->phase = HELPER_READY; break;
		default: goto done;
		}
	}
	if (h->phase == HELPER_APPLY) {
		while (h->next_argument) {
			const struct graph_continuation *a = h->next_argument;
			if (application_body(s, h->body, projection(s, plan->context, a->argument), &h->body)) {
				result = 2; goto done;
			}
			if (!h->body) goto done;
			h->next_argument = a->next;
		}
		h->phase = HELPER_MATCH;
	}
	if (h->phase == HELPER_MATCH) {
		enum pg_evidence_rule rule;
		const struct pg_evidence *elimination, *unused;
		int view = computation_view(s, h->body, &rule, &elimination, &unused);
		if (view > 0) { result = 2; goto done; }
		if (view < 0 || rule != PG_MATCH_ELIM) goto done;
		const struct pg_evidence *input = pg_evidence_premise(elimination, 3);
		const struct pg_term *term = pg_evidence_subject(input)->core;
		if (term->kind != PG_REFERENCE || !pg_context_lookup(pg_evidence_context(s->context), term->as.reference)) goto done;
	}
	if (pg_evidence_subject(h->function) == pg_evidence_subject(s->source_function)) { result = -1; goto done; }
	struct graph_call *call = pg_alloc(&s->temporary, sizeof(*call));
	if (!call) { result = -1; goto done; }
	*call = (struct graph_call){.field = SIZE_MAX, .hypothesis = SIZE_MAX,
		.helper_source = h->function, .helper_environment = h->environment, .helper_arity = h->count};
	call->helper_arguments = pg_alloc(&s->temporary, h->count * sizeof(*call->helper_arguments));
	if (!call->helper_arguments) { result = -1; goto done; }
	for (size_t i = 0; i < h->count; ++i, h->arguments = h->arguments->next)
		call->helper_arguments[i] = h->arguments->argument;
	s->waiting = call;
	result = 1;
done:
	if (result != 2) {
		pg_graph_destroy(&h->temporary);
		*h = (struct helper_cursor){0};
	}
	return result;
}

static int record_call(struct pg_function_graph_state *s, struct graph_case *plan, struct graph_call *call)
{
	struct pg_typing *t = s->typing;
	if (plan->call_count == SIZE_MAX) return -1;
	const struct pg_evidence *type = pg_prove_classifier(t, plan->context, plan->computation);
	type = pg_prove_return_content(t, type);
	plan->context = pg_prove_context_extension(t, plan->context, pg_binder(t->graph), type);
	if (!plan->context) return -1;
	call->result_context = plan->context;
	call->previous = plan->calls;
	plan->calls = call;
	++plan->call_count;
	return plan_result(s, plan, pg_prove_variable(t, plan->context, pg_evidence_context(plan->context)->binder));
}

/* Symbolically expose sequencing using retained typing evidence. A recursive
 * call creates an output variable; it is not asserted equal by conversion.
 * The companion witness will supply this variable from the corresponding IH. */
static int plan_step(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (s->waiting) {
		if (!s->waiting->helper) return 0;
		struct graph_call *call = s->waiting;
		s->waiting = NULL;
		return record_call(s, plan, call);
	}
	if (plan->normalization) {
		enum pg_eval_status status = pg_whnf_advance(plan->normalization, 1);
		if (status == PG_EVAL_PENDING) return 0;
		if (status != PG_EVAL_WHNF) return -1;
		const struct pg_evidence *normalized = pg_prove_normalization(t, plan->computation,
			pg_whnf_certificate(plan->normalization));
		plan->normalization = NULL;
		const struct pg_evidence *value = pg_prove_return_value(t, normalized);
		if (!value) value = pg_prove_total_pure_value(t, normalized);
		return plan_result(s, plan, value);
	}
	if (plan->continuations && plan->continuations->argument) {
		/* Sequencing can expose a callable with its arguments still on the
		 * continuation stack. Preserve that typed call before beta exposure. */
		const struct pg_evidence *call = plan->computation;
		struct graph_continuation *rest = plan->continuations;
		while (call && rest && rest->argument) {
			call = pg_prove_application(t, call, projection(s, plan->context, rest->argument));
			rest = rest->next;
		}
		int helper = helper_call(s, plan, call);
		if (helper == 2) return 0;
		if (helper) {
			if (helper < 0) return -1;
			plan->computation = call;
			plan->continuations = rest;
			return 0;
		}
		const struct pg_evidence *body = pg_prove_application_body(t, plan->computation,
			projection(s, plan->context, plan->continuations->argument));
		if (body) {
			plan->continuations = plan->continuations->next;
			plan->computation = body;
			return 0;
		}
	}
	const struct pg_evidence *left, *right, *value = NULL;
	enum pg_evidence_rule rule;
	int view = computation_view(s, plan->computation, &rule, &left, &right);
	if (view > 0) return 0;
	if (view < 0) goto normalize;
	switch (rule) {
	case PG_FOLD_ELIM: {
		struct graph_continuation *frame = pg_alloc(&s->temporary, sizeof(*frame));
		if (!frame) return -1;
		*frame = (struct graph_continuation){.function = right, .next = plan->continuations};
		plan->continuations = frame;
		plan->computation = left;
		return 0;
	}
	case PG_APP_ELIM: {
		int helper = helper_call(s, plan, plan->computation);
		if (helper) return helper < 0 ? -1 : 0;
		const struct pg_evidence *body = pg_prove_application_body(t, left, right);
		if (body) { plan->computation = body; return 0; }
		struct graph_continuation *frame = pg_alloc(&s->temporary, sizeof(*frame));
		if (!frame) return -1;
		*frame = (struct graph_continuation){.argument = right, .next = plan->continuations};
		plan->continuations = frame;
		plan->computation = left;
		return 0;
	}
	case PG_FORCE_ELIM: {
		const struct pg_term *core = pg_evidence_subject(left)->core;
		size_t i = hypothesis_position(s, plan, core);
		if (i == plan->hypothesis_count) {
			view = computation_view(s, left, &rule, &left, &right);
			if (view > 0) return 0;
			if (view < 0 || rule != PG_THUNK_INTRO) goto normalize;
			plan->computation = left;
			return 0;
		}
		struct graph_call *call = pg_alloc(&s->temporary, sizeof(*call));
		if (!call || plan->call_count == SIZE_MAX) return -1;
		*call = (struct graph_call){.field = plan->hypothesis_fields[i], .hypothesis = i};
		call->child = pg_prove_variable(t, plan->scopes[call->field], pg_evidence_context(plan->scopes[call->field])->binder);
		if (plan->origin_map) call->child = map_value(s, plan->origin_map, call->child);
		if (!call->child) return -1;
		const struct pg_term *type, *domain, *codomain;
		const struct pg_object *binder;
		if (pg_thunk_type_view(pg_evidence_classifier(call->child), &type)) {
			while (pg_pi_view(type, &domain, &binder, &codomain)) {
				if (call->field_arity == SIZE_MAX / sizeof(*call->field_arguments)) return -1;
				++call->field_arity; type = codomain;
			}
			call->field_arguments = pg_alloc(&s->temporary, call->field_arity * sizeof(*call->field_arguments));
			if (call->field_arity && !call->field_arguments) return -1;
			const struct pg_evidence *child_call = pg_prove_force(t, projection(s, plan->context, call->child));
			for (size_t j = 0; child_call && j < call->field_arity; ++j) {
				struct graph_continuation *frame = plan->continuations;
				if (!frame || !frame->argument) return -1;
				call->field_arguments[j] = frame->argument;
				const struct pg_evidence *argument = projection(s, plan->context, frame->argument);
				child_call = pg_prove_application(t, child_call, argument);
				plan->computation = pg_prove_application(t, plan->computation, argument);
				if (!plan->computation) return -1;
				plan->continuations = frame->next;
			}
			call->child = pg_prove_total_pure_value(t, child_call);
			if (!call->child) return -1;
		}
		call->arguments = pg_alloc(&s->temporary, s->arity * sizeof(*call->arguments));
		if (s->arity && !call->arguments) return -1;
		for (size_t j = 0; j < s->arity; ++j) {
			struct graph_continuation *frame = plan->continuations;
			if (!frame || !frame->argument) return -1;
			call->arguments[j] = frame->argument;
			plan->computation = pg_prove_application(t, plan->computation, projection(s, plan->context, call->arguments[j]));
			if (!plan->computation) return -1;
			plan->continuations = frame->next;
		}
		return record_call(s, plan, call);
	}
	case PG_RETURN_INTRO: value = left; break;
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: {
		const struct pg_evidence *body = pg_prove_elimination_body(t, left);
		if (!body) {
			if (pg_evidence_rule(left) == PG_INDUCTION_ELIM) goto normalize;
			return split_case(s, plan, left);
		}
		plan->computation = body;
		return 0;
	}
	default: return -1;
	}
	return plan_result(s, plan, value);
normalize:
	/* Normalize the saturated call, retaining outer sequencing. A neutral
	 * result needs the checked TOTAL/empty-row projection above. */
	while (plan->continuations && plan->continuations->argument) {
		plan->computation = pg_prove_application(t, plan->computation,
			projection(s, plan->context, plan->continuations->argument));
		if (!plan->computation) return -1;
		plan->continuations = plan->continuations->next;
	}
	plan->normalization = pg_whnf_request(s->evaluation, &pg_pure_policy, pg_evidence_subject(plan->computation)->core);
	return plan->normalization ? 0 : -1;
}

/* Match public source slots to executed calls without changing sequencing.
 * Unused IHs are not calls; repeated calls receive distinct result binders. */
static int order_calls(struct pg_function_graph_state *s, struct graph_case *plan)
{
	size_t count = plan->call_count - plan->first_call;
	if (count > SIZE_MAX / sizeof(*plan->ordered)) return -1;
	if (count > SIZE_MAX / sizeof(*plan->executed)) return -1;
	plan->ordered = pg_alloc(&s->temporary, count * sizeof(*plan->ordered));
	plan->executed = pg_alloc(&s->temporary, count * sizeof(*plan->executed));
	if (count && (!plan->ordered || !plan->executed)) return -1;
	const struct graph_call *call = plan->calls;
	for (size_t i = count; i; --i) {
		if (!call) return -1;
		plan->executed[i - 1].call = call;
		call = call->previous;
	}
	if (call != (plan->parent ? plan->parent->calls : NULL)) return -1;
	/* A refined branch has no flat source telescope. Its prefix and child
	 * calls use the typed execution order, just like its exported fields. */
	if (plan->child_count) plan->source_order = NULL;
	for (size_t i = 0; i < count; ++i)
		if (plan->executed[i].call->helper) plan->source_order = NULL;
	if (!plan->source_order) {
		for (size_t slot = 0; slot < count; ++slot) {
			plan->executed[slot].slot = slot;
			plan->ordered[slot] = &plan->executed[slot];
		}
		return 0;
	}
	if (plan->source_order->count != count) return -1;
	struct graph_call_layout **heads = pg_alloc(&s->temporary, plan->field_count * sizeof(*heads));
	struct graph_call_layout **tails = pg_alloc(&s->temporary, plan->field_count * sizeof(*tails));
	if (plan->field_count && (!heads || !tails)) return -1;
	for (size_t i = 0; i < count; ++i) {
		struct graph_call_layout *entry = &plan->executed[i];
		call = entry->call;
		if (call->field >= plan->field_count) return -1;
		if (tails[call->field]) tails[call->field]->field_next = entry;
		else heads[call->field] = entry;
		tails[call->field] = entry;
	}
	for (size_t slot = 0; slot < count; ++slot) {
		size_t field = plan->source_order->fields[slot];
		if (field >= plan->field_count || !heads[field]) return -1;
		struct graph_call_layout *entry = heads[field];
		heads[field] = entry->field_next;
		entry->slot = slot;
		plan->ordered[slot] = entry;
	}
	return 0;
}

/* Reconstruct the original recursive function at the same child. A function
 * field supplies that child through its checked total pure result expression. */
static const struct pg_evidence *original_hypothesis(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *field)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *scope = context, *child = field;
	const struct pg_term *type;
	if (pg_thunk_type_view(pg_evidence_classifier(field), &type)) {
		struct pg_call_telescope telescope;
		if (pg_prove_call_telescope(t, scope, pg_prove_force(t, field), NULL, &telescope)) return NULL;
		scope = telescope.context;
		child = pg_prove_total_pure_value(t, telescope.call);
	}
	if (!child) return NULL;
	const struct pg_evidence *input = argument_substitution(s, scope, child);
	const struct pg_evidence *call = projection(s, scope, s->recursive_function);
	for (size_t j = 0; call && j < s->index_count; ++j)
		call = pg_prove_application(t, call, pg_substitution_image(t, input,
			pg_evidence_context(s->index_arguments[j])->binder));
	call = pg_prove_application(t, call, child);
	call = pg_prove_abstract(t, context, scope, call);
	return pg_prove_thunk(t, call);
}

/* Schema and witness use the same source scope: fields, original IH functions,
 * then remaining Pi arguments. Only the destination's supplied fields differ. */
static const struct pg_evidence *case_source_map(struct pg_function_graph_state *s,
	const struct graph_case *plan, const struct pg_evidence *context,
	const struct pg_evidence *argument, const struct pg_evidence *const *fields,
	const struct pg_evidence **arguments)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *map = argument_substitution(s, context, argument);
	size_t count = plan->field_count;
	for (size_t i = 0; map && i < count; ++i)
		map = pg_prove_substitution_pair(t, map, plan->scopes[i], fields[i]);
	for (size_t i = 0; map && i < plan->hypothesis_count; ++i) {
		const struct pg_evidence *field = fields[plan->hypothesis_fields[i]];
		map = pg_prove_substitution_pair(t, map, plan->scopes[count + i], original_hypothesis(s, context, field));
	}
	for (size_t i = 0; map && i < s->arity; ++i) {
		const struct pg_evidence *source = plan->scopes[count + plan->hypothesis_count + i];
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), map_value(s, map, pg_evidence_premise(source, 1)));
		if (!context) return NULL;
		arguments[i] = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		map = pg_prove_substitution_compose(t, map,
			pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context));
		map = pg_prove_substitution_pair(t, map, source, arguments[i]);
	}
	return map;
}

static const struct pg_evidence *case_base(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (plan->parent) {
		const struct graph_case *parent = plan->parent;
		const struct pg_evidence *value = map_value(s, parent->schema_map, parent->discriminant);
		plan->schema_refinement = pg_prove_constructor_refinement(t,
			pg_evidence_premise(parent->schema_map, 1), value, plan->constructor);
		plan->base_input = pg_prove_substitution_compose(t, parent->schema_input, plan->schema_refinement);
		return pg_prove_refinement_factor(t, plan->refinement,
			pg_prove_substitution_compose(t, parent->schema_map, plan->schema_refinement),
			pg_evidence_subject(parent->discriminant)->core->as.reference);
	}
	if (!s->cases) {
		const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, s->self);
		map = pg_prove_substitution_lift(t, map, s->argument_context, pg_binder(t->graph));
		for (size_t i = 0; map && i < s->arity; ++i)
			map = pg_prove_substitution_lift(t, map, s->arguments[i], pg_binder(t->graph));
		plan->base_input = map;
		return map;
	}
	const struct pg_object *constructor = plan->constructor;
	const struct pg_evidence *parameters = parameters_at(s, s->self);
	const struct pg_evidence *fields = pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!fields) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(fields, 1);
	size_t offset = pg_evidence_premise_count(parameters) + 1;
	size_t count = pg_evidence_premise_count(fields) - offset;
	const struct pg_evidence **values = pg_alloc(&s->temporary, count * sizeof(*values));
	if (count && !values) return NULL;
	for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(fields, offset + i);
	const struct pg_evidence *argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, context), count, values);
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, s->arity * sizeof(*arguments));
	if (s->arity && !arguments) return NULL;
	const struct pg_evidence *map = case_source_map(s, plan, context, argument, values, arguments);
	if (!map) return NULL;
	plan->base_input = input_substitution(s, pg_evidence_premise(map, 1), argument, arguments);
	return map;
}

static const struct pg_evidence *helper_application(struct pg_function_graph_state *s,
	const struct graph_call *call, const struct pg_evidence *map, int witness)
{
	const struct pg_evidence *proof = witness ? pg_function_graph_witness(call->helper)
		: pg_function_graph_formation(call->helper);
	if (call->helper_environment) proof = pg_prove_reindex(s->typing, call->helper_environment, proof);
	proof = map_value(s, map, proof);
	for (size_t i = 0; proof && i < call->helper_arity; ++i) {
		const struct pg_evidence *argument = map_value(s, map, call->helper_arguments[i]);
		if (witness) proof = pg_prove_application(s->typing, proof, argument);
		else {
			const struct pg_evidence *body = pg_prove_application_body(s->typing, proof, argument);
			proof = body ? body : pg_prove_family_application(s->typing, proof, argument);
		}
	}
	return proof;
}

static const struct pg_evidence *helper_result_type(struct pg_function_graph_state *s,
	const struct graph_call *call, const struct pg_evidence *map)
{
	struct pg_typing *t = s->typing;
	const struct pg_function_graph_state *helper = call->helper->state;
	const struct pg_evidence *environment = call->helper_environment;
	if (environment) environment = pg_prove_substitution_compose(t, environment,
		pg_prove_substitution_projection(t, pg_evidence_premise(environment, 1), pg_evidence_premise(map, 0)));
	else environment = pg_prove_substitution_projection(t, helper->outer_context, pg_evidence_premise(map, 0));
	environment = pg_prove_substitution_compose(t, environment, map);
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, call->helper_arity * sizeof(*arguments));
	if (!arguments) return NULL;
	for (size_t i = 0; i < call->helper_arity; ++i) arguments[i] = map_value(s, map, call->helper_arguments[i]);
	size_t remaining = helper->arity - helper->specialized_arity;
	if (helper->specialization) {
		if (call->helper_arity < remaining) return NULL;
		size_t prefix = call->helper_arity - remaining;
		environment = pg_prove_substitution_extend(t, environment, helper->context, prefix, arguments);
		environment = pg_prove_substitution_compose(t, helper->specialization, environment);
		arguments += prefix;
	}
	environment = pg_prove_substitution_extend(t, environment, helper->result_context,
		helper->specialization ? remaining : call->helper_arity, arguments);
	return pg_prove_reindex(t, environment, helper->range);
}

static int case_branch(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (order_calls(s, plan)) return -1;
	const struct pg_evidence *map = case_base(s, plan);
	if (!map || !plan->base_input) return -1;
	const struct pg_evidence *context = pg_evidence_premise(map, 1);
	plan->schema_base = context;
	const struct pg_evidence **call_arguments = pg_alloc(&s->temporary, s->arity * sizeof(*call_arguments));
	if (s->arity && !call_arguments) return -1;
	size_t calls = plan->call_count - plan->first_call;
	size_t mapped = 0;
	for (size_t slot = 0; slot < calls; ++slot) {
		struct graph_call_layout *entry = plan->ordered[slot];
		const struct graph_call *call = entry->call;
		const struct pg_evidence *input = NULL, *result_type;
		if (call->helper) result_type = helper_result_type(s, call, map);
		else {
			for (size_t i = 0; i < s->arity; ++i) call_arguments[i] = map_value(s, map, call->arguments[i]);
			input = input_substitution(s, context, map_value(s, map, call->child), call_arguments);
			result_type = pg_prove_reindex(t, input, s->range);
		}
		const struct pg_evidence *before = context;
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), projection(s, context, result_type));
		if (!context) return -1;
		const struct pg_evidence *output = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		const struct pg_evidence *relation;
		if (call->helper) relation = pg_prove_family_application(t,
			projection(s, context, helper_application(s, call, map, 0)), output);
		else {
			input = pg_prove_substitution_compose(t, input, pg_prove_substitution_projection(t, before, context));
			relation = apply_relation(s, pg_prove_variable(t, context, pg_evidence_context(s->self)->binder), input, output);
		}
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), relation);
		if (!context) return -1;
		entry->layout_output = output;
		while (mapped < calls && plan->executed[mapped].layout_output) {
			const struct graph_call_layout *ready = &plan->executed[mapped];
			map = pg_prove_substitution_compose(t, map,
				pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context));
			map = pg_prove_substitution_pair(t, map, ready->call->result_context, projection(s, context, ready->layout_output));
			if (!map) return -1;
			++mapped;
		}
	}
	if (mapped != calls) return -1;
	const struct pg_evidence *lift = pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context);
	plan->schema_map = pg_prove_substitution_compose(t, map, lift);
	plan->schema_input = pg_prove_substitution_compose(t, plan->base_input,
		pg_prove_substitution_projection(t, plan->schema_base, context));
	if (!plan->schema_map || !plan->schema_input) return -1;
	if (plan->discriminant) return 0;
	const struct pg_evidence *output = pg_prove_reindex(t, plan->schema_map, plan->output);
	size_t arity = s->index_count + s->arity + 2;
	const struct pg_evidence **values = pg_alloc(&s->temporary, arity * sizeof(*values));
	if (!values || !output) return -1;
	size_t inputs = pg_evidence_premise_count(plan->schema_input);
	for (size_t i = 0; i < arity - 1; ++i)
		values[i] = pg_evidence_premise(plan->schema_input, inputs - arity + 1 + i);
	values[arity - 1] = output;
	plan->result = pg_prove_substitution_extend(t,
		pg_prove_substitution_projection(t, s->self, context), s->indices, arity, values);
	if (!plan->result || s->leaf_count == SIZE_MAX) return -1;
	plan->leaf = s->leaf_count++;
	*s->leaf_tail = plan;
	s->leaf_tail = &plan->leaf_next;
	return 0;
}

int pg_function_source_advance(struct pg_typing *typing, struct pg_function_source_cursor *source)
{
	if (!source || !pg_evidence_owned_by(source->function, typing)) return -1;
	if (!source->query) source->query = pg_construction_origin_request(typing, source->function);
	if (!pg_typed_query_advance(source->query, 1)) return 0;
	const struct pg_evidence *function = pg_typed_query_result(source->query);
	if (!function) return -1;
	source->function = function;
	const struct pg_evidence *environment = pg_construction_origin_environment(source->query);
	source->query = NULL;
	if (source->applying) { source->applying = 0; return 0; }
	enum pg_evidence_rule rule = pg_evidence_rule(function);
	if (rule == PG_VARIABLE && environment) {
		const struct pg_term *variable = pg_evidence_subject(function)->core;
		const struct pg_evidence *image = pg_substitution_image(typing, environment, variable->as.reference);
		if (!image || pg_evidence_subject(image)->core == variable) return -1;
		source->function = image;
		return 0;
	}
	if (rule == PG_LAMBDA_INTRO) {
		if (!environment) return 1;
		const struct pg_context *scope = pg_evidence_context(function);
		if (pg_evidence_context_map(environment) == pg_context_map_projection(typing, scope,
			pg_evidence_context(environment))) return 1;
		/* Specialization keeps its typed context action. Reuse the mapped
		 * Core binder, including capture avoidance, rather than freshening
		 * a different graph source on every request. */
		const struct pg_evidence *mapped = pg_prove_reindex(typing, environment, function);
		const struct pg_term *core = mapped ? pg_evidence_subject(mapped)->core : NULL;
		if (!core || core->kind != PG_LAMBDA) return -1;
		const struct pg_evidence *extension = pg_evidence_premise(pg_evidence_premise(function, 0), 0);
		const struct pg_evidence *lifted = pg_prove_substitution_lift(typing, environment, extension, core->as.lambda.binder);
		const struct pg_evidence *body = pg_prove_reindex(typing, lifted, pg_evidence_premise(function, 1));
		source->function = pg_prove_abstract(typing, pg_evidence_premise(environment, 1), pg_evidence_premise(lifted, 1), body);
		return source->function ? 1 : -1;
	}
	if (rule == PG_APP_ELIM) {
		const struct pg_evidence *callee = pg_evidence_premise(function, 0);
		const struct pg_evidence *argument = pg_evidence_premise(function, 1);
		if (environment) {
			callee = pg_prove_reindex(typing, environment, callee);
			argument = pg_prove_reindex(typing, environment, argument);
		}
		source->query = pg_application_body_request(typing, callee, argument);
		source->applying = 1;
		return source->query ? 0 : -1;
	}
	/* Invert only the introduction just checked from typed construction. */
	if (rule != PG_THUNK_INTRO && rule != PG_FORCE_ELIM && rule != PG_THUNK_COMPUTATION) return -1;
	source->function = pg_evidence_premise(function, 0);
	if (environment) source->function = pg_prove_reindex(typing, environment, source->function);
	return source->function ? 0 : -1;
}

const struct pg_evidence *pg_function_graph_source(struct pg_typing *typing,
	const struct pg_evidence *function)
{
	struct pg_function_source_cursor source = {.function = function};
	int status;
	while (!(status = pg_function_source_advance(typing, &source))) {}
	return status > 0 ? source.function : NULL;
}

static int prepare_head(struct pg_function_graph_state *s);

static int function_signature(struct pg_function_graph_state *s, const struct pg_evidence *function)
{
	struct pg_typing *typing = s->typing;
	s->recursive_function = function;
	const struct pg_evidence *pi = pg_evidence_premise(function, 0);
	s->argument_context = pg_evidence_premise(pi, 0);
	s->context = pg_evidence_premise(s->argument_context, 0);
	if (pg_evidence_rule(s->argument_context) != PG_CONTEXT_EXTEND) goto unsupported;
	s->domain = pg_evidence_premise(s->argument_context, 1);
	const struct pg_evidence *result = pg_evidence_premise(pi, 1);
	s->result_context = s->argument_context;
	s->arity = 0;
	const struct pg_term *result_type = pg_evidence_subject(result)->core, *argument_type, *codomain;
	const struct pg_object *result_binder;
	while (pg_pi_view(result_type, &argument_type, &result_binder, &codomain)) {
		++s->arity;
		result_type = codomain;
	}
	if (s->arity > SIZE_MAX / sizeof(*s->arguments)) goto error;
	s->arguments = pg_alloc(&s->temporary, s->arity * sizeof(*s->arguments));
	if (s->arity && !s->arguments) goto error;
	for (size_t i = 0; i < s->arity; ++i) {
		const struct pg_evidence *domain = pg_prove_pi_domain(typing, result);
		s->result_context = pg_prove_context_extension(typing, s->result_context, pg_binder(typing->graph), domain);
		if (!s->result_context) goto unsupported;
		s->arguments[i] = s->result_context;
		result = pg_prove_pi_codomain(typing, projection(s, s->result_context, result),
			pg_prove_variable(typing, s->result_context, pg_evidence_context(s->result_context)->binder));
		if (!result) goto unsupported;
	}
	const struct pg_term *range;
	if (!result || !pg_pure_computation_type_view(pg_evidence_subject(result)->core, &s->totality, &range)) goto unsupported;
	s->range = pg_prove_return_content(typing, result);
	if (!s->range) goto unsupported;
	s->body = pg_evidence_premise(function, 1);
	return 0;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return -1;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
	return -1;
}

int pg_function_graph_init(struct pg_function_graph_work *work,
	struct pg_typing *typing,
	struct pg_whnf_work *evaluation, const struct pg_evidence *function)
{
	if (!work) return -1;
	*work = (struct pg_function_graph_work){0};
	if (!typing || !evaluation) return -1;
	if (evaluation->graph != typing->graph) return -1;
	if (!pg_evidence_owned_by(function, typing)) return -1;
	struct pg_function_graph_state *s = calloc(1, sizeof(*s));
	if (!s) return -1;
	work->state = s;
	s->typing = typing; s->evaluation = evaluation;
	s->source.function = function;
	return 0;
}

/* Abstract the eliminator's index telescope and input, not its captured
 * environment. Publication specializes the same checked local function. */
static int capture_eliminator(struct pg_function_graph_state *s, const struct pg_evidence *scrutinee)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *context = s->argument_context;
	const struct pg_evidence *scope = pg_prove_inductive_motive_context(t,
		pg_evidence_premise(s->body, 1), pg_evidence_premise(s->body, 2), pg_binder(t->graph));
	if (!scope) return -1;
	const struct pg_evidence *map = pg_prove_substitution_projection(t, context, scope);
	const struct pg_evidence *body = pg_prove_elimination_reindex(t, map, s->body);
	if (!body) return -1;
	size_t count = pg_evidence_premise_count(body) - 6;
	const struct pg_evidence **branches = pg_alloc(&s->temporary, count * sizeof(*branches));
	if (count && !branches) return -1;
	for (size_t i = 0; i < count; ++i) branches[i] = pg_evidence_premise(body, i + 5);
	const struct pg_evidence *input = pg_prove_variable(t, scope, pg_evidence_context(scope)->binder);
	if (pg_evidence_rule(body) == PG_INDUCTION_ELIM)
		body = pg_prove_induction(t, pg_evidence_premise(body, 1),
			pg_evidence_premise(body, 2), input, pg_evidence_premise(body, 4),
			pg_evidence_premise(body, 0), count, branches);
	else body = pg_prove_match(t, pg_evidence_premise(body, 1),
		pg_evidence_premise(body, 2), input, pg_evidence_premise(body, 4),
		pg_evidence_premise(body, 0), count, branches);
	const struct pg_evidence *function = pg_prove_abstract(t, context, scope, body);
	if (!function) return -1;
	s->captured_input = scrutinee;
	const struct pg_evidence *input_function = function;
	while (pg_evidence_context(pg_evidence_premise(pg_evidence_premise(input_function, 0), 0)) != pg_evidence_context(scope))
		input_function = pg_evidence_premise(input_function, 1);
	if (function_signature(s, input_function)) return -1;
	s->recursive_function = function;
	return 0;
}

static void prepare_graph(struct pg_function_graph_state *s)
{
	struct pg_typing *typing = s->typing;
	const struct pg_evidence *source_function = s->source_function;
	switch (s->preparation) {
	case GRAPH_SOURCE: {
		int status = pg_function_source_advance(typing, &s->source);
		if (!status) return;
		if (status > 0) {
			const struct pg_evidence *function = s->source.function;
			if (!s->source_function) {
				s->source_function = function;
				s->outer_context = pg_evidence_premise(pg_evidence_premise(pg_evidence_premise(function, 0), 0), 0);
			}
			s->recursive_function = function;
			const struct pg_evidence *body = pg_evidence_premise(function, 1);
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			if (pg_pi_view(pg_evidence_classifier(body), &domain, &binder, &codomain)) {
				s->source = (struct pg_function_source_cursor){.function = body};
				return;
			}
		} else if (!s->source_function) goto unsupported;
		/* Source queries are graph-owned; later helper work owns a private arena. */
		s->helper = (struct helper_cursor){0};
		s->preparation = GRAPH_HEAD;
		function_signature(s, s->recursive_function);
		return;
	}
	case GRAPH_HEAD: {
		if (prepare_head(s)) return;
		enum pg_evidence_rule rule = pg_evidence_rule(s->body);
		s->cases = rule == PG_MATCH_ELIM || rule == PG_INDUCTION_ELIM;
		s->induction = rule == PG_INDUCTION_ELIM;
		s->count = 1;
		if (s->cases) {
			const struct pg_evidence *scrutinee = pg_evidence_premise(s->body, 3);
			const struct pg_term *variable = pg_reference(typing->graph, pg_evidence_context(s->argument_context)->binder);
			if ((s->head_arguments || pg_evidence_subject(scrutinee)->core != variable) &&
				capture_eliminator(s, scrutinee)) goto unsupported;
		} else {
			for (const struct graph_continuation *argument = s->head_arguments; argument; argument = argument->next)
				s->body = pg_prove_application(typing, s->body, argument->argument);
			if (!s->body) goto unsupported;
			s->head_arguments = NULL;
		}
		s->preparation = GRAPH_INPUT;
		return;
	}
	case GRAPH_INPUT:
		if (!s->cases) break;
		struct pg_typed_query *query = pg_inductive_request(typing, s->domain);
		if (await_view(s, query)) return;
		const struct pg_inductive_instance *input = pg_inductive_query_result(query);
		if (!input) goto unsupported;
		s->input = *input;
		if (s->input.formation != pg_evidence_premise(s->body, 1)) goto unsupported;
		if (s->input.indices) {
			size_t offset = pg_evidence_premise_count(s->input.parameters) + 1;
			s->index_count = pg_evidence_premise_count(s->input.indices) - offset;
			if (s->index_count > SIZE_MAX / sizeof(*s->index_arguments)) goto error;
			s->index_arguments = pg_alloc(&s->temporary, s->index_count * sizeof(*s->index_arguments));
			if (s->index_count && !s->index_arguments) goto error;
			/* The source binds generic indices immediately before the selected
			 * input. Check this telescope; never solve a fixed index backwards. */
			for (size_t i = s->index_count; i; --i) {
				if (pg_evidence_rule(s->context) != PG_CONTEXT_EXTEND) goto unsupported;
				const struct pg_term *image = pg_evidence_subject(pg_evidence_premise(s->input.indices, offset + i - 1))->core;
				if (image != pg_reference(typing->graph, pg_evidence_context(s->context)->binder)) goto unsupported;
				s->index_arguments[i - 1] = s->context;
				s->context = pg_evidence_premise(s->context, 0);
			}
		}
		s->count = pg_data_constructor_count(s->input.schema);
		break;
	case GRAPH_PARAMETERS:
		goto parameters;
	case GRAPH_READY:
		return;
	}
	s->preparation = GRAPH_PARAMETERS;
	return;
parameters:
	if (s->captured_input) source_function = s->recursive_function;
	if (s->input.indices) {
		struct pg_typed_query *query = pg_substitution_rebase_request(typing, s->context, s->input.parameters);
		if (await_view(s, query)) return;
		s->input.parameters = pg_typed_query_result(query);
		if (!s->input.parameters || !pg_inductive_motive_context_valid(typing,
			s->input.formation, s->input.parameters, s->argument_context)) goto unsupported;
		while (source_function && pg_evidence_context(source_function) != pg_evidence_context(s->context))
			source_function = pg_evidence_rule(pg_evidence_premise(source_function, 1)) == PG_LAMBDA_INTRO
				? pg_evidence_premise(source_function, 1) : NULL;
		if (!source_function) goto unsupported;
		s->recursive_function = source_function;
	}
	if (s->captured_input) {
		s->specialization = argument_substitution(s, s->context, s->captured_input);
		for (const struct graph_continuation *argument = s->head_arguments; argument; argument = argument->next) {
			if (s->specialized_arity == s->arity) goto unsupported;
			s->specialization = pg_prove_substitution_pair(typing, s->specialization,
				s->arguments[s->specialized_arity++], argument->argument);
		}
		if (!s->specialization) goto unsupported;
	}
	if (s->count > SIZE_MAX / sizeof(*s->plans)) goto error;
	s->plans = pg_alloc(&s->temporary, s->count * sizeof(*s->plans));
	if (s->count && !s->plans) goto error;
	if (graph_universe(s, &s->level)) goto unsupported;
	s->leaf_tail = &s->leaves;
	s->build_tail = &s->building;
	s->preparation = GRAPH_READY;
	return;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
}

/* Expose typed beta/quotation/sequencing wrappers one scheduled step at a
 * time. A neutral Match keeps its motive and branches; it is not a RETURN. */
static int prepare_head(struct pg_function_graph_state *s)
{
	const struct pg_evidence *left, *right, *body = NULL;
	if (s->head_arguments) {
		if (application_body(s, s->body, s->head_arguments->argument, &body)) return 1;
		if (body) {
			s->head_arguments = s->head_arguments->next;
			s->body = body;
			return 1;
		}
	}
	enum pg_evidence_rule rule = pg_evidence_rule(s->body);
	if (rule == PG_MATCH_ELIM || rule == PG_INDUCTION_ELIM) return 0;
	int view = computation_view(s, s->body, &rule, &left, &right);
	if (view > 0) return 1;
	if (view < 0) return 0;
	switch (rule) {
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM:
		s->body = left;
		return 0;
	case PG_APP_ELIM:
		if (application_body(s, left, right, &body)) return 1;
		if (!body) {
			struct graph_continuation *argument = pg_alloc(&s->temporary, sizeof(*argument));
			if (!argument) { s->status = PG_FUNCTION_GRAPH_ERROR; return 1; }
			argument->argument = right; argument->next = s->head_arguments;
			s->head_arguments = argument;
			body = left;
		}
		break;
	case PG_FOLD_ELIM:
		if (application_body(s, right, pg_prove_return_value(s->typing, left), &body)) return 1;
		break;
	case PG_FORCE_ELIM:
		view = computation_view(s, left, &rule, &left, &right);
		if (view > 0) return 1;
		if (!view && rule == PG_THUNK_INTRO) body = left;
		break;
	default: break;
	}
	if (!body) return 0;
	s->body = body;
	return 1;
}

size_t pg_function_graph_trailing_arity(const struct pg_function_graph_work *work)
{
	return work && work->state ? work->state->arity : 0;
}

int pg_function_graph_prepared(const struct pg_function_graph_work *work)
{
	return work && work->state && work->state->preparation == GRAPH_READY;
}

int pg_function_graph_source_order(struct pg_function_graph_work *work,
	size_t count, const struct pg_function_graph_order *orders)
{
	if (!work || !work->state) return -1;
	struct pg_function_graph_state *s = work->state;
	if (s->status != PG_FUNCTION_GRAPH_PENDING || !pg_function_graph_prepared(work) || s->next) return -1;
	if (!s->cases || count != s->count || (count && !orders)) return -1;
	for (size_t i = 0; i < count; ++i) {
		if (s->plans[i].computation || s->plans[i].source_order) return -1;
		if (orders[i].count > SIZE_MAX / sizeof(size_t) || (orders[i].count && !orders[i].fields)) return -1;
	}
	struct pg_function_graph_order *copy = pg_alloc(&s->temporary, count * sizeof(*copy));
	if (count && !copy) return -1;
	for (size_t i = 0; i < count; ++i) {
		size_t *fields = pg_alloc(&s->temporary, orders[i].count * sizeof(*fields));
		if (orders[i].count && !fields) return -1;
		for (size_t j = 0; j < orders[i].count; ++j) fields[j] = orders[i].fields[j];
		copy[i] = (struct pg_function_graph_order){orders[i].count, fields};
	}
	for (size_t i = 0; i < count; ++i) s->plans[i].source_order = &copy[i];
	return 0;
}

enum pg_function_graph_status pg_function_graph_advance(struct pg_function_graph_work *work, uint64_t budget)
{
	if (!work || !work->state) return PG_FUNCTION_GRAPH_ERROR;
	struct pg_function_graph_state *s = work->state;
	while (s->status == PG_FUNCTION_GRAPH_PENDING && budget--) {
		/* Resume shared work before rediscovering the enclosing call/view. */
		if (s->view) {
			if (await_view(s, s->view)) continue;
			s->view = NULL;
		}
		if (!pg_function_graph_prepared(work)) {
			prepare_graph(s);
			continue;
		}
		if (s->next == s->count) {
			/* The call plan determines constructor field universes. Build Self
			 * only after all helper graphs and nested splits have supplied them. */
			if (!s->self) {
				if (signature(s)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
				continue;
			}
			if (s->building) {
				if (case_branch(s, s->building)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
				s->building = s->building->build_next;
				continue;
			}
			size_t count = s->leaf_count;
			if (count > SIZE_MAX / sizeof(*s->results)) { s->status = PG_FUNCTION_GRAPH_ERROR; break; }
			s->results = pg_alloc(&s->temporary, count * sizeof(*s->results));
			if (count && !s->results) { s->status = PG_FUNCTION_GRAPH_ERROR; break; }
			for (const struct graph_case *leaf = s->leaves; leaf; leaf = leaf->leaf_next)
				s->results[leaf->leaf] = leaf->result;
			s->schema = pg_data_schema(s->typing,
				pg_data_signature(s->typing, s->self, s->indices), count, s->results);
			s->declaration = pg_prove_inductive_type(s->typing, s->schema);
			s->formation = s->declaration;
			if (s->specialization) {
				size_t end = pg_evidence_premise_count(s->specialization);
				for (size_t i = end - s->index_count - 1 - s->specialized_arity; s->formation && i < end; ++i)
					s->formation = pg_prove_family_application(s->typing, s->formation,
						pg_evidence_premise(s->specialization, i));
			}
			for (const struct pg_evidence *context = s->context;
				s->formation && pg_evidence_context(context) != pg_evidence_context(s->outer_context);
				context = pg_evidence_premise(context, 0))
				s->formation = pg_prove_family_abstraction(s->typing, context, s->formation);
			s->status = s->formation ? PG_FUNCTION_GRAPH_DONE : PG_FUNCTION_GRAPH_UNSUPPORTED;
			break;
		}
		if (!s->pending) {
			s->pending = &s->plans[s->next];
			if (plan_case(s, s->pending)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
			continue;
		}
		struct graph_case *plan = s->pending;
		if (!plan->output && !plan->discriminant) {
			if (plan_step(s, plan) < 0) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
			continue;
		}
		*s->build_tail = plan;
		s->build_tail = &plan->build_next;
		s->pending = plan->pending;
		for (size_t i = plan->child_count; i; --i) {
			plan->children[i - 1].pending = s->pending;
			s->pending = &plan->children[i - 1];
		}
		if (!s->pending) ++s->next;
	}
	return s->status;
}

const struct pg_evidence *pg_function_graph_formation(const struct pg_function_graph_work *work)
{
	return work && work->state && work->state->status == PG_FUNCTION_GRAPH_DONE ? work->state->formation : NULL;
}

const struct pg_evidence *pg_function_graph_declaration(const struct pg_function_graph_work *work)
{
	return work && work->state && work->state->status == PG_FUNCTION_GRAPH_DONE ? work->state->declaration : NULL;
}

const struct pg_evidence *pg_function_graph_case_input(const struct pg_function_graph_work *work)
{
	return pg_function_graph_prepared(work) ? work->state->input.formation : NULL;
}

const struct pg_evidence *pg_function_graph_dependency(const struct pg_function_graph_work *work)
{
	return work && work->state && work->state->waiting && !work->state->waiting->helper
		? work->state->waiting->helper_source : NULL;
}

int pg_function_graph_supply(struct pg_function_graph_work *work, const struct pg_function_graph_work *dependency)
{
	if (!work || !work->state || !dependency || !dependency->state) return -1;
	struct pg_function_graph_state *s = work->state;
	const struct pg_function_graph_state *d = dependency->state;
	if (!s->waiting || s->waiting->helper || s == d) return -1;
	if (s->typing != d->typing) return -1;
	if (d->witness_status != PG_FUNCTION_GRAPH_DONE) return -1;
	if (pg_evidence_subject(s->waiting->helper_source) != pg_evidence_subject(d->source_function)) return -1;
	s->waiting->helper = dependency;
	if (d->level > s->level) s->level = d->level;
	return 0;
}

int pg_function_graph_case_source(const struct pg_function_graph_work *work,
	size_t index, struct pg_function_graph_case_source *source)
{
	if (!work || !work->state || !source) return 0;
	const struct pg_function_graph_state *s = work->state;
	if (s->status != PG_FUNCTION_GRAPH_DONE) return 0;
	const struct graph_case *leaf = s->leaves;
	while (leaf && leaf->leaf != index) leaf = leaf->leaf_next;
	if (!leaf) return 0;
	*source = (struct pg_function_graph_case_source){.refined = leaf->parent != NULL};
	for (const struct graph_call *call = leaf->calls; call; call = call->previous)
		if (call->helper) { source->refined = 1; break; }
	while (leaf->parent) {
		if (leaf->parent->child_count > 1 || (!s->cases && !leaf->parent->parent)) {
			source->formation = leaf->parent->split_formation;
			source->constructor = leaf->constructor;
			return 1;
		}
		leaf = leaf->parent;
	}
	source->formation = s->input.formation;
	source->constructor = leaf->constructor;
	return 1;
}

static const struct pg_evidence *packet_type(struct pg_function_graph_state *s,
	const struct pg_evidence *map)
{
	return pg_prove_reindex(s->typing, map, s->packet);
}

static const struct pg_object *packet_constructor(struct pg_function_graph_state *s)
{
	return pg_data_constructor(pg_data_schema_layout(pg_evidence_inductive_schema(s->packet)), 0);
}

static int packet_formation(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *u = pg_prove_universe(t, s->result_context, s->level);
	const struct pg_evidence *self = pg_prove_context_extension(t, s->result_context, pg_binder(t->graph), u);
	const struct pg_evidence *output = pg_prove_context_extension(t, self, pg_binder(t->graph), projection(s, self, s->range));
	if (!output) return -1;
	const struct pg_evidence *relation = projection(s, output, s->declaration);
	relation = apply_relation(s, relation, pg_prove_substitution_projection(t, s->result_context, output),
		pg_prove_variable(t, output, pg_evidence_context(output)->binder));
	const struct pg_evidence *fields = pg_prove_context_extension(t, output, pg_binder(t->graph), relation);
	const struct pg_evidence *result = pg_prove_substitution_projection(t, self, fields);
	const struct pg_data_schema *schema = pg_data_schema(t, pg_data_signature(t, self, self), 1, &result);
	s->packet = pg_prove_inductive_type(t, schema);
	return s->packet ? 0 : -1;
}

static const struct pg_evidence *return_packet(struct pg_function_graph_state *s, size_t index,
	const struct pg_evidence *context, size_t count, const struct pg_evidence *const *fields)
{
	struct pg_typing *t = s->typing;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->schema), index);
	const struct pg_evidence *graph = pg_prove_constructor(t, s->declaration, constructor,
		pg_prove_substitution_projection(t, s->context, context), count, fields);
	if (!graph) return NULL;
	const struct pg_evidence *result = pg_data_result(t, s->schema, constructor, pg_evidence_premise(graph, 3));
	size_t n = pg_evidence_premise_count(result);
	if (n < s->arity + 2) return NULL;
	const struct pg_evidence *input = pg_evidence_premise(result, n - s->arity - 2), *output = pg_evidence_premise(result, n - 1);
	const struct pg_evidence *values[] = {output, graph};
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, s->arity * sizeof(*arguments));
	if (s->arity && !arguments) return NULL;
	for (size_t i = 0; i < s->arity; ++i) arguments[i] = pg_evidence_premise(result, n - s->arity - 1 + i);
	const struct pg_evidence *packet = pg_prove_constructor(t, s->packet, packet_constructor(s),
		input_substitution(s, context, input, arguments), 2, values);
	return pg_prove_return_contract(t, s->totality, packet);
}

struct packet_frame {
	const struct pg_evidence *before, *bound, *parameters, *input, *value, *fields, *target, *formation;
	const struct pg_object *constructor;
};

struct witness_branch {
	const struct graph_case *plan;
	struct witness_branch *parent;
	const struct pg_evidence *source_map, *schema_map, *context, *target;
	const struct pg_evidence **hypotheses, **values, **refinements, **branches;
	struct packet_frame *frames;
	size_t next;
	int prepared;
};

static int witness_calls(struct pg_function_graph_state *s, struct witness_branch *work)
{
	struct pg_typing *t = s->typing;
	const struct graph_case *plan = work->plan;
	size_t calls = plan->call_count - plan->first_call;
	if (calls > SIZE_MAX / (2 * sizeof(*work->values)) || calls > SIZE_MAX / sizeof(*work->frames)) return -1;
	work->values = pg_alloc(&s->temporary, 2 * calls * sizeof(*work->values));
	work->frames = pg_alloc(&s->temporary, calls * sizeof(*work->frames));
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, s->arity * sizeof(*arguments));
	if ((calls && (!work->values || !work->frames)) || (s->arity && !arguments)) return -1;
	const struct pg_evidence *context = pg_evidence_premise(work->source_map, 1);
	work->target = pg_prove_computation_type(t, s->totality,
		pg_effect_row(t->graph, 0, NULL), packet_type(s,
			pg_prove_substitution_compose(t, plan->base_input, work->schema_map)));
	if (!work->target) return -1;
	for (size_t next = 0; next < calls; ++next) {
		const struct graph_call_layout *entry = &plan->executed[next];
		const struct graph_call *call = entry->call;
		struct packet_frame *f = &work->frames[next];
		f->before = context;
		f->target = projection(s, context, work->target);
		if (call->helper) f->input = helper_application(s, call, work->source_map, 1);
		else {
			if (call->hypothesis >= plan->hypothesis_count) return -1;
			f->input = pg_prove_force(t, projection(s, context, work->hypotheses[call->hypothesis]));
			for (size_t i = 0; f->input && i < call->field_arity; ++i)
				f->input = pg_prove_application(t, f->input, map_value(s, work->source_map, call->field_arguments[i]));
			for (size_t i = 0; i < s->arity; ++i) {
				arguments[i] = map_value(s, work->source_map, call->arguments[i]);
				f->input = pg_prove_application(t, f->input, arguments[i]);
			}
		}
		const struct pg_evidence *packet = pg_prove_return_content(t, pg_prove_classifier(t, context, f->input));
		f->bound = pg_prove_context_extension(t, context, pg_binder(t->graph),
			packet);
		if (!f->bound || !f->input || !f->target) return -1;
		f->value = pg_prove_variable(t, f->bound, pg_evidence_context(f->bound)->binder);
		struct pg_inductive_instance instance;
		if (!pg_inductive_instance(t, projection(s, f->bound, packet), &instance)) return -1;
		f->formation = instance.formation;
		f->constructor = pg_data_constructor(pg_data_schema_layout(instance.schema), 0);
		f->parameters = instance.parameters;
		f->fields = pg_prove_constructor_scope(t, f->formation, f->constructor, f->parameters);
		if (!f->fields) return -1;
		context = pg_evidence_premise(f->fields, 1);
		size_t start = pg_evidence_premise_count(f->parameters) + 1;
		work->values[2 * entry->slot] = pg_evidence_premise(f->fields, start);
		work->values[2 * entry->slot + 1] = pg_evidence_premise(f->fields, start + 1);
		work->source_map = pg_prove_substitution_compose(t, work->source_map,
			pg_prove_substitution_projection(t, pg_evidence_premise(work->source_map, 1), context));
		work->source_map = pg_prove_substitution_pair(t, work->source_map, call->result_context,
			pg_evidence_premise(f->fields, start));
		if (!work->source_map) return -1;
	}
	for (size_t i = 0; i < 2 * calls; ++i) work->values[i] = projection(s, context, work->values[i]);
	work->schema_map = pg_prove_substitution_compose(t, work->schema_map,
		pg_prove_substitution_projection(t, pg_evidence_premise(work->schema_map, 1), context));
	work->schema_map = pg_prove_substitution_extend(t, work->schema_map,
		pg_evidence_premise(plan->schema_map, 1), 2 * calls, work->values);
	work->context = context;
	if (plan->child_count > SIZE_MAX / sizeof(*work->branches)) return -1;
	work->branches = pg_alloc(&s->temporary, plan->child_count * sizeof(*work->branches));
	work->refinements = pg_alloc(&s->temporary, plan->child_count * sizeof(*work->refinements));
	if (plan->child_count && (!work->branches || !work->refinements)) return -1;
	return work->schema_map ? 0 : -1;
}

/* The source, schema and witness share one branch tree. Each edge factors a
 * checked constructor refinement; it never identifies an unknown value with
 * a constructor in the unrefined parent context. */
static const struct pg_evidence *witness_tree(struct pg_function_graph_state *s, struct witness_branch *work)
{
	struct pg_typing *t = s->typing;
	while (work) {
		const struct graph_case *plan = work->plan;
		if (!work->prepared) {
			if (witness_calls(s, work)) return NULL;
			work->prepared = 1;
		}
		if (work->next < plan->child_count) {
			const struct graph_case *branch = &plan->children[work->next];
			const struct pg_evidence *refinement = pg_prove_constructor_refinement(t,
				work->context, map_value(s, work->source_map, plan->discriminant), branch->constructor);
			if (!refinement) return NULL;
			work->refinements[work->next] = refinement;
			struct witness_branch *child = pg_alloc(&s->temporary, sizeof(*child));
			if (!child) return NULL;
			*child = (struct witness_branch){.plan = branch, .parent = work};
			child->source_map = pg_prove_refinement_factor(t, branch->refinement,
				pg_prove_substitution_compose(t, work->source_map, refinement),
				pg_evidence_subject(plan->discriminant)->core->as.reference);
			const struct pg_evidence *schema_value = map_value(s, plan->schema_map, plan->discriminant);
			if (!schema_value) return NULL;
			child->schema_map = pg_prove_refinement_factor(t, branch->schema_refinement,
				pg_prove_substitution_compose(t, work->schema_map, refinement),
				pg_evidence_subject(schema_value)->core->as.reference);
			if (!child->source_map || !child->schema_map) return NULL;
			child->hypotheses = pg_alloc(&s->temporary, plan->hypothesis_count * sizeof(*child->hypotheses));
			if (plan->hypothesis_count && !child->hypotheses) return NULL;
			for (size_t i = 0; i < plan->hypothesis_count; ++i) {
				child->hypotheses[i] = map_value(s, refinement, work->hypotheses[i]);
				if (!child->hypotheses[i]) return NULL;
			}
			work = child;
			continue;
		}
		const struct pg_evidence *body;
		if (plan->discriminant) {
			body = pg_prove_refined_match(t, work->context,
				map_value(s, work->source_map, plan->discriminant), projection(s, work->context, work->target),
				plan->child_count, work->refinements, work->branches);
		} else {
			size_t count;
			if (pg_context_extension_size(pg_evidence_context(pg_evidence_premise(work->schema_map, 0)),
				pg_evidence_context(s->self), &count)) return NULL;
			const struct pg_evidence **values = pg_alloc(&s->temporary, count * sizeof(*values));
			if (count && !values) return NULL;
			size_t start = pg_evidence_premise_count(work->schema_map) - count;
			for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(work->schema_map, start + i);
			body = return_packet(s, plan->leaf, work->context, count, values);
		}
		for (size_t i = plan->call_count - plan->first_call; body && i; --i) {
			const struct packet_frame *f = &work->frames[i - 1];
			body = pg_prove_abstract(t, f->bound, pg_evidence_premise(f->fields, 1), body);
			const struct pg_evidence *mc = pg_prove_inductive_motive_context(t, f->formation, f->parameters, pg_binder(t->graph));
			body = pg_prove_match(t, f->formation, f->parameters, f->value, mc,
				projection(s, mc, f->target), 1, &body);
			body = pg_prove_abstract(t, f->before, f->bound, body);
			body = pg_prove_fold(t, f->input, body);
		}
		if (!body || !work->parent) return body;
		work = work->parent;
		work->branches[work->next++] = body;
	}
	return NULL;
}

static const struct pg_evidence *witness_case(struct pg_function_graph_state *s, size_t index)
{
	struct pg_typing *t = s->typing;
	const struct graph_case *plan = &s->plans[index];
	if (!s->cases) {
		const struct pg_evidence *context = s->result_context;
		const struct pg_evidence *map = pg_prove_substitution_pair(t,
			pg_prove_substitution_projection(t, s->context, context), s->self, projection(s, context, s->declaration));
		size_t count = s->arity + 1;
		const struct pg_evidence **values = pg_alloc(&s->temporary, count * sizeof(*values));
		if (!values) return NULL;
		values[0] = pg_prove_variable(t, context, pg_evidence_context(s->argument_context)->binder);
		for (size_t i = 0; i < s->arity; ++i)
			values[i + 1] = pg_prove_variable(t, context, pg_evidence_context(s->arguments[i])->binder);
		map = pg_prove_substitution_extend(t, map, plan->schema_base, count, values);
		if (!map) return NULL;
		struct witness_branch work = {.plan = plan, .schema_map = map,
			.source_map = pg_prove_substitution_projection(t, context, context)};
		return pg_prove_abstract(t, s->argument_context, context, witness_tree(s, &work));
	}
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->input.schema), index);
	const struct pg_evidence *parameters = parameters_at(s, s->argument_context);
	const struct pg_evidence *map = s->induction
		? pg_prove_induction_scope(t, s->input.formation, constructor, parameters, s->motive_context, s->motive)
		: pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!map) return NULL;
	const struct pg_evidence *base = pg_evidence_premise(map, 1), *context = base;
	const struct pg_evidence *original = pg_data_schema_fields(s->input.schema, constructor);
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(original),
		pg_evidence_context(pg_data_schema_parameters(s->input.schema)), &count)) return NULL;
	size_t offset = pg_evidence_premise_count(parameters) + 1;
	size_t scope_count;
	if (pg_context_extension_size(pg_evidence_context(base), pg_evidence_context(s->argument_context), &scope_count)) return NULL;
	if (scope_count < count) return NULL;
	size_t ih_count = scope_count - count;
	if (ih_count != plan->hypothesis_count || count != plan->field_count) return NULL;
	if (count > SIZE_MAX / sizeof(void *) || s->arity > SIZE_MAX / sizeof(void *) - count) return NULL;
	const struct pg_evidence **values = pg_alloc(&s->temporary, (count + s->arity) * sizeof(*values));
	const struct pg_evidence **hypotheses = pg_alloc(&s->temporary, ih_count * sizeof(*hypotheses));
	if ((count + s->arity && !values) || (ih_count && !hypotheses)) return NULL;
	const struct pg_context *scope = pg_evidence_context(base);
	for (size_t i = ih_count; i; --i, scope = scope->parent)
		hypotheses[i - 1] = pg_prove_variable(t, base, scope->binder);
	for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(map, offset + i);
	const struct pg_evidence *argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, base), count, values);
	if (!argument) return NULL;
	const struct pg_evidence *source_map = case_source_map(s, plan, context, argument, values, values + count);
	if (!source_map) return NULL;
	context = pg_evidence_premise(source_map, 1);
	const struct pg_evidence *arguments_context = context;
	for (size_t i = 0; i < count + s->arity; ++i) values[i] = projection(s, context, values[i]);
	const struct pg_evidence *schema_map = pg_prove_substitution_pair(t,
		pg_prove_substitution_projection(t, s->context, context), s->self, projection(s, context, s->declaration));
	schema_map = pg_prove_substitution_extend(t, schema_map, plan->schema_base, count + s->arity, values);
	if (!schema_map) return NULL;
	struct witness_branch work = {.plan = plan, .source_map = source_map, .schema_map = schema_map, .hypotheses = hypotheses};
	const struct pg_evidence *body = witness_tree(s, &work);
	body = pg_prove_abstract(t, base, arguments_context, body);
	return pg_prove_abstract(t, s->argument_context, base, body);
}

enum pg_function_graph_status pg_function_graph_witness_advance(struct pg_function_graph_work *work, uint64_t budget)
{
	if (!work || !work->state) return PG_FUNCTION_GRAPH_ERROR;
	struct pg_function_graph_state *s = work->state;
	if (s->status != PG_FUNCTION_GRAPH_DONE) {
		enum pg_function_graph_status status = pg_function_graph_advance(work, budget);
		return status == PG_FUNCTION_GRAPH_DONE ? PG_FUNCTION_GRAPH_PENDING : status;
	}
	while (s->witness_status == PG_FUNCTION_GRAPH_PENDING && budget--) {
		if (!s->packet) {
			if (packet_formation(s)) goto unsupported;
			s->witness_branches = pg_alloc(&s->temporary, s->count * sizeof(*s->witness_branches));
			if (s->count && !s->witness_branches) { s->witness_status = PG_FUNCTION_GRAPH_ERROR; break; }
			if (s->cases) {
				s->motive_context = pg_prove_inductive_motive_context(s->typing, s->input.formation,
					parameters_at(s, s->argument_context), pg_binder(s->typing->graph));
				if (!s->motive_context) goto unsupported;
				const struct pg_evidence *z = pg_prove_variable(s->typing, s->motive_context,
					pg_evidence_context(s->motive_context)->binder);
				const struct pg_evidence *map = argument_substitution(s, s->motive_context, z);
				for (size_t i = 0; map && i < s->arity; ++i)
					map = pg_prove_substitution_lift(s->typing, map, s->arguments[i], pg_binder(s->typing->graph));
				if (!map) goto unsupported;
				const struct pg_evidence *context = pg_evidence_premise(map, 1);
				s->motive = pg_prove_computation_type(s->typing, s->totality,
					pg_effect_row(s->typing->graph, 0, NULL), packet_type(s, map));
				for (size_t i = s->arity; s->motive && i; --i, context = pg_evidence_premise(context, 0))
					s->motive = pg_prove_pi(s->typing, context, s->motive);
				if (!s->motive) goto unsupported;
			}
			continue;
		}
		const struct pg_evidence *argument = pg_prove_variable(s->typing, s->argument_context,
			pg_evidence_context(s->argument_context)->binder);
		if (s->cases && s->witness_next < s->count) {
			s->witness_branches[s->witness_next] = witness_case(s, s->witness_next);
			if (!s->witness_branches[s->witness_next]) goto unsupported;
			++s->witness_next;
			continue;
		}
		const struct pg_evidence *body;
		if (!s->cases) {
			body = witness_case(s, 0);
		}
		else if (s->induction) body = pg_prove_induction(s->typing, s->input.formation,
			parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		else body = pg_prove_match(s->typing, s->input.formation,
			parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		const struct pg_evidence *context = s->argument_context;
		if (s->specialization) {
			body = pg_prove_abstract(s->typing, s->context, context, body);
			size_t end = pg_evidence_premise_count(s->specialization);
			for (size_t i = end - s->index_count - 1 - s->specialized_arity; body && i < end; ++i)
				body = pg_prove_application(s->typing, body, pg_evidence_premise(s->specialization, i));
			context = s->context;
		}
		s->witness = pg_prove_abstract(s->typing, s->outer_context, context, body);
		if (!s->witness) goto unsupported;
		s->witness_status = PG_FUNCTION_GRAPH_DONE;
	}
	return s->witness_status;
unsupported:
	s->witness_status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return s->witness_status;
}

const struct pg_evidence *pg_function_graph_witness(const struct pg_function_graph_work *work)
{
	return work && work->state && work->state->witness_status == PG_FUNCTION_GRAPH_DONE ? work->state->witness : NULL;
}

const struct pg_evidence *pg_function_graph_packet(const struct pg_function_graph_work *work)
{
	return work && work->state ? work->state->packet : NULL;
}

void pg_function_graph_destroy(struct pg_function_graph_work *work)
{
	if (!work || !work->state) return;
	if (work->state->preparation != GRAPH_SOURCE) pg_graph_destroy(&work->state->helper.temporary);
	pg_graph_destroy(&work->state->temporary);
	free(work->state);
	work->state = NULL;
}
