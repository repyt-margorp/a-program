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
	const struct pg_evidence **scopes;
	size_t *hypothesis_fields;
	const struct pg_evidence *context, *computation, *output;
	struct pg_whnf_job *normalization;
	const struct graph_call *calls;
	struct graph_call_layout *executed, **ordered;
	const struct pg_function_graph_order *source_order;
	struct graph_continuation *continuations;
};

struct graph_continuation {
	const struct pg_evidence *function;
	const struct pg_evidence *argument;
	struct graph_continuation *next;
};

struct pg_function_graph_state {
	struct pg_graph temporary;
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	struct pg_whnf_work *evaluation;
	const struct pg_evidence *body, *outer_context, *context, *argument_context;
	const struct pg_evidence *recursive_function;
	const struct pg_evidence *source_function;
	const struct pg_evidence *domain, *range, *self, *indices;
	const struct pg_evidence *result_context;
	const struct pg_evidence **arguments;
	size_t arity;
	const struct pg_evidence **index_arguments;
	size_t index_count;
	struct pg_inductive_instance input;
	const struct pg_evidence **results;
	struct graph_case *plans;
	const struct pg_evidence *branch, *branch_context, *branch_argument, *formation, *declaration;
	const struct pg_evidence *branch_input;
	const struct pg_data_schema *schema;
	const struct pg_evidence *packet, *witness, *motive_context, *motive;
	const struct pg_evidence **witness_branches;
	size_t witness_next;
	enum pg_function_graph_status witness_status;
	struct pg_whnf_job *normalization;
	size_t count, next;
	int cases, induction, ready;
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
		return pg_prove_inductive_motive_substitution(s->typing, s->classifiers,
			s->input.formation, s->input.parameters, s->argument_context,
			context, projection(s, context, argument));
	return pg_prove_substitution_pair(s->typing,
		pg_prove_substitution_projection(s->typing, s->context, context), s->argument_context,
		projection(s, context, argument));
}

static const struct pg_evidence *range_at(struct pg_function_graph_state *s,
	const struct pg_evidence *map)
{
	return pg_prove_reindex(s->typing, map, s->range);
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
	uint64_t level;
	if (graph_universe(s, &level)) return -1;
	const struct pg_evidence *x = s->argument_context;
	const struct pg_evidence *y = pg_prove_context_extension(t, s->result_context, pg_binder(t->graph), s->range);
	const struct pg_evidence *universe = pg_prove_universe(t, s->classifiers, y, level);
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

/* Expose executable premises, not formation premises. Substitution is the
 * ordinary typed context map; this view grants no new conversion rule. */
static int computation_view(struct pg_function_graph_state *s,
	const struct pg_evidence *proof, enum pg_evidence_rule *rule,
	const struct pg_evidence **left, const struct pg_evidence **right)
{
	const struct pg_evidence *map = NULL;
	for (;;) {
		*rule = pg_evidence_rule(proof);
		if (*rule == PG_CONTEXT_PROJECTION || *rule == PG_REINDEX) {
			const struct pg_evidence *inner = pg_evidence_premise(proof, 1);
			const struct pg_evidence *step = pg_evidence_premise(proof, 0);
			/* Projection may cross more than one extension. */
			if (*rule == PG_CONTEXT_PROJECTION) {
				const struct pg_evidence *source = pg_evidence_premise(proof, 0);
				while (source && pg_evidence_context(source) != pg_evidence_context(inner))
					source = pg_evidence_premise(source, 0);
				step = pg_prove_substitution_projection(s->typing, source, pg_evidence_premise(proof, 0));
			}
			map = map ? pg_prove_substitution_compose(s->typing, step, map) : step;
			if (!map) return -1;
			proof = inner;
			continue;
		}
		if (*rule == PG_TYPE_CONVERSION || *rule == PG_PURE_NORMALIZATION || *rule == PG_EFFECT_SUBSUMPTION) {
			proof = pg_evidence_premise(proof, 0);
			continue;
		}
		break;
	}
	size_t count;
	switch (*rule) {
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM:
		*left = map ? pg_prove_elimination_reindex(s->typing, s->classifiers, map, proof) : proof;
		*right = NULL;
		return *left ? 0 : -1;
	case PG_APP_ELIM: case PG_FOLD_ELIM: count = 2; break;
	case PG_FORCE_ELIM: case PG_THUNK_INTRO: case PG_RETURN_INTRO: count = 1; break;
	default: return -1;
	}
	*left = pg_evidence_premise(proof, 0);
	*right = count == 2 ? pg_evidence_premise(proof, 1) : NULL;
	if (map) {
		*left = pg_prove_reindex(s->typing, map, *left);
		if (*right) *right = pg_prove_reindex(s->typing, map, *right);
	}
	return *left && (count == 1 || *right) ? 0 : -1;
}

static int plan_case(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
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
	plan->computation = pg_evidence_premise(s->body, 5 + s->next);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *pi = pg_prove_classifier(t, s->classifiers, plan->context, plan->computation);
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

/* Symbolically expose sequencing using retained typing evidence. A recursive
 * call creates an output variable; it is not asserted equal by conversion.
 * The companion witness will supply this variable from the corresponding IH. */
static int plan_step(struct pg_function_graph_state *s, struct graph_case *plan)
{
	struct pg_typing *t = s->typing;
	if (plan->normalization) {
		enum pg_eval_status status = pg_whnf_advance(plan->normalization, 1);
		if (status == PG_EVAL_PENDING) return 0;
		if (status != PG_EVAL_WHNF) return -1;
		const struct pg_evidence *normalized = pg_prove_normalization(t, plan->computation,
			pg_whnf_certificate(plan->normalization));
		plan->normalization = NULL;
		return plan_result(s, plan, pg_prove_return_value(t, normalized));
	}
	if (plan->continuations && plan->continuations->argument) {
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
	if (computation_view(s, plan->computation, &rule, &left, &right)) goto normalize;
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
		size_t i = 0;
		for (; i < plan->hypothesis_count; ++i) {
			const struct pg_context *scope = pg_evidence_context(plan->scopes[plan->field_count + i]);
			if (core == pg_reference(t->graph, scope->binder)) break;
		}
		if (i == plan->hypothesis_count) {
			if (computation_view(s, left, &rule, &left, &right) || rule != PG_THUNK_INTRO) goto normalize;
			plan->computation = left;
			return 0;
		}
		struct graph_call *call = pg_alloc(&s->temporary, sizeof(*call));
		if (!call || plan->call_count == SIZE_MAX) return -1;
		*call = (struct graph_call){.field = plan->hypothesis_fields[i], .hypothesis = i};
		call->child = pg_prove_variable(t, plan->scopes[call->field], pg_evidence_context(plan->scopes[call->field])->binder);
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
			call->child = pg_prove_total_pure_value(t, child_call, pg_binder(t->graph));
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
		const struct pg_evidence *result_type = pg_prove_classifier(t, s->classifiers, plan->context, plan->computation);
		result_type = pg_prove_return_content(t, result_type);
		plan->context = pg_prove_context_extension(t, plan->context, pg_binder(t->graph), result_type);
		if (!plan->context) return -1;
		call->result_context = plan->context;
		call->previous = plan->calls;
		plan->calls = call;
		++plan->call_count;
		value = pg_prove_variable(t, plan->context, pg_evidence_context(plan->context)->binder);
		break;
	}
	case PG_RETURN_INTRO: value = left; break;
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: goto normalize;
	default: return -1;
	}
	return plan_result(s, plan, value);
normalize:
	/* Closed ordinary helpers still use the shared normalizer. Unknown IHs
	 * remain neutral, so this cannot manufacture their return values. */
	plan->normalization = pg_whnf_request(s->evaluation, &pg_pure_policy, pg_evidence_subject(plan->computation)->core);
	return plan->normalization ? 0 : -1;
}

/* Match public source slots to executed calls without changing sequencing.
 * Unused IHs are not calls; repeated calls receive distinct result binders. */
static int order_calls(struct pg_function_graph_state *s, struct graph_case *plan)
{
	if (plan->call_count > SIZE_MAX / sizeof(*plan->ordered)) return -1;
	if (plan->call_count > SIZE_MAX / sizeof(*plan->executed)) return -1;
	plan->ordered = pg_alloc(&s->temporary, plan->call_count * sizeof(*plan->ordered));
	plan->executed = pg_alloc(&s->temporary, plan->call_count * sizeof(*plan->executed));
	if (plan->call_count && (!plan->ordered || !plan->executed)) return -1;
	const struct graph_call *call = plan->calls;
	for (size_t i = plan->call_count; i; --i) {
		if (!call) return -1;
		plan->executed[i - 1].call = call;
		call = call->previous;
	}
	if (call) return -1;
	if (!plan->source_order) {
		for (size_t slot = 0; slot < plan->call_count; ++slot) {
			plan->executed[slot].slot = slot;
			plan->ordered[slot] = &plan->executed[slot];
		}
		return 0;
	}
	if (plan->source_order->count != plan->call_count) return -1;
	struct graph_call_layout **heads = pg_alloc(&s->temporary, plan->field_count * sizeof(*heads));
	struct graph_call_layout **tails = pg_alloc(&s->temporary, plan->field_count * sizeof(*tails));
	if (plan->field_count && (!heads || !tails)) return -1;
	for (size_t i = 0; i < plan->call_count; ++i) {
		struct graph_call_layout *entry = &plan->executed[i];
		call = entry->call;
		if (call->field >= plan->field_count) return -1;
		if (tails[call->field]) tails[call->field]->field_next = entry;
		else heads[call->field] = entry;
		tails[call->field] = entry;
	}
	for (size_t slot = 0; slot < plan->call_count; ++slot) {
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
	const struct pg_term *type, *domain, *codomain;
	const struct pg_object *binder;
	if (pg_thunk_type_view(pg_evidence_classifier(field), &type)) {
		const struct pg_evidence *call = pg_prove_force(t, field);
		const struct pg_evidence *classifier = pg_prove_classifier(t, s->classifiers, scope, call);
		while (classifier && pg_pi_view(pg_evidence_subject(classifier)->core, &domain, &binder, &codomain)) {
			binder = pg_binder(t->graph);
			scope = pg_prove_context_extension(t, scope, binder, pg_prove_pi_domain(t, classifier));
			if (!scope) return NULL;
			call = pg_prove_application(t, projection(s, scope, call), pg_prove_variable(t, scope, binder));
			classifier = pg_prove_classifier(t, s->classifiers, scope, call);
		}
		child = pg_prove_total_pure_value(t, call, pg_binder(t->graph));
	}
	if (!child) return NULL;
	const struct pg_evidence *input = argument_substitution(s, scope, child);
	const struct pg_evidence *call = projection(s, scope, s->recursive_function);
	for (size_t j = 0; call && j < s->index_count; ++j)
		call = pg_prove_application(t, call, pg_substitution_image(t, input,
			pg_evidence_context(s->index_arguments[j])->binder));
	call = pg_prove_application(t, call, child);
	call = pg_prove_abstract(t, s->classifiers, context, scope, call);
	return pg_prove_thunk(t, s->classifiers, call);
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

static int case_branch(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	struct graph_case *plan = &s->plans[s->next];
	if (order_calls(s, plan)) return -1;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->input.schema), s->next);
	const struct pg_evidence *parameters = parameters_at(s, s->self);
	const struct pg_evidence *fields = pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!fields) return -1;
	const struct pg_evidence *context = pg_evidence_premise(fields, 1);
	size_t offset = pg_evidence_premise_count(parameters) + 1;
	size_t count = pg_evidence_premise_count(fields) - offset;
	const struct pg_evidence **values = pg_alloc(&s->temporary, count * sizeof(*values));
	if (count && !values) return -1;
	for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(fields, offset + i);
	s->branch_argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, context), count, values);
	const struct pg_evidence **arguments = pg_alloc(&s->temporary, s->arity * sizeof(*arguments));
	const struct pg_evidence **call_arguments = pg_alloc(&s->temporary, s->arity * sizeof(*call_arguments));
	if (s->arity && (!arguments || !call_arguments)) return -1;
	const struct pg_evidence *map = case_source_map(s, plan, context, s->branch_argument, values, arguments);
	if (!map) return -1;
	context = pg_evidence_premise(map, 1);
	size_t mapped = 0;
	for (size_t slot = 0; slot < plan->call_count; ++slot) {
		struct graph_call_layout *entry = plan->ordered[slot];
		const struct graph_call *call = entry->call;
		for (size_t i = 0; i < s->arity; ++i) call_arguments[i] = map_value(s, map, call->arguments[i]);
		const struct pg_evidence *child = map_value(s, map, call->child);
		const struct pg_evidence *input = input_substitution(s, context, child, call_arguments);
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), range_at(s, input));
		if (!context) return -1;
		const struct pg_evidence *output = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		const struct pg_evidence *relation = pg_prove_variable(t, context, pg_evidence_context(s->self)->binder);
		input = input_substitution(s, context, child, call_arguments);
		relation = apply_relation(s, relation, input, output);
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), relation);
		if (!context) return -1;
		entry->layout_output = output;
		while (mapped < plan->call_count && plan->executed[mapped].layout_output) {
			const struct graph_call_layout *ready = &plan->executed[mapped];
			map = pg_prove_substitution_compose(t, map,
				pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context));
			map = pg_prove_substitution_pair(t, map, ready->call->result_context, projection(s, context, ready->layout_output));
			if (!map) return -1;
			++mapped;
		}
	}
	if (mapped != plan->call_count) return -1;
	const struct pg_evidence *lift = pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context);
	map = pg_prove_substitution_compose(t, map, lift);
	s->branch_argument = projection(s, context, s->branch_argument);
	s->branch_input = input_substitution(s, context, s->branch_argument, arguments);
	s->branch = pg_prove_return(t, s->classifiers, pg_prove_reindex(t, map, plan->output));
	s->branch_context = context;
	return s->branch ? 0 : -1;
}

static int direct_branch(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	s->branch_context = pg_prove_context_extension(t, s->self, pg_binder(t->graph), projection(s, s->self, s->domain));
	if (!s->branch_context) return -1;
	s->branch_argument = pg_prove_variable(t, s->branch_context, pg_evidence_context(s->branch_context)->binder);
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, s->branch_context);
	map = pg_prove_substitution_pair(t, map, s->argument_context, s->branch_argument);
	s->branch = pg_prove_reindex(t, map, s->body);
	for (size_t i = 0; s->branch && i < s->arity; ++i) {
		s->branch_context = pg_prove_context_extension(t, s->branch_context, pg_binder(t->graph),
			map_value(s, map, pg_evidence_premise(s->arguments[i], 1)));
		if (!s->branch_context) return -1;
		const struct pg_evidence *argument = pg_prove_variable(t, s->branch_context, pg_evidence_context(s->branch_context)->binder);
		map = pg_prove_substitution_compose(t, map, pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), s->branch_context));
		map = pg_prove_substitution_pair(t, map, s->arguments[i], argument);
		s->branch = pg_prove_application(t, projection(s, s->branch_context, s->branch), argument);
	}
	s->branch_input = map;
	return s->branch ? 0 : -1;
}

const struct pg_evidence *pg_function_graph_source(const struct pg_evidence *function)
{
	while (function) {
		enum pg_evidence_rule rule = pg_evidence_rule(function);
		if (rule == PG_LAMBDA_INTRO) return function;
		const struct pg_evidence *inner;
		switch (rule) {
		case PG_THUNK_INTRO: case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION:
			inner = pg_evidence_premise(function, 0); break;
		case PG_CONTEXT_PROJECTION:
			inner = pg_evidence_premise(function, 1); break;
		default: return NULL;
		}
		if (pg_evidence_context(inner) != pg_evidence_context(function)) return NULL;
		function = inner;
	}
	return NULL;
}

static int prepare_graph(struct pg_function_graph_state *s);

int pg_function_graph_init(struct pg_function_graph_work *work,
	struct pg_typing *typing, struct pg_classifiers *classifiers,
	struct pg_whnf_work *evaluation, const struct pg_evidence *function)
{
	if (!work) return -1;
	*work = (struct pg_function_graph_work){0};
	if (!typing || !classifiers || classifiers->graph != typing->graph || !evaluation) return -1;
	if (evaluation->graph != typing->graph) return -1;
	if (!pg_evidence_owned_by(function, typing)) return -1;
	struct pg_function_graph_state *s = calloc(1, sizeof(*s));
	if (!s) return -1;
	work->state = s;
	s->typing = typing; s->classifiers = classifiers; s->evaluation = evaluation;
	function = pg_function_graph_source(function);
	if (!function) goto unsupported;
	s->source_function = function;
	s->outer_context = pg_evidence_premise(pg_evidence_premise(pg_evidence_premise(function, 0), 0), 0);
	while (pg_evidence_rule(pg_evidence_premise(function, 1)) == PG_LAMBDA_INTRO)
		function = pg_evidence_premise(function, 1);
	s->recursive_function = function;
	const struct pg_evidence *pi = pg_evidence_premise(function, 0);
	s->argument_context = pg_evidence_premise(pi, 0);
	s->context = pg_evidence_premise(s->argument_context, 0);
	if (pg_evidence_rule(s->argument_context) != PG_CONTEXT_EXTEND) goto unsupported;
	s->domain = pg_evidence_premise(s->argument_context, 1);
	const struct pg_evidence *result = pg_evidence_premise(pi, 1);
	s->result_context = s->argument_context;
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
	if (pg_evidence_rule(s->body) == PG_MATCH_ELIM || pg_evidence_rule(s->body) == PG_INDUCTION_ELIM)
		return prepare_graph(s);
	return 0;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return 0;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
	return 0;
}

static int prepare_graph(struct pg_function_graph_state *s)
{
	struct pg_typing *typing = s->typing;
	const struct pg_evidence *source_function = s->source_function;
	enum pg_evidence_rule rule = pg_evidence_rule(s->body);
	s->cases = rule == PG_MATCH_ELIM || rule == PG_INDUCTION_ELIM;
	s->induction = rule == PG_INDUCTION_ELIM;
	s->count = 1;
	if (s->cases) {
		const struct pg_evidence *scrutinee = pg_evidence_premise(s->body, 3);
		const struct pg_term *variable = pg_reference(typing->graph, pg_evidence_context(s->argument_context)->binder);
		if (pg_evidence_subject(scrutinee)->core != variable) goto unsupported;
		if (!pg_inductive_instance(typing, s->domain, &s->input)) goto unsupported;
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
			s->input.parameters = pg_prove_substitution_rebase(typing, s->context, s->input.parameters);
			if (!s->input.parameters || !pg_inductive_motive_context_valid(typing,
				s->input.formation, s->input.parameters, s->argument_context)) goto unsupported;
			while (source_function && pg_evidence_context(source_function) != pg_evidence_context(s->context))
				source_function = pg_evidence_rule(pg_evidence_premise(source_function, 1)) == PG_LAMBDA_INTRO
					? pg_evidence_premise(source_function, 1) : NULL;
			if (!source_function) goto unsupported;
			s->recursive_function = source_function;
		}
		s->count = pg_data_constructor_count(s->input.schema);
	}
	if (s->count > SIZE_MAX / sizeof(*s->results)) goto error;
	s->results = pg_alloc(&s->temporary, s->count * sizeof(*s->results));
	if (s->count > SIZE_MAX / sizeof(*s->plans)) goto error;
	s->plans = pg_alloc(&s->temporary, s->count * sizeof(*s->plans));
	if ((s->count && (!s->results || !s->plans)) || signature(s)) goto error;
	s->ready = 1;
	return 0;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return 0;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
	return 0;
}

/* Expose typed beta/quotation/sequencing wrappers one scheduled step at a
 * time. A neutral Match keeps its motive and branches; it is not a RETURN. */
static int prepare_head(struct pg_function_graph_state *s)
{
	const struct pg_evidence *left, *right, *body = NULL;
	enum pg_evidence_rule rule;
	if (computation_view(s, s->body, &rule, &left, &right)) return 0;
	switch (rule) {
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM:
		s->body = left;
		return 0;
	case PG_APP_ELIM:
		body = pg_prove_application_body(s->typing, left, right);
		break;
	case PG_FOLD_ELIM:
		body = pg_prove_application_body(s->typing, right, pg_prove_return_value(s->typing, left));
		break;
	case PG_FORCE_ELIM:
		if (!computation_view(s, left, &rule, &left, &right) && rule == PG_THUNK_INTRO) body = left;
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

int pg_function_graph_source_order(struct pg_function_graph_work *work,
	size_t count, const struct pg_function_graph_order *orders)
{
	if (!work || !work->state) return -1;
	struct pg_function_graph_state *s = work->state;
	if (s->status != PG_FUNCTION_GRAPH_PENDING || s->next || s->branch) return -1;
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
		if (!s->ready) {
			if (!prepare_head(s)) prepare_graph(s);
			continue;
		}
		if (s->next == s->count) {
			s->schema = pg_data_schema(s->typing,
				pg_data_signature(s->typing, s->self, s->indices), s->count, s->results);
			s->declaration = pg_prove_inductive_type(s->typing, s->classifiers, s->schema);
			s->formation = s->declaration;
			for (const struct pg_evidence *context = s->context;
				s->formation && pg_evidence_context(context) != pg_evidence_context(s->outer_context);
				context = pg_evidence_premise(context, 0))
				s->formation = pg_prove_family_abstraction(s->typing, context, s->formation);
			s->status = s->formation ? PG_FUNCTION_GRAPH_DONE : PG_FUNCTION_GRAPH_UNSUPPORTED;
			break;
		}
		if (!s->branch) {
			if (s->cases) {
				struct graph_case *plan = &s->plans[s->next];
				if (!plan->computation) {
					if (plan_case(s, plan)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
					continue;
				}
				if (!plan->output) {
					if (plan_step(s, plan) < 0) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
					continue;
				}
			}
			if (s->cases ? case_branch(s) : direct_branch(s)) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
			s->normalization = pg_whnf_request(s->evaluation, &pg_pure_policy, pg_evidence_subject(s->branch)->core);
			if (!s->normalization) { s->status = PG_FUNCTION_GRAPH_ERROR; break; }
			continue;
		}
		enum pg_eval_status status = pg_whnf_advance(s->normalization, 1);
		if (status == PG_EVAL_PENDING) continue;
		if (status != PG_EVAL_WHNF) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
		const struct pg_evidence *normalized = pg_prove_normalization(s->typing, s->branch, pg_whnf_certificate(s->normalization));
		const struct pg_evidence *output = pg_prove_return_value(s->typing, normalized);
		if (!output) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
		const struct pg_evidence *map = pg_prove_substitution_projection(s->typing, s->self, s->branch_context);
		size_t arity = s->index_count + s->arity + 2;
		const struct pg_evidence **values = pg_alloc(&s->temporary, arity * sizeof(*values));
		if (!values || !s->branch_input) { s->status = PG_FUNCTION_GRAPH_ERROR; break; }
		size_t inputs = pg_evidence_premise_count(s->branch_input);
		for (size_t i = 0; i < arity - 1; ++i)
			values[i] = pg_evidence_premise(s->branch_input, inputs - arity + 1 + i);
		values[arity - 1] = output;
		s->results[s->next] = pg_prove_substitution_extend(s->typing, map, s->indices, arity, values);
		if (!s->results[s->next]) { s->status = PG_FUNCTION_GRAPH_UNSUPPORTED; break; }
		++s->next;
		s->branch = NULL; s->normalization = NULL;
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
	return work && work->state && work->state->status == PG_FUNCTION_GRAPH_DONE ? work->state->input.formation : NULL;
}

static const struct pg_evidence *packet_type(struct pg_function_graph_state *s,
	const struct pg_evidence *map)
{
	return pg_prove_reindex(s->typing, map, s->packet);
}

static const struct pg_object *packet_constructor(struct pg_function_graph_state *s)
{
	return pg_data_constructor(pg_data_declaration_layout(pg_evidence_inductive_declaration(s->packet)), 0);
}

static int packet_formation(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	uint64_t level;
	if (graph_universe(s, &level)) return -1;
	const struct pg_evidence *u = pg_prove_universe(t, s->classifiers, s->result_context, level);
	const struct pg_evidence *self = pg_prove_context_extension(t, s->result_context, pg_binder(t->graph), u);
	const struct pg_evidence *output = pg_prove_context_extension(t, self, pg_binder(t->graph), projection(s, self, s->range));
	if (!output) return -1;
	const struct pg_evidence *relation = projection(s, output, s->declaration);
	relation = apply_relation(s, relation, pg_prove_substitution_projection(t, s->result_context, output),
		pg_prove_variable(t, output, pg_evidence_context(output)->binder));
	const struct pg_evidence *fields = pg_prove_context_extension(t, output, pg_binder(t->graph), relation);
	const struct pg_evidence *result = pg_prove_substitution_projection(t, self, fields);
	const struct pg_data_schema *schema = pg_data_schema(t, pg_data_signature(t, self, self), 1, &result);
	s->packet = pg_prove_inductive_type(t, s->classifiers, schema);
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
	return pg_prove_return_contract(t, s->classifiers, s->totality, packet);
}

struct packet_frame {
	const struct pg_evidence *before, *bound, *parameters, *input, *value, *fields, *target;
};

static const struct pg_evidence *witness_case(struct pg_function_graph_state *s, size_t index)
{
	struct pg_typing *t = s->typing;
	const struct graph_case *plan = &s->plans[index];
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->input.schema), index);
	const struct pg_evidence *parameters = parameters_at(s, s->argument_context);
	const struct pg_evidence *map = s->induction
		? pg_prove_induction_scope(t, s->classifiers, s->input.formation, constructor, parameters, s->motive_context, s->motive)
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
	size_t calls = plan->call_count;
	if (count > SIZE_MAX / sizeof(void *) || s->arity > SIZE_MAX / sizeof(void *) - count) return NULL;
	if (calls > (SIZE_MAX / sizeof(void *) - count - s->arity) / 2) return NULL;
	if (calls > SIZE_MAX / sizeof(struct packet_frame)) return NULL;
	const struct pg_evidence **values = pg_alloc(&s->temporary, (count + s->arity + 2 * calls) * sizeof(*values));
	const struct pg_evidence **call_arguments = pg_alloc(&s->temporary, s->arity * sizeof(*call_arguments));
	const struct pg_object **ih_binders = pg_alloc(&s->temporary, ih_count * sizeof(*ih_binders));
	struct packet_frame *frames = pg_alloc(&s->temporary, calls * sizeof(*frames));
	if ((count + s->arity + 2 * calls && !values) || (calls && !frames) || (ih_count && !ih_binders)) return NULL;
	if (s->arity && !call_arguments) return NULL;
	const struct pg_context *scope = pg_evidence_context(base);
	for (size_t i = ih_count; i; --i, scope = scope->parent) ih_binders[i - 1] = scope->binder;
	for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(map, offset + i);
	const struct pg_evidence *argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, base), count, values);
	if (!argument) return NULL;
	const struct pg_evidence *source_map = case_source_map(s, plan, context, argument, values, values + count);
	if (!source_map) return NULL;
	context = pg_evidence_premise(source_map, 1);
	const struct pg_evidence *arguments_context = context;
	for (size_t next = 0; next < calls; ++next) {
		const struct graph_call_layout *entry = &plan->executed[next];
		const struct graph_call *call = entry->call;
		if (call->hypothesis >= ih_count) return NULL;
		struct packet_frame *f = &frames[next];
		f->before = context;
		f->target = pg_prove_computation_type(t, s->classifiers, s->totality,
			pg_effect_row(t->graph, 0, NULL), packet_type(s, input_substitution(s, context, argument, values + count)));
		f->input = pg_prove_force(t, pg_prove_variable(t, context, ih_binders[call->hypothesis]));
		const struct pg_evidence *field = map_value(s, source_map, call->child);
		for (size_t i = 0; f->input && i < call->field_arity; ++i)
			f->input = pg_prove_application(t, f->input, map_value(s, source_map, call->field_arguments[i]));
		for (size_t i = 0; i < s->arity; ++i) {
			call_arguments[i] = map_value(s, source_map, call->arguments[i]);
			f->input = pg_prove_application(t, f->input, call_arguments[i]);
		}
		f->bound = pg_prove_context_extension(t, context, pg_binder(t->graph), packet_type(s, input_substitution(s, context, field, call_arguments)));
		if (!f->bound || !f->input || !f->target) return NULL;
		f->value = pg_prove_variable(t, f->bound, pg_evidence_context(f->bound)->binder);
		f->parameters = input_substitution(s, f->bound, field, call_arguments);
		f->fields = pg_prove_constructor_scope(t, s->packet, packet_constructor(s), f->parameters);
		if (!f->fields) return NULL;
		context = pg_evidence_premise(f->fields, 1);
		size_t start = pg_evidence_premise_count(f->parameters) + 1;
		values[count + s->arity + 2 * entry->slot] = pg_evidence_premise(f->fields, start);
		values[count + s->arity + 2 * entry->slot + 1] = pg_evidence_premise(f->fields, start + 1);
		source_map = pg_prove_substitution_compose(t, source_map,
			pg_prove_substitution_projection(t, pg_evidence_premise(source_map, 1), context));
		source_map = pg_prove_substitution_pair(t, source_map, call->result_context, pg_evidence_premise(f->fields, start));
		if (!source_map) return NULL;
	}
	for (size_t i = 0; i < count + s->arity + 2 * calls; ++i) values[i] = projection(s, context, values[i]);
	const struct pg_evidence *body = return_packet(s, index, context, count + s->arity + 2 * calls, values);
	for (size_t i = calls; body && i; --i) {
		const struct packet_frame *f = &frames[i - 1];
		body = pg_prove_abstract(t, s->classifiers, f->bound, pg_evidence_premise(f->fields, 1), body);
		const struct pg_evidence *mc = pg_prove_inductive_motive_context(t, s->packet, f->parameters, pg_binder(t->graph));
		body = pg_prove_match(t, s->classifiers, s->packet, f->parameters, f->value, mc,
			projection(s, mc, f->target), 1, &body);
		body = pg_prove_abstract(t, s->classifiers, f->before, f->bound, body);
		body = pg_prove_fold(t, s->classifiers, f->input, body);
	}
	body = pg_prove_abstract(t, s->classifiers, base, arguments_context, body);
	return pg_prove_abstract(t, s->classifiers, s->argument_context, base, body);
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
				s->motive = pg_prove_computation_type(s->typing, s->classifiers, s->totality,
					pg_effect_row(s->typing->graph, 0, NULL), packet_type(s, map));
				for (size_t i = s->arity; s->motive && i; --i, context = pg_evidence_premise(context, 0))
					s->motive = pg_prove_pi(s->typing, s->classifiers, context, s->motive);
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
			const struct pg_evidence **values = pg_alloc(&s->temporary, (s->arity + 1) * sizeof(*values));
			if (!values) { s->witness_status = PG_FUNCTION_GRAPH_ERROR; break; }
			values[0] = projection(s, s->result_context, argument);
			for (size_t i = 0; i < s->arity; ++i) values[i + 1] = pg_prove_variable(s->typing, s->result_context,
				pg_evidence_context(s->arguments[i])->binder);
			body = return_packet(s, 0, s->result_context, s->arity + 1, values);
			body = pg_prove_abstract(s->typing, s->classifiers, s->argument_context, s->result_context, body);
		}
		else if (s->induction) body = pg_prove_induction(s->typing, s->classifiers, s->input.formation,
			parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		else body = pg_prove_match(s->typing, s->classifiers, s->input.formation,
			parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		s->witness = pg_prove_abstract(s->typing, s->classifiers, s->outer_context, s->argument_context, body);
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
	pg_graph_destroy(&work->state->temporary);
	free(work->state);
	work->state = NULL;
}
