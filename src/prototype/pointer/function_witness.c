#include "function_graph_internal.h"
#include "function_witness.h"
#include "computation.h"

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
	const struct pg_evidence *output = pg_prove_context_extension(t, self, pg_binder(t->graph), pg_function_plan_projection(s, self, s->range));
	if (!output) return -1;
	const struct pg_evidence *relation = pg_function_plan_projection(s, output, s->declaration);
	relation = pg_function_plan_apply_relation(s, relation, pg_prove_substitution_projection(t, s->result_context, output),
		pg_prove_variable(t, output, pg_evidence_context(output)->binder));
	const struct pg_evidence *fields = pg_prove_context_extension(t, output, pg_binder(t->graph), relation);
	const struct pg_evidence *result = pg_prove_substitution_projection(t, self, fields);
	const struct pg_data_schema *schema = pg_data_schema(t, pg_data_signature(t, self, self), 1, &result);
	s->packet = pg_prove_inductive_type(t, schema);
	return s->packet ? 0 : -1;
}

static const struct pg_evidence *return_packet(struct pg_function_graph_state *s, size_t index,
	const struct pg_evidence *instance)
{
	struct pg_typing *t = s->typing;
	const struct pg_evidence *context = pg_evidence_premise(instance, 1);
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(s->schema), index);
	const struct pg_evidence *graph = pg_prove_constructor_instance(t, s->declaration, constructor,
		pg_prove_substitution_projection(t, s->context, context), instance);
	if (!graph) return NULL;
	const struct pg_evidence *result = pg_data_result(t, s->schema, constructor, pg_evidence_premise(graph, 3));
	if (!result) return NULL;
	size_t n = pg_evidence_context_map(result)->count;
	if (n < s->arity + 2) return NULL;
	const struct pg_evidence *const *images = pg_substitution_images(t, result, &s->temporary);
	if (!images) return NULL;
	const struct pg_evidence *input = images[n - s->arity - 2];
	const struct pg_evidence *values[] = {images[n - 1], graph};
	const struct pg_evidence *const *arguments = images + n - s->arity - 1;
	const struct pg_evidence *packet = pg_prove_constructor(t, s->packet, packet_constructor(s),
		pg_function_plan_input_substitution(s, context, input, arguments), 2, values);
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
		f->target = pg_function_plan_projection(s, context, work->target);
		if (call->helper) {
			f->input = pg_function_plan_helper_function(s, call, work->source_map, pg_function_graph_witness(call->helper));
			for (size_t i = 0; f->input && i < call->helper_arity; ++i)
				f->input = pg_prove_application(t, f->input, pg_function_plan_map_value(s, work->source_map, call->helper_arguments[i]));
		} else {
			if (call->hypothesis >= plan->hypothesis_count) return -1;
			f->input = pg_prove_force(t, pg_function_plan_projection(s, context, work->hypotheses[call->hypothesis]));
			for (size_t i = 0; f->input && i < call->field_arity; ++i)
				f->input = pg_prove_application(t, f->input, pg_function_plan_map_value(s, work->source_map, call->field_arguments[i]));
			for (size_t i = 0; i < s->arity; ++i) {
				arguments[i] = pg_function_plan_map_value(s, work->source_map, call->arguments[i]);
				f->input = pg_prove_application(t, f->input, arguments[i]);
			}
		}
		const struct pg_evidence *packet = pg_prove_return_content(t, pg_prove_classifier(t, context, f->input));
		f->bound = pg_prove_context_extension(t, context, pg_binder(t->graph),
			packet);
		if (!f->bound || !f->input || !f->target) return -1;
		f->value = pg_prove_variable(t, f->bound, pg_evidence_context(f->bound)->binder);
		struct pg_inductive_instance instance;
		if (!pg_inductive_instance(t, pg_function_plan_projection(s, f->bound, packet), &instance)) return -1;
		f->formation = instance.formation;
		f->constructor = pg_data_constructor(pg_data_schema_layout(instance.schema), 0);
		f->parameters = instance.parameters;
		f->fields = pg_prove_constructor_scope(t, f->formation, f->constructor, f->parameters);
		if (!f->fields) return -1;
		context = pg_evidence_premise(f->fields, 1);
		size_t start = pg_evidence_context_map(f->parameters)->count + 1;
		work->values[2 * entry->slot] = pg_substitution_image_at(t, f->fields, start);
		work->values[2 * entry->slot + 1] = pg_substitution_image_at(t, f->fields, start + 1);
		work->source_map = pg_prove_substitution_compose(t, work->source_map,
			pg_prove_substitution_projection(t, pg_evidence_premise(work->source_map, 1), context));
		work->source_map = pg_prove_substitution_pair(t, work->source_map, call->result_context,
			pg_substitution_image_at(t, f->fields, start));
		if (!work->source_map) return -1;
	}
	for (size_t i = 0; i < 2 * calls; ++i) work->values[i] = pg_function_plan_projection(s, context, work->values[i]);
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
				work->context, pg_function_plan_map_value(s, work->source_map, plan->discriminant), branch->constructor);
			if (!refinement) return NULL;
			work->refinements[work->next] = refinement;
			struct witness_branch *child = pg_alloc(&s->temporary, sizeof(*child));
			if (!child) return NULL;
			*child = (struct witness_branch){.plan = branch, .parent = work};
			child->source_map = pg_prove_refinement_factor(t, branch->refinement,
				pg_prove_substitution_compose(t, work->source_map, refinement),
				pg_evidence_subject(plan->discriminant)->core->as.reference);
			const struct pg_evidence *schema_value = pg_function_plan_map_value(s, plan->schema_map, plan->discriminant);
			if (!schema_value) return NULL;
			child->schema_map = pg_prove_refinement_factor(t, branch->schema_refinement,
				pg_prove_substitution_compose(t, work->schema_map, refinement),
				pg_evidence_subject(schema_value)->core->as.reference);
			if (!child->source_map || !child->schema_map) return NULL;
			child->hypotheses = pg_alloc(&s->temporary, plan->hypothesis_count * sizeof(*child->hypotheses));
			if (plan->hypothesis_count && !child->hypotheses) return NULL;
			for (size_t i = 0; i < plan->hypothesis_count; ++i) {
				child->hypotheses[i] = pg_function_plan_map_value(s, refinement, work->hypotheses[i]);
				if (!child->hypotheses[i]) return NULL;
			}
			work = child;
			continue;
		}
		const struct pg_evidence *body;
		if (plan->discriminant) {
			body = pg_prove_refined_match(t, work->context,
				pg_function_plan_map_value(s, work->source_map, plan->discriminant), pg_function_plan_projection(s, work->context, work->target),
				plan->child_count, work->refinements, work->branches);
		} else {
			body = return_packet(s, plan->leaf, work->schema_map);
		}
		for (size_t i = plan->call_count - plan->first_call; body && i; --i) {
			const struct packet_frame *f = &work->frames[i - 1];
			body = pg_prove_abstract(t, f->bound, pg_evidence_premise(f->fields, 1), body);
			const struct pg_evidence *mc = pg_prove_inductive_motive_context(t, f->formation, f->parameters, pg_binder(t->graph));
			body = pg_prove_match(t, f->formation, f->parameters, f->value, mc,
				pg_function_plan_projection(s, mc, f->target), 1, &body);
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
			pg_prove_substitution_projection(t, s->context, context), s->self, pg_function_plan_projection(s, context, s->declaration));
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
	const struct pg_evidence *parameters = pg_function_plan_parameters_at(s, s->argument_context);
	const struct pg_evidence *map = s->induction
		? pg_prove_induction_scope(t, s->input.formation, constructor, parameters, s->motive_context, s->motive)
		: pg_prove_constructor_scope(t, s->input.formation, constructor, parameters);
	if (!map) return NULL;
	const struct pg_evidence *base = pg_evidence_premise(map, 1), *context = base;
	const struct pg_evidence *original = pg_data_schema_fields(s->input.schema, constructor);
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(original),
		pg_evidence_context(pg_data_schema_parameters(s->input.schema)), &count)) return NULL;
	size_t offset = pg_evidence_context_map(parameters)->count + 1;
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
	for (size_t i = 0; i < count; ++i) values[i] = pg_substitution_image_at(t, map, offset + i);
	const struct pg_evidence *argument = pg_prove_constructor_instance(t, s->input.formation, constructor,
		pg_function_plan_parameters_at(s, base), map);
	if (!argument) return NULL;
	const struct pg_evidence *source_map = pg_function_plan_case_source_map(s, plan, context, argument, values, values + count);
	if (!source_map) return NULL;
	context = pg_evidence_premise(source_map, 1);
	const struct pg_evidence *arguments_context = context;
	for (size_t i = 0; i < count + s->arity; ++i) values[i] = pg_function_plan_projection(s, context, values[i]);
	const struct pg_evidence *schema_map = pg_prove_substitution_pair(t,
		pg_prove_substitution_projection(t, s->context, context), s->self, pg_function_plan_projection(s, context, s->declaration));
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
					pg_function_plan_parameters_at(s, s->argument_context), pg_binder(s->typing->graph));
				if (!s->motive_context) goto unsupported;
				const struct pg_evidence *z = pg_prove_variable(s->typing, s->motive_context,
					pg_evidence_context(s->motive_context)->binder);
				const struct pg_evidence *map = pg_function_plan_argument_substitution(s, s->motive_context, z);
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
			pg_function_plan_parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		else body = pg_prove_match(s->typing, s->input.formation,
			pg_function_plan_parameters_at(s, s->argument_context), argument, s->motive_context, s->motive, s->count, s->witness_branches);
		const struct pg_evidence *context = s->argument_context;
		if (s->specialization) {
			body = pg_prove_abstract(s->typing, s->context, context, body);
			size_t end = pg_evidence_context_map(s->specialization)->count;
			for (size_t i = end - s->index_count - 1 - s->specialized_arity; body && i < end; ++i)
				body = pg_prove_application(s->typing, body, pg_substitution_image_at(s->typing, s->specialization, i));
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
