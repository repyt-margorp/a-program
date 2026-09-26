#include "function_graph_internal.h"
#include "computation.h"

#include <stdlib.h>

const struct pg_evidence *pg_function_plan_projection(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	return pg_prove_projection(s->typing, context, proof);
}

const struct pg_evidence *pg_function_plan_argument_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument)
{
	if (s->index_count)
		return pg_prove_inductive_motive_substitution(s->typing,
			s->input.formation, s->input.parameters, s->argument_context,
			context, pg_function_plan_projection(s, context, argument));
	return pg_prove_substitution_pair(s->typing,
		pg_prove_substitution_projection(s->typing, s->context, context), s->argument_context,
		pg_function_plan_projection(s, context, argument));
}

const struct pg_evidence *pg_function_plan_input_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument,
	const struct pg_evidence *const *arguments)
{
	const struct pg_evidence *map = pg_function_plan_argument_substitution(s, context, argument);
	for (size_t i = 0; map && i < s->arity; ++i)
		map = pg_prove_substitution_pair(s->typing, map, s->arguments[i], pg_function_plan_projection(s, context, arguments[i]));
	return map;
}

const struct pg_evidence *pg_function_plan_map_value(struct pg_function_graph_state *s,
	const struct pg_evidence *map, const struct pg_evidence *value)
{
	if (!map) return NULL;
	return pg_prove_reindex(s->typing, map, pg_function_plan_projection(s, pg_evidence_premise(map, 0), value));
}

const struct pg_evidence *pg_function_plan_apply_relation(struct pg_function_graph_state *s,
	const struct pg_evidence *relation, const struct pg_evidence *map,
	const struct pg_evidence *output)
{
	if (!map) return NULL;
	size_t count = pg_evidence_context_map(map)->count;
	size_t inputs = s->index_count + s->arity + 1;
	if (count < inputs) return NULL;
	for (size_t i = count - inputs; relation && i < count; ++i)
		relation = pg_prove_family_application(s->typing, relation, pg_substitution_image_at(s->typing, map, i));
	return pg_prove_family_application(s->typing, relation, output);
}

const struct pg_evidence *pg_function_plan_parameters_at(struct pg_function_graph_state *s,
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

static int scoped_input(struct pg_function_graph_state *s,
	const struct pg_evidence *source, size_t index, const struct pg_evidence **result)
{
	const struct pg_occurrence *input = pg_occurrence_scoped_input(pg_evidence_subject(source), index);
	if (input) {
		*result = pg_prove_structural_subject(s->typing, input);
		return *result ? 0 : -1;
	}
	struct pg_typed_query *query = pg_typed_input_request(s->typing, source, index);
	if (await_view(s, query)) return 1;
	*result = pg_typed_query_result(query);
	return *result ? 0 : -1;
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
		plan->computation = pg_function_plan_projection(s, plan->context, s->body);
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
	return 0;
}

static int plan_scope(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	size_t i = plan->next_scope;
	if (!plan->scopes[i]) {
		const struct pg_evidence *pi = pg_prove_classifier(t, plan->context, plan->computation);
		const struct pg_evidence *domain = pg_prove_pi_domain(t, pi);
		plan->scopes[i] = pg_prove_context_extension(t, plan->context, pg_binder(t->graph), domain);
	}
	const struct pg_evidence *context = plan->scopes[i];
	if (!context) return -1;
	const struct pg_evidence *variable = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
	const struct pg_evidence *body;
	if (application_body(s, pg_function_plan_projection(s, context, plan->computation), variable, &body)) return 0;
	if (!body) return -1;
	plan->context = context;
	plan->computation = body;
	++plan->next_scope;
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
		child->computation = pg_prove_elimination_reindex(t, map, elimination);
		if (!child->origin_map || !child->computation) return -1;
		struct graph_continuation **tail = &child->continuations;
		for (const struct graph_continuation *old = plan->continuations; old; old = old->next) {
			struct graph_continuation *frame = pg_alloc(&s->temporary, sizeof(*frame));
			if (!frame) return -1;
			frame->function = old->function ? pg_function_plan_map_value(s, map, old->function) : NULL;
			frame->argument = old->argument ? pg_function_plan_map_value(s, map, old->argument) : NULL;
			if ((old->function && !frame->function) || (old->argument && !frame->argument)) return -1;
			*tail = frame; tail = &frame->next;
		}
	}
	return 0;
}

static int plan_result(struct graph_case *plan, const struct pg_evidence *value)
{
	plan->output = value;
	return value ? 0 : -1;
}

static size_t hypothesis_position(struct pg_function_graph_state *s,
	const struct graph_case *plan, const struct pg_term *core)
{
	for (size_t i = 0; i < plan->hypothesis_count; ++i) {
		const struct pg_evidence *scope = plan->scopes[plan->field_count + i];
		const struct pg_evidence *hypothesis = pg_prove_variable(s->typing, scope, pg_evidence_context(scope)->binder);
		if (plan->origin_map) hypothesis = pg_function_plan_map_value(s, plan->origin_map, hypothesis);
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
 * that services public @f requests, not generated afresh at each call.
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
			if (h->environment) item->argument = pg_function_plan_map_value(s, h->environment, item->argument);
			item->next = h->arguments; h->arguments = item; ++h->count;
			h->function = pg_evidence_premise(h->function, 0);
			if (h->environment) h->function = pg_function_plan_map_value(s, h->environment, h->function);
			continue;
		}
		if (rule == PG_FORCE_ELIM || rule == PG_THUNK_COMPUTATION) ++h->forces;
		else if (rule == PG_THUNK_INTRO && h->forces) --h->forces;
		else { h->phase = HELPER_SOURCE; break; }
		h->function = pg_evidence_premise(h->function, 0);
		if (h->environment) h->function = pg_function_plan_map_value(s, h->environment, h->function);
	}
	if (!h->count) goto done;
	if (h->phase == HELPER_SOURCE && h->forces == 1 && pg_evidence_rule(h->function) == PG_VARIABLE) {
		if (!h->source.function) {
			const struct pg_evidence *current = h->environment ? pg_function_plan_map_value(s, h->environment, h->function) : h->function;
			if (!current) goto done;
			if (hypothesis_position(s, plan, pg_evidence_subject(current)->core) != plan->hypothesis_count) goto done;
			h->source.function = current;
		}
		int status = pg_function_source_advance(s->typing, &h->source);
		if (!status) { result = 2; goto done; }
		const struct pg_evidence *concrete = status > 0 ? h->source.function : NULL;
		if (concrete) {
			const struct pg_evidence *context = pg_evidence_for_context(s->typing, pg_evidence_context(concrete));
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
			if (pg_evidence_rule(h->body) == PG_LAMBDA_INTRO) {
				int input = scoped_input(s, h->body, 0, &h->body);
				if (input) { result = input > 0 ? 2 : 0; goto done; }
			} else if (pg_evidence_rule(h->body) == PG_APP_ELIM) h->body = pg_evidence_premise(h->body, 0);
			else if (pg_evidence_rule(h->body) == PG_FORCE_ELIM || pg_evidence_rule(h->body) == PG_THUNK_COMPUTATION ||
				pg_evidence_rule(h->body) == PG_THUNK_INTRO) h->body = pg_evidence_premise(h->body, 0);
			else break;
		}
		switch (pg_evidence_rule(h->body)) {
		case PG_MATCH_ELIM: {
			/* A split may refine local graph inputs, but must not replace
			 * the fixed parameter telescope. Retain this call when its
			 * discriminant belongs to that telescope. */
			h->body = h->environment ? pg_function_plan_map_value(s, h->environment, h->function) : h->function;
			h->body = pg_function_plan_projection(s, plan->context, h->body);
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
			if (application_body(s, h->body, pg_function_plan_projection(s, plan->context, a->argument), &h->body)) {
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
	return plan_result(plan, pg_prove_variable(t, plan->context, pg_evidence_context(plan->context)->binder));
}

/* Symbolically expose sequencing using retained typing evidence. A recursive
 * call creates an output variable; it is not asserted equal by conversion.
 * The companion witness will supply this variable from the corresponding IH. */
static int plan_step(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (plan->output) {
		const struct graph_continuation *frame = plan->continuations;
		if (!frame || !frame->function) return -1;
		const struct pg_evidence *body;
		if (application_body(s, pg_function_plan_projection(s, plan->context, frame->function),
			pg_function_plan_projection(s, plan->context, plan->output), &body)) return 0;
		if (!body) return -1;
		plan->computation = body;
		plan->continuations = frame->next;
		plan->output = NULL;
		return 0;
	}
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
		return plan_result(plan, value);
	}
	if (plan->continuations && plan->continuations->argument) {
		/* Sequencing can expose a callable with its arguments still on the
		 * continuation stack. Preserve that typed call before beta exposure. */
		const struct pg_evidence *call = plan->computation;
		struct graph_continuation *rest = plan->continuations;
		while (call && rest && rest->argument) {
			call = pg_prove_application(t, call, pg_function_plan_projection(s, plan->context, rest->argument));
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
		const struct pg_evidence *body;
		if (application_body(s, plan->computation,
			pg_function_plan_projection(s, plan->context, plan->continuations->argument), &body)) return 0;
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
		const struct pg_evidence *body;
		if (application_body(s, left, right, &body)) return 0;
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
		if (plan->origin_map) call->child = pg_function_plan_map_value(s, plan->origin_map, call->child);
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
			const struct pg_evidence *child_call = pg_prove_force(t, pg_function_plan_projection(s, plan->context, call->child));
			for (size_t j = 0; child_call && j < call->field_arity; ++j) {
				struct graph_continuation *frame = plan->continuations;
				if (!frame || !frame->argument) return -1;
				call->field_arguments[j] = frame->argument;
				const struct pg_evidence *argument = pg_function_plan_projection(s, plan->context, frame->argument);
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
			plan->computation = pg_prove_application(t, plan->computation, pg_function_plan_projection(s, plan->context, call->arguments[j]));
			if (!plan->computation) return -1;
			plan->continuations = frame->next;
		}
		return record_call(s, plan, call);
	}
	case PG_RETURN_INTRO: value = left; break;
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: {
		struct pg_typed_query *query = pg_elimination_body_request(t, left);
		if (await_view(s, query)) return 0;
		const struct pg_evidence *body = pg_typed_query_result(query);
		if (!body) {
			if (pg_evidence_rule(left) == PG_INDUCTION_ELIM) goto normalize;
			return split_case(s, plan, left);
		}
		plan->computation = body;
		return 0;
	}
	default: return -1;
	}
	return plan_result(plan, value);
normalize:
	/* Normalize the saturated call, retaining outer sequencing. A neutral
	 * result needs the checked TOTAL/empty-row pg_function_plan_projection above. */
	while (plan->continuations && plan->continuations->argument) {
		plan->computation = pg_prove_application(t, plan->computation,
			pg_function_plan_projection(s, plan->context, plan->continuations->argument));
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
	const struct pg_evidence *input = pg_function_plan_argument_substitution(s, scope, child);
	const struct pg_evidence *call = pg_function_plan_projection(s, scope, s->recursive_function);
	for (size_t j = 0; call && j < s->index_count; ++j)
		call = pg_prove_application(t, call, pg_substitution_image(t, input,
			pg_evidence_context(s->index_arguments[j])->binder));
	call = pg_prove_application(t, call, child);
	call = pg_prove_abstract(t, context, scope, call);
	return pg_prove_thunk(t, call);
}

/* Schema and witness use the same source scope: fields, original IH functions,
 * then remaining Pi arguments. Only the destination's supplied fields differ. */
const struct pg_evidence *pg_function_plan_case_source_map(struct pg_function_graph_state *s,
	const struct graph_case *plan, const struct pg_evidence *context,
	const struct pg_evidence *argument, const struct pg_evidence *const *fields,
	const struct pg_evidence **arguments)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *map = pg_function_plan_argument_substitution(s, context, argument);
	size_t count = plan->field_count;
	for (size_t i = 0; map && i < count; ++i)
		map = pg_prove_substitution_pair(t, map, plan->scopes[i], fields[i]);
	for (size_t i = 0; map && i < plan->hypothesis_count; ++i) {
		const struct pg_evidence *field = fields[plan->hypothesis_fields[i]];
		map = pg_prove_substitution_pair(t, map, plan->scopes[count + i], original_hypothesis(s, context, field));
	}
	for (size_t i = 0; map && i < s->arity; ++i) {
		const struct pg_evidence *source = plan->scopes[count + plan->hypothesis_count + i];
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), pg_function_plan_map_value(s, map, pg_evidence_premise(source, 1)));
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
		const struct pg_evidence *value = pg_function_plan_map_value(s, parent->schema_map, parent->discriminant);
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
	const struct pg_evidence *parameters = pg_function_plan_parameters_at(s, s->self);
	const struct pg_evidence *fields = pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!fields) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(fields, 1);
	size_t offset = pg_evidence_context_map(parameters)->count + 1;
	const struct pg_evidence *const *images = pg_substitution_images(t, fields, &s->temporary);
	if (!images) return NULL;
	const struct pg_evidence *const *values = images + offset;
	const struct pg_evidence *argument = pg_prove_constructor_instance(t, s->input.formation, constructor,
		pg_function_plan_parameters_at(s, context), fields);
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, s->arity * sizeof(*arguments));
	if (s->arity && !arguments) return NULL;
	const struct pg_evidence *map = pg_function_plan_case_source_map(s, plan, context, argument, values, arguments);
	if (!map) return NULL;
	plan->base_input = pg_function_plan_input_substitution(s, pg_evidence_premise(map, 1), argument, arguments);
	return map;
}

const struct pg_evidence *pg_function_plan_helper_function(struct pg_function_graph_state *s,
	const struct graph_call *call, const struct pg_evidence *map,
	const struct pg_evidence *proof)
{
	if (call->helper_environment) proof = pg_prove_reindex(s->typing, call->helper_environment, proof);
	return pg_function_plan_map_value(s, map, proof);
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
	for (size_t i = 0; i < call->helper_arity; ++i) arguments[i] = pg_function_plan_map_value(s, map, call->helper_arguments[i]);
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
	struct case_cursor *cursor = s->branch;
	if (!plan->schema_base) {
		*cursor = (struct case_cursor){0};
		if (order_calls(s, plan)) return -1;
		cursor->map = case_base(s, plan);
		if (!cursor->map || !plan->base_input) return -1;
		cursor->context = pg_evidence_premise(cursor->map, 1);
		plan->schema_base = cursor->context;
		cursor->arguments = pg_alloc(&s->temporary, s->arity * sizeof(*cursor->arguments));
		if (s->arity && !cursor->arguments) return -1;
	}
	size_t calls = plan->call_count - plan->first_call;
	const struct pg_evidence *map = cursor->map, *context = cursor->context;
	for (; cursor->slot < calls; ++cursor->slot) {
		struct graph_call_layout *entry = plan->ordered[cursor->slot];
		const struct graph_call *call = entry->call;
		const struct pg_evidence *input = NULL, *result_type;
		if (call->helper) {
			if (!cursor->relation) cursor->relation = pg_function_plan_helper_function(s, call, map, pg_function_graph_formation(call->helper));
			if (!cursor->relation) return -1;
			for (; cursor->argument < call->helper_arity; ++cursor->argument) {
				const struct pg_evidence *argument = pg_function_plan_map_value(s, map, call->helper_arguments[cursor->argument]);
				const struct pg_evidence *body;
				if (application_body(s, cursor->relation, argument, &body)) return 1;
				cursor->relation = body ? body : pg_prove_family_application(t, cursor->relation, argument);
				if (!cursor->relation) return -1;
			}
			result_type = helper_result_type(s, call, map);
		} else {
			for (size_t i = 0; i < s->arity; ++i) cursor->arguments[i] = pg_function_plan_map_value(s, map, call->arguments[i]);
			input = pg_function_plan_input_substitution(s, context, pg_function_plan_map_value(s, map, call->child), cursor->arguments);
			result_type = pg_prove_reindex(t, input, s->range);
		}
		const struct pg_evidence *before = context;
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), pg_function_plan_projection(s, context, result_type));
		if (!context) return -1;
		const struct pg_evidence *output = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		const struct pg_evidence *relation;
		if (call->helper) relation = pg_prove_family_application(t,
			pg_function_plan_projection(s, context, cursor->relation), output);
		else {
			input = pg_prove_substitution_compose(t, input, pg_prove_substitution_projection(t, before, context));
			relation = pg_function_plan_apply_relation(s, pg_prove_variable(t, context, pg_evidence_context(s->self)->binder), input, output);
		}
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), relation);
		if (!context) return -1;
		entry->layout_output = output;
		while (cursor->mapped < calls && plan->executed[cursor->mapped].layout_output) {
			const struct graph_call_layout *ready = &plan->executed[cursor->mapped];
			map = pg_prove_substitution_compose(t, map,
				pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context));
			map = pg_prove_substitution_pair(t, map, ready->call->result_context, pg_function_plan_projection(s, context, ready->layout_output));
			if (!map) return -1;
			++cursor->mapped;
		}
		cursor->map = map;
		cursor->context = context;
		cursor->relation = NULL;
		cursor->argument = 0;
	}
	if (cursor->mapped != calls) return -1;
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
	size_t inputs = pg_evidence_context_map(plan->schema_input)->count;
	for (size_t i = 0; i < arity - 1; ++i)
		values[i] = pg_substitution_image_at(t, plan->schema_input, inputs - arity + 1 + i);
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
		/* Preserve the selected type and context action. Scoped-input queries
		 * already transport the body; do not rebuild a second Lambda. */
		source->function = pg_prove_reindex(typing, environment, function);
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
	const struct pg_evidence *body, *result;
	int status = scoped_input(s, function, 0, &body);
	if (status > 0) return 1;
	if (status < 0) goto unsupported;
	s->context = pg_evidence_for_context(typing, pg_evidence_context(function));
	const struct pg_evidence *pi = pg_prove_classifier(typing, s->context, function);
	status = scoped_input(s, pi, 1, &result);
	if (status > 0) return 1;
	if (status < 0) goto unsupported;
	const struct pg_context *scope = pg_evidence_context(body);
	if (!scope || scope->judgement != PG_JUDGEMENT_VALUE) goto unsupported;
	s->domain = pg_prove_pi_domain(typing, pi);
	/* Scope admission does not select a Universe bound. The retained Pi
	 * domain does, including when another formation admitted this Context. */
	s->argument_context = pg_prove_context_extension(typing, s->context, scope->binder, s->domain);
	if (!s->argument_context || pg_evidence_context(s->argument_context) != scope) goto unsupported;
	if (pg_evidence_context(result) != scope) {
		/* Independent capture-avoiding actions may allocate different Pi
		 * and Lambda binders. Align only this bound variable, not free names. */
		const struct pg_context *codomain_scope = pg_evidence_context(result);
		if (!codomain_scope || codomain_scope->parent != scope->parent) goto unsupported;
		const struct pg_evidence *map = pg_prove_substitution_pair(typing,
			pg_prove_substitution_projection(typing, s->context, s->argument_context),
			pg_evidence_for_context(typing, codomain_scope),
			pg_prove_variable(typing, s->argument_context, scope->binder));
		result = pg_prove_reindex(typing, map, result);
		if (!result) goto unsupported;
	}
	s->recursive_function = function;
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
		result = pg_prove_pi_codomain(typing, pg_function_plan_projection(s, s->result_context, result),
			pg_prove_variable(typing, s->result_context, pg_evidence_context(s->result_context)->binder));
		if (!result) goto unsupported;
	}
	const struct pg_term *range;
	if (!result || !pg_pure_computation_type_view(pg_evidence_subject(result)->core, &s->totality, &range)) goto unsupported;
	s->range = pg_prove_return_content(typing, result);
	if (!s->range) goto unsupported;
	s->body = body;
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
	const struct pg_evidence *const *branches = pg_evidence_premises(body) + 5;
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
	for (;;) {
		const struct pg_evidence *input;
		if (scoped_input(s, input_function, 0, &input)) return -1;
		if (pg_evidence_context(input) == pg_evidence_context(scope)) break;
		input_function = input;
	}
	if (function_signature(s, input_function)) return -1;
	s->recursive_function = function;
	return 0;
}

static void prepare_graph(struct pg_function_graph_state *s)
{
	struct pg_typing *typing = s->typing;
	switch (s->preparation) {
	case GRAPH_SOURCE: {
		int status = pg_function_source_advance(typing, &s->source);
		if (!status) return;
		if (status > 0) {
			const struct pg_evidence *function = s->source.function;
			if (!s->source_function) {
				s->source_function = function;
				s->outer_context = pg_evidence_for_context(typing, pg_evidence_context(function));
			}
			s->recursive_function = function;
			const struct pg_evidence *body;
			int input = scoped_input(s, function, 0, &body);
			if (input > 0) return;
			if (input < 0) goto unsupported;
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			if (pg_pi_view(pg_evidence_classifier(body), &domain, &binder, &codomain)) {
				s->source = (struct pg_function_source_cursor){.function = body};
				return;
			}
		} else if (!s->source_function) goto unsupported;
		if (function_signature(s, s->recursive_function)) return;
		/* Source queries are graph-owned; later helper work owns a private arena. */
		s->helper = (struct helper_cursor){0};
		s->preparation = GRAPH_HEAD;
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
			size_t offset = pg_evidence_context_map(s->input.parameters)->count + 1;
			s->index_count = pg_evidence_context_map(s->input.indices)->count - offset;
			if (s->index_count > SIZE_MAX / sizeof(*s->index_arguments)) goto error;
			s->index_arguments = pg_alloc(&s->temporary, s->index_count * sizeof(*s->index_arguments));
			if (s->index_count && !s->index_arguments) goto error;
			/* Reuse a generic source telescope. Otherwise abstract the checked
			 * eliminator and specialize it, never solve a fixed index backwards. */
			for (size_t i = s->index_count; i; --i) {
				if (pg_evidence_rule(s->context) != PG_CONTEXT_EXTEND) goto capture;
				const struct pg_term *image = pg_evidence_context_map(s->input.indices)->images[offset + i - 1]->core;
				if (image != pg_reference(typing->graph, pg_evidence_context(s->context)->binder)) goto capture;
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
	if (s->input.indices && !s->captured_input) s->recursive_function = s->source_function;
	s->preparation = GRAPH_PARAMETERS;
	return;
capture:
	if (capture_eliminator(s, pg_evidence_premise(s->body, 3))) goto unsupported;
	return;
parameters:
	if (s->input.indices) {
		struct pg_typed_query *query = pg_substitution_rebase_request(typing, s->context, s->input.parameters);
		if (await_view(s, query)) return;
		s->input.parameters = pg_typed_query_result(query);
		if (!s->input.parameters || !pg_inductive_motive_context_valid(typing,
			s->input.formation, s->input.parameters, s->argument_context)) goto unsupported;
		while (pg_evidence_context(s->recursive_function) != pg_evidence_context(s->context)) {
			const struct pg_evidence *body;
			int input = scoped_input(s, s->recursive_function, 0, &body);
			if (input > 0) return;
			if (input < 0) goto unsupported;
			if (pg_evidence_subject(body)->core->kind != PG_LAMBDA) goto unsupported;
			s->recursive_function = body;
		}
	}
	if (s->captured_input) {
		s->specialization = pg_function_plan_argument_substitution(s, s->context, s->captured_input);
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
				s->branch = pg_alloc(&s->temporary, sizeof(*s->branch));
				if (!s->branch) { s->status = PG_FUNCTION_GRAPH_ERROR; break; }
				continue;
			}
			if (s->building) {
				int status = case_branch(s, s->building);
				if (status < 0) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
				if (!status) s->building = s->building->build_next;
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
				size_t end = pg_evidence_context_map(s->specialization)->count;
				for (size_t i = end - s->index_count - 1 - s->specialized_arity; s->formation && i < end; ++i)
					s->formation = pg_prove_family_application(s->typing, s->formation,
						pg_substitution_image_at(s->typing, s->specialization, i));
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
		if (!plan->parent && s->cases && plan->next_scope < plan->field_count + plan->hypothesis_count + s->arity) {
			if (plan_scope(s, plan)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
			continue;
		}
		if (!plan->discriminant && (!plan->output || plan->continuations)) {
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
	return work && work->state && !work->state->self && work->state->waiting && !work->state->waiting->helper
		? work->state->waiting->helper_source : NULL;
}

int pg_function_graph_supply(struct pg_function_graph_work *work, const struct pg_function_graph_work *dependency)
{
	if (!work || !work->state || !dependency || !dependency->state) return -1;
	struct pg_function_graph_state *s = work->state;
	const struct pg_function_graph_state *d = dependency->state;
	if (s->self || !s->waiting || s->waiting->helper || s == d) return -1;
	if (s->typing != d->typing) return -1;
	if (d->status != PG_FUNCTION_GRAPH_DONE) return -1;
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

void pg_function_graph_destroy(struct pg_function_graph_work *work)
{
	if (!work || !work->state) return;
	if (work->state->preparation != GRAPH_SOURCE) pg_graph_destroy(&work->state->helper.temporary);
	pg_graph_destroy(&work->state->temporary);
	free(work->state);
	work->state = NULL;
}
