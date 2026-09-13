#include "function_graph.h"
#include "computation.h"

#include <stdlib.h>

struct graph_call {
	size_t field, hypothesis, slot;
	const struct pg_evidence *result_context;
	const struct pg_evidence *layout_output;
	struct graph_call *next;
	struct graph_call *field_next;
};

struct graph_case {
	size_t field_count, hypothesis_count, call_count;
	const struct pg_evidence **scopes;
	size_t *hypothesis_fields;
	const struct pg_evidence *context, *computation, *output;
	struct pg_whnf_job *normalization;
	struct graph_call *calls, **tail;
	struct graph_call **ordered;
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
	const struct pg_evidence *function, *body, *outer_context, *context, *argument_context;
	const struct pg_evidence *domain, *range, *self, *indices;
	struct pg_inductive_instance input;
	const struct pg_evidence **results;
	struct graph_case *plans;
	const struct pg_evidence *branch, *branch_context, *branch_argument, *formation, *declaration;
	const struct pg_data_schema *schema;
	const struct pg_evidence *packet, *witness, *motive_context, *motive;
	const struct pg_evidence **witness_branches;
	size_t witness_next;
	enum pg_function_graph_status witness_status;
	struct pg_whnf_job *normalization;
	size_t count, next;
	int cases, induction;
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
	return pg_prove_substitution_pair(s->typing,
		pg_prove_substitution_projection(s->typing, s->context, context), s->argument_context,
		projection(s, context, argument));
}

static const struct pg_evidence *range_at(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument)
{
	return pg_prove_reindex(s->typing, argument_substitution(s, context, argument), s->range);
}

static const struct pg_evidence *parameters_at(struct pg_function_graph_state *s,
	const struct pg_evidence *context)
{
	const struct pg_evidence *map = pg_prove_substitution_projection(s->typing, s->context, context);
	return pg_prove_substitution_compose(s->typing, s->input.parameters, map);
}

static int signature(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	uint64_t a, b;
	if (!pg_universe_level(pg_evidence_classifier(s->domain), &a) ||
		!pg_universe_level(pg_evidence_classifier(s->range), &b)) return -1;
	const struct pg_evidence *x = s->argument_context;
	const struct pg_evidence *y = pg_prove_context_extension(t, x, pg_binder(t->graph), s->range);
	const struct pg_evidence *universe = pg_prove_universe(t, s->classifiers, y, a > b ? a : b);
	s->self = pg_prove_family_context_extension(t, s->context, pg_binder(t->graph), y, universe);
	if (!s->self) return -1;
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, s->self);
	map = pg_prove_substitution_lift(t, map, x, pg_binder(t->graph));
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
		const struct pg_term *type = pg_evidence_context(fields)->declared_type, *content;
		int recursive = pg_data_recursive_field(type, parameters->binder);
		if (recursive < 0) return -1;
		if (!recursive) continue;
		if (pg_thunk_type_view(type, &content)) return -1;
		plan->hypothesis_fields[plan->hypothesis_count++] = i - 1;
	}
	for (size_t i = 0; i < plan->hypothesis_count / 2; ++i) {
		size_t j = plan->hypothesis_count - 1 - i, field = plan->hypothesis_fields[i];
		plan->hypothesis_fields[i] = plan->hypothesis_fields[j];
		plan->hypothesis_fields[j] = field;
	}
	if (plan->hypothesis_count > SIZE_MAX - count) return -1;
	count += plan->hypothesis_count;
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
	plan->tail = &plan->calls;
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
		const struct pg_evidence *result_type = pg_prove_classifier(t, s->classifiers, plan->context, plan->computation);
		result_type = pg_prove_return_content(t, result_type);
		plan->context = pg_prove_context_extension(t, plan->context, pg_binder(t->graph), result_type);
		if (!plan->context) return -1;
		*call = (struct graph_call){.field = plan->hypothesis_fields[i], .hypothesis = i,
			.result_context = plan->context};
		*plan->tail = call; plan->tail = &call->next;
		++plan->call_count;
		value = pg_prove_variable(t, plan->context, pg_evidence_context(plan->context)->binder);
		break;
	}
	case PG_RETURN_INTRO: value = left; break;
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
	plan->ordered = pg_alloc(&s->temporary, plan->call_count * sizeof(*plan->ordered));
	if (plan->call_count && !plan->ordered) return -1;
	if (!plan->source_order) {
		size_t slot = 0;
		for (struct graph_call *call = plan->calls; call; call = call->next) {
			call->slot = slot;
			plan->ordered[slot++] = call;
		}
		return 0;
	}
	if (plan->source_order->count != plan->call_count) return -1;
	struct graph_call **heads = pg_alloc(&s->temporary, plan->field_count * sizeof(*heads));
	struct graph_call **tails = pg_alloc(&s->temporary, plan->field_count * sizeof(*tails));
	if (plan->field_count && (!heads || !tails)) return -1;
	for (struct graph_call *call = plan->calls; call; call = call->next) {
		if (call->field >= plan->field_count) return -1;
		if (tails[call->field]) tails[call->field]->field_next = call;
		else heads[call->field] = call;
		tails[call->field] = call;
	}
	for (size_t slot = 0; slot < plan->call_count; ++slot) {
		size_t field = plan->source_order->fields[slot];
		if (field >= plan->field_count || !heads[field]) return -1;
		struct graph_call *call = heads[field];
		heads[field] = call->field_next;
		call->slot = slot;
		plan->ordered[slot] = call;
	}
	return 0;
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
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, context);
	map = pg_prove_substitution_pair(t, map, s->argument_context, s->branch_argument);
	for (size_t i = 0; map && i < count; ++i)
		map = pg_prove_substitution_pair(t, map, plan->scopes[i], values[i]);
	/* Complete the source context map with the already-typed original calls,
	 * never guessed outputs. Exposed calls now refer to their result variables;
	 * unused or delayed IH references retain the original function semantics. */
	for (size_t i = 0; map && i < plan->hypothesis_count; ++i) {
		const struct pg_evidence *call = pg_prove_application(t, projection(s, context, s->function), values[plan->hypothesis_fields[i]]);
		map = pg_prove_substitution_pair(t, map, plan->scopes[count + i], pg_prove_thunk(t, s->classifiers, call));
	}
	if (!map) return -1;
	for (size_t slot = 0; slot < plan->call_count; ++slot) {
		struct graph_call *call = plan->ordered[slot];
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), range_at(s, context, values[call->field]));
		if (!context) return -1;
		const struct pg_evidence *output = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		const struct pg_evidence *relation = pg_prove_variable(t, context, pg_evidence_context(s->self)->binder);
		relation = pg_prove_family_application(t, relation, projection(s, context, values[call->field]));
		relation = pg_prove_family_application(t, relation, output);
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), relation);
		if (!context) return -1;
		call->layout_output = output;
	}
	const struct pg_evidence *lift = pg_prove_substitution_projection(t, pg_evidence_premise(map, 1), context);
	map = pg_prove_substitution_compose(t, map, lift);
	for (const struct graph_call *call = plan->calls; call; call = call->next) {
		map = pg_prove_substitution_pair(t, map, call->result_context, projection(s, context, call->layout_output));
		if (!map) return -1;
	}
	s->branch_argument = projection(s, context, s->branch_argument);
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
	s->outer_context = pg_evidence_premise(pg_evidence_premise(pg_evidence_premise(function, 0), 0), 0);
	while (pg_evidence_rule(pg_evidence_premise(function, 1)) == PG_LAMBDA_INTRO)
		function = pg_evidence_premise(function, 1);
	s->function = function;
	const struct pg_evidence *pi = pg_evidence_premise(function, 0);
	s->argument_context = pg_evidence_premise(pi, 0);
	s->context = pg_evidence_premise(s->argument_context, 0);
	if (pg_evidence_rule(s->argument_context) != PG_CONTEXT_EXTEND) goto unsupported;
	s->domain = pg_evidence_premise(s->argument_context, 1);
	const struct pg_evidence *result = pg_evidence_premise(pi, 1);
	const struct pg_term *range;
	if (!result || !pg_pure_computation_type_view(pg_evidence_subject(result)->core, &s->totality, &range)) goto unsupported;
	s->range = pg_prove_return_content(typing, result);
	if (!s->range) goto unsupported;
	s->body = pg_evidence_premise(function, 1);
	enum pg_evidence_rule rule = pg_evidence_rule(s->body);
	s->cases = rule == PG_MATCH_ELIM || rule == PG_INDUCTION_ELIM;
	s->induction = rule == PG_INDUCTION_ELIM;
	s->count = 1;
	if (s->cases) {
		const struct pg_evidence *scrutinee = pg_evidence_premise(s->body, 3);
		const struct pg_term *variable = pg_reference(typing->graph, pg_evidence_context(s->argument_context)->binder);
		if (pg_evidence_subject(scrutinee)->core != variable) goto unsupported;
		if (!pg_inductive_instance(typing, s->domain, &s->input) || s->input.indices) goto unsupported;
		if (s->input.formation != pg_evidence_premise(s->body, 1)) goto unsupported;
		s->count = pg_data_constructor_count(s->input.schema);
	}
	if (s->count > SIZE_MAX / sizeof(*s->results)) goto error;
	s->results = pg_alloc(&s->temporary, s->count * sizeof(*s->results));
	if (s->count > SIZE_MAX / sizeof(*s->plans)) goto error;
	s->plans = pg_alloc(&s->temporary, s->count * sizeof(*s->plans));
	if ((s->count && (!s->results || !s->plans)) || signature(s)) goto error;
	return 0;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return 0;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
	return 0;
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
		const struct pg_evidence *values[] = {s->branch_argument, output};
		s->results[s->next] = pg_prove_substitution_extend(s->typing, map, s->indices, 2, values);
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
	const struct pg_evidence *context, const struct pg_evidence *argument)
{
	return pg_prove_reindex(s->typing, argument_substitution(s, context, argument), s->packet);
}

static const struct pg_object *packet_constructor(struct pg_function_graph_state *s)
{
	return pg_data_constructor(pg_data_declaration_layout(pg_evidence_inductive_declaration(s->packet)), 0);
}

static int packet_formation(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	uint64_t a, b;
	if (!pg_universe_level(pg_evidence_classifier(s->domain), &a) ||
		!pg_universe_level(pg_evidence_classifier(s->range), &b)) return -1;
	const struct pg_evidence *u = pg_prove_universe(t, s->classifiers, s->argument_context, a > b ? a : b);
	const struct pg_evidence *self = pg_prove_context_extension(t, s->argument_context, pg_binder(t->graph), u);
	const struct pg_evidence *output = pg_prove_context_extension(t, self, pg_binder(t->graph), projection(s, self, s->range));
	if (!output) return -1;
	const struct pg_evidence *relation = projection(s, output, s->declaration);
	relation = pg_prove_family_application(t, relation,
		pg_prove_variable(t, output, pg_evidence_context(s->argument_context)->binder));
	relation = pg_prove_family_application(t, relation, pg_prove_variable(t, output, pg_evidence_context(output)->binder));
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
	if (n < 2) return NULL;
	const struct pg_evidence *input = pg_evidence_premise(result, n - 2), *output = pg_evidence_premise(result, n - 1);
	const struct pg_evidence *values[] = {output, graph};
	const struct pg_evidence *packet = pg_prove_constructor(t, s->packet, packet_constructor(s),
		argument_substitution(s, context, input), 2, values);
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
	if (count > SIZE_MAX / sizeof(void *) || calls > (SIZE_MAX / sizeof(void *) - count) / 2) return NULL;
	if (calls > SIZE_MAX / sizeof(struct packet_frame)) return NULL;
	const struct pg_evidence **values = pg_alloc(&s->temporary, (count + 2 * calls) * sizeof(*values));
	const struct pg_object **ih_binders = pg_alloc(&s->temporary, ih_count * sizeof(*ih_binders));
	struct packet_frame *frames = pg_alloc(&s->temporary, calls * sizeof(*frames));
	if ((count + 2 * calls && !values) || (calls && !frames) || (ih_count && !ih_binders)) return NULL;
	const struct pg_context *scope = pg_evidence_context(base);
	for (size_t i = ih_count; i; --i, scope = scope->parent) ih_binders[i - 1] = scope->binder;
	for (size_t i = 0; i < count; ++i) values[i] = pg_evidence_premise(map, offset + i);
	const struct pg_evidence *argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, base), count, values);
	if (!argument) return NULL;
	size_t next = 0;
	for (const struct graph_call *call = plan->calls; call; call = call->next) {
		if (next >= calls || call->hypothesis >= ih_count) return NULL;
		struct packet_frame *f = &frames[next];
		f->before = context;
		f->target = pg_prove_computation_type(t, s->classifiers, s->totality,
			pg_effect_row(t->graph, 0, NULL), packet_type(s, context, projection(s, context, argument)));
		f->input = pg_prove_force(t, pg_prove_variable(t, context, ih_binders[call->hypothesis]));
		const struct pg_evidence *field = projection(s, context, values[call->field]);
		f->bound = pg_prove_context_extension(t, context, pg_binder(t->graph), packet_type(s, context, field));
		if (!f->bound || !f->input || !f->target) return NULL;
		f->value = pg_prove_variable(t, f->bound, pg_evidence_context(f->bound)->binder);
		f->parameters = argument_substitution(s, f->bound, field);
		f->fields = pg_prove_constructor_scope(t, s->packet, packet_constructor(s), f->parameters);
		if (!f->fields) return NULL;
		context = pg_evidence_premise(f->fields, 1);
		size_t start = pg_evidence_premise_count(f->parameters) + 1;
		values[count + 2 * call->slot] = pg_evidence_premise(f->fields, start);
		values[count + 2 * call->slot + 1] = pg_evidence_premise(f->fields, start + 1);
		++next;
	}
	if (next != calls) return NULL;
	for (size_t i = 0; i < count + 2 * calls; ++i) values[i] = projection(s, context, values[i]);
	const struct pg_evidence *body = return_packet(s, index, context, count + 2 * calls, values);
	for (size_t i = calls; body && i; --i) {
		const struct packet_frame *f = &frames[i - 1];
		body = pg_prove_abstract(t, s->classifiers, f->bound, pg_evidence_premise(f->fields, 1), body);
		const struct pg_evidence *mc = pg_prove_inductive_motive_context(t, s->packet, f->parameters, pg_binder(t->graph));
		body = pg_prove_match(t, s->classifiers, s->packet, f->parameters, f->value, mc,
			projection(s, mc, f->target), 1, &body);
		body = pg_prove_abstract(t, s->classifiers, f->before, f->bound, body);
		body = pg_prove_fold(t, s->classifiers, f->input, body);
	}
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
				s->motive = pg_prove_computation_type(s->typing, s->classifiers, s->totality,
					pg_effect_row(s->typing->graph, 0, NULL), packet_type(s, s->motive_context, z));
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
		if (!s->cases) body = return_packet(s, 0, s->argument_context, 1, &argument);
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
