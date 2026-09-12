#include "function_graph.h"
#include "computation.h"

#include <stdlib.h>

struct pg_function_graph_state {
	struct pg_graph temporary;
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	struct pg_whnf_work *evaluation;
	const struct pg_evidence *body, *context, *argument_context;
	const struct pg_evidence *domain, *range, *self, *indices;
	struct pg_inductive_instance input;
	const struct pg_evidence **results;
	const struct pg_evidence *branch, *branch_context, *branch_argument, *formation;
	struct pg_whnf_job *normalization;
	size_t count, next;
	int cases, induction;
	enum pg_function_graph_status status;
};

static const struct pg_evidence *projection(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	return pg_prove_projection(s->typing, context, proof);
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
	const struct pg_evidence *x = pg_prove_context_extension(t, s->context, pg_binder(t->graph), s->domain);
	const struct pg_evidence *y = pg_prove_context_extension(t, x, pg_binder(t->graph), projection(s, x, s->range));
	const struct pg_evidence *universe = pg_prove_universe(t, s->classifiers, y, a > b ? a : b);
	s->self = pg_prove_family_context_extension(t, s->context, pg_binder(t->graph), y, universe);
	if (!s->self) return -1;
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, s->self);
	map = pg_prove_substitution_lift(t, map, x, pg_binder(t->graph));
	map = pg_prove_substitution_lift(t, map, y, pg_binder(t->graph));
	s->indices = pg_evidence_premise(map, 1);
	return s->indices ? 0 : -1;
}

/* Replace recursive-result thunks by RETURN of fresh output variables. Each
 * such variable is accompanied by a positive recursive graph premise. */
static int case_branch(struct pg_function_graph_state *s)
{
	struct pg_typing *t = s->typing;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->input.schema), s->next);
	const struct pg_evidence *parameters = parameters_at(s, s->self);
	const struct pg_evidence *fields = pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!fields) return -1;
	const struct pg_evidence *context = pg_evidence_premise(fields, 1);
	size_t offset = pg_evidence_premise_count(parameters) + 1;
	size_t count = pg_evidence_premise_count(fields) - offset;
	const struct pg_evidence **values = pg_alloc(&s->temporary, count * sizeof(*values));
	const struct pg_evidence **hypotheses = pg_alloc(&s->temporary, count * sizeof(*hypotheses));
	const struct pg_evidence **extensions = pg_alloc(&s->temporary, count * sizeof(*extensions));
	if (count && (!values || !hypotheses || !extensions)) return -1;
	const struct pg_evidence *original = pg_data_schema_fields(s->input.schema, constructor);
	for (size_t i = count; i; --i, original = pg_evidence_premise(original, 0)) extensions[i - 1] = original;
	const struct pg_object *source_self = pg_evidence_context(pg_data_schema_parameters(s->input.schema))->binder;
	size_t ih_count = 0;
	for (size_t i = 0; i < count; ++i) {
		values[i] = pg_evidence_premise(fields, offset + i);
		if (!s->induction) continue;
		const struct pg_term *type = pg_evidence_context(extensions[i])->declared_type;
		int recursive = pg_data_recursive_field(type, source_self);
		if (!recursive) continue;
		const struct pg_term *content;
		if (recursive < 0 || pg_thunk_type_view(type, &content)) return -1;
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), projection(s, context, s->range));
		if (!context) return -1;
		const struct pg_evidence *output = pg_prove_variable(t, context, pg_evidence_context(context)->binder);
		const struct pg_evidence *relation = pg_prove_variable(t, context, pg_evidence_context(s->self)->binder);
		relation = pg_prove_family_application(t, relation, projection(s, context, values[i]));
		relation = pg_prove_family_application(t, relation, output);
		context = pg_prove_context_extension(t, context, pg_binder(t->graph), relation);
		if (!context) return -1;
		hypotheses[ih_count++] = pg_prove_thunk(t, s->classifiers,
			pg_prove_return(t, s->classifiers, projection(s, context, output)));
	}
	for (size_t i = 0; i < count; ++i) values[i] = projection(s, context, values[i]);
	s->branch_argument = pg_prove_constructor(t, s->input.formation, constructor,
		parameters_at(s, context), count, values);
	const struct pg_evidence *map = pg_prove_substitution_projection(t, s->context, context);
	map = pg_prove_substitution_pair(t, map, s->argument_context, s->branch_argument);
	s->branch = pg_prove_reindex(t, map, pg_evidence_premise(s->body, 5 + s->next));
	for (size_t i = 0; s->branch && i < count; ++i) s->branch = pg_prove_application(t, s->branch, values[i]);
	for (size_t i = 0; s->branch && i < ih_count; ++i)
		s->branch = pg_prove_application(t, s->branch, projection(s, context, hypotheses[i]));
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
	const struct pg_evidence *pi = pg_evidence_premise(function, 0);
	s->argument_context = pg_evidence_premise(pi, 1);
	s->context = pg_evidence_premise(s->argument_context, 0);
	s->domain = pg_evidence_premise(pi, 0);
	const struct pg_evidence *result = pg_prove_pi_constant_codomain(typing, pi);
	const struct pg_term *range;
	if (!result || !pg_return_type_view(pg_evidence_subject(result)->core, &range)) goto unsupported;
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
	if ((s->count && !s->results) || signature(s)) goto error;
	return 0;
unsupported:
	s->status = PG_FUNCTION_GRAPH_UNSUPPORTED;
	return 0;
error:
	s->status = PG_FUNCTION_GRAPH_ERROR;
	return 0;
}

enum pg_function_graph_status pg_function_graph_advance(struct pg_function_graph_work *work, uint64_t budget)
{
	if (!work || !work->state) return PG_FUNCTION_GRAPH_ERROR;
	struct pg_function_graph_state *s = work->state;
	while (s->status == PG_FUNCTION_GRAPH_PENDING && budget--) {
		if (s->next == s->count) {
			const struct pg_data_schema *schema = pg_data_schema(s->typing,
				pg_data_signature(s->typing, s->self, s->indices), s->count, s->results);
			s->formation = pg_prove_inductive_type(s->typing, s->classifiers, schema);
			s->status = s->formation ? PG_FUNCTION_GRAPH_DONE : PG_FUNCTION_GRAPH_UNSUPPORTED;
			break;
		}
		if (!s->branch) {
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

void pg_function_graph_destroy(struct pg_function_graph_work *work)
{
	if (!work || !work->state) return;
	pg_graph_destroy(&work->state->temporary);
	free(work->state);
	work->state = NULL;
}
