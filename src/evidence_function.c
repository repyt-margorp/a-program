#include "evidence_structure.h"
#include "scope.h"
#include "typed_query.h"
#include "iadt.h"

struct family_parameter {
	const struct pg_evidence *source, *environment, *map;
	const struct pg_scope *scope;
	struct pg_typing_wait *frames;
	size_t skip;
	unsigned stage;
};

static int family_parameter_step(struct pg_typed_query *work);
static void family_parameter_destroy(struct pg_typed_query *work)
{
	struct family_parameter *local = pg_typed_query_state(work);
	pg_typing_wait_clear(pg_typed_query_typing(work), &local->frames);
}

static const struct pg_typed_query_class FAMILY_PARAMETER[1] = {{
	.pending = {&pg_typed_query_pending_ops},
	.size = sizeof(struct family_parameter), .input_count = 1, .advance = family_parameter_step,
	.destroy = family_parameter_destroy}};

struct pg_typed_query *pg_family_parameter_request(struct pg_typing *typing,
	const struct pg_evidence *family, size_t ordinal)
{
	if (!pg_evidence_owned_by(family, typing)) return NULL;
	if (pg_evidence_judgement(family) != PG_JUDGEMENT_TYPE_FAMILY || ordinal == SIZE_MAX) return NULL;
	const void *inputs[] = {family};
	return pg_typed_query_request(typing, FAMILY_PARAMETER, ordinal, inputs, NULL);
}

static int parameter_telescope(struct pg_typed_query *work,
	const struct pg_evidence *indices, const struct pg_evidence *prefix)
{
	struct family_parameter *local = pg_typed_query_state(work);
	size_t count;
	if (!indices || !prefix) return -1;
	if (pg_context_extension_size(pg_evidence_context(indices), pg_evidence_context(prefix), &count)) return -1;
	if (work->ordinal >= count) return -1;
	local->scope = pg_evidence_scope(indices);
	local->skip = count - work->ordinal - 1;
	return 0;
}

static int parameter_select(struct pg_typed_query *work)
{
	struct family_parameter *local = pg_typed_query_state(work);
	struct pg_typing *typing = pg_typed_query_typing(work);
	const struct pg_evidence *source = local->source;
	const struct pg_occurrence *subject = pg_evidence_subject(source);
	const struct pg_evidence *parent, *prefix;
	switch (pg_evidence_rule(source)) {
	case PG_TYPE_FAMILY_ABSTRACT:
		if (!work->ordinal) local->scope = pg_evidence_binding_scope(source);
		else work->dependency = pg_family_parameter_request(typing,
			pg_prove_structural_subject(typing, subject->operands[0]), work->ordinal - 1);
		break;
	case PG_TYPE_FAMILY_APP:
		work->dependency = pg_family_parameter_request(typing,
			pg_prove_structural_subject(typing, subject->operands[0]), work->ordinal + 1);
		break;
	case PG_CONTEXT_PROJECTION: case PG_REINDEX: {
		const struct pg_evidence *input = pg_evidence_premise(source, 1);
		local->map = pg_evidence_premise(source, 0);
		if (pg_evidence_rule(source) == PG_CONTEXT_PROJECTION) {
			prefix = local->map;
			while (prefix && pg_evidence_context(prefix) != pg_evidence_context(input))
				prefix = pg_context_parent_input(typing, prefix);
			local->map = pg_prove_substitution_projection(typing, prefix, local->map);
		}
		if (!local->map) return -1;
		work->dependency = pg_family_parameter_request(typing, input, work->ordinal);
		break;
	}
	case PG_INDUCTIVE_FORM: {
		const struct pg_data_schema *schema = pg_evidence_inductive_schema(source);
		prefix = pg_evidence_premise(source, 0);
		parent = pg_context_parent_input(typing, prefix);
		local->map = pg_prove_substitution_pair(typing,
			pg_prove_substitution_projection(typing, parent, parent), prefix, source);
		if (!local->map) return -1;
		return parameter_telescope(work, pg_data_schema_indices(schema), prefix);
	}
	case PG_VARIABLE: {
		const struct pg_evidence *context = pg_evidence_premise(source, 0);
		const struct pg_scope *scope = pg_evidence_scope(context);
		while (scope && scope->context->binder != subject->core->as.reference) scope = scope->parent;
		if (!scope || !scope->indices) return -1;
		parent = pg_evidence_for_scope(typing, scope->parent);
		local->map = pg_prove_substitution_projection(typing, parent, context);
		if (!local->map) return -1;
		return parameter_telescope(work, pg_evidence_for_scope(typing, scope->indices), parent);
	}
	default: return -1;
	}
	return local->scope || work->dependency ? 0 : -1;
}

static int family_parameter_step(struct pg_typed_query *work)
{
	struct family_parameter *local = pg_typed_query_state(work);
	struct pg_typing *typing = pg_typed_query_typing(work);
	if (!local->stage) {
		local->source = work->inputs[0];
		/* Scope-sensitive inputs retain their selected formation. A search
		 * by occurrence alone may find another valid Universe bound. */
		switch (pg_evidence_rule(local->source)) {
		case PG_VARIABLE: case PG_INDUCTIVE_FORM: case PG_TYPE_FAMILY_ABSTRACT:
		case PG_TYPE_FAMILY_APP: case PG_CONTEXT_PROJECTION: case PG_REINDEX:
			local->stage = 2;
			return parameter_select(work);
		default: break;
		}
		work->dependency = pg_construction_origin_request(typing, work->inputs[0]);
		local->stage = 1;
		return work->dependency ? 0 : -1;
	}
	if (local->stage == 1) {
		local->source = pg_typed_query_result(work->dependency);
		if (!local->source) return -1;
		local->environment = pg_construction_origin_environment(work->dependency);
		work->dependency = NULL;
		local->stage = 2;
		return parameter_select(work);
	}
	if (local->stage == 2) {
		if (work->dependency) {
			const struct pg_evidence *scope = pg_typed_query_result(work->dependency);
			if (!scope) return -1;
			local->scope = pg_evidence_scope(scope);
			work->dependency = NULL;
			if (pg_evidence_rule(local->source) == PG_TYPE_FAMILY_APP) {
				/* Consume precisely the first parameter. Later declarations are
				 * transported by the same checked scope action as every map. */
				const struct pg_occurrence *source = pg_evidence_subject(local->source);
				const struct pg_scope *first = local->scope;
				while (first && first->context->parent != source->context) first = first->parent;
				if (!first) return -1;
				const struct pg_evidence *parent = pg_evidence_for_scope(typing, first->parent);
				local->map = pg_prove_substitution_pair(typing,
					pg_prove_substitution_projection(typing, parent, parent),
					pg_evidence_for_scope(typing, first),
					pg_prove_structural_subject(typing, source->operands[1]));
				if (!local->map) return -1;
			}
		}
		if (local->skip) {
			local->scope = local->scope->parent;
			--local->skip;
			return 0;
		}
		if (!local->scope) return -1;
		if (!local->map) {
			if (!local->environment) {
				work->result = pg_evidence_for_scope(typing, local->scope);
				return work->result ? 1 : -1;
			}
			local->map = local->environment;
		} else if (local->environment) {
			work->dependency = pg_substitution_compose_request(typing, local->map, local->environment);
			if (!work->dependency) return -1;
		}
		local->stage = 3;
		return 0;
	}
	if (local->stage == 3) {
		if (work->dependency) {
			local->map = pg_typed_query_result(work->dependency);
			work->dependency = NULL;
			if (!local->map) return -1;
		}
		if (local->scope && local->scope->context != pg_evidence_context_map(local->map)->source) {
			if (pg_typing_wait_push(typing, &local->frames, (void *)local->scope)) return -1;
			local->scope = local->scope->parent;
			return 0;
		}
		if ((local->scope ? local->scope->context : NULL) != pg_evidence_context_map(local->map)->source) return -1;
		local->stage = 4;
	}
	if (work->dependency) {
		local->map = pg_typed_query_result(work->dependency);
		work->dependency = NULL;
		if (!local->map) return -1;
		pg_typing_wait_pop(typing, &local->frames);
	}
	if (local->frames) {
		const struct pg_scope *scope = local->frames->work;
		const struct pg_object *binder = scope->context->binder;
		if (pg_context_lookup(pg_evidence_context(local->map), binder)) binder = pg_binder(typing->graph);
		work->dependency = pg_substitution_lift_request(typing, local->map,
			pg_evidence_for_scope(typing, scope), binder);
		return work->dependency ? 0 : -1;
	}
	work->result = pg_evidence_premise(local->map, 1);
	return work->result ? 1 : -1;
}

size_t pg_function_proof_inputs(const struct pg_evidence *proof,
	const struct pg_occurrence **inputs)
{
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	switch (pg_evidence_rule(proof)) {
	case PG_PI_DOMAIN: case PG_PI_CONSTANT_CODOMAIN:
		inputs[0] = subject->origin;
		return 1;
	case PG_PI_CODOMAIN:
		inputs[0] = subject->origin;
		inputs[1] = subject->operands[0];
		return 2;
	case PG_LAMBDA_INTRO:
		inputs[0] = subject->type;
		inputs[1] = subject->operands[0];
		return 2;
	case PG_APP_ELIM: case PG_TYPE_FAMILY_APP:
		inputs[0] = subject->operands[0];
		inputs[1] = subject->operands[1];
		return 2;
	default: return 0;
	}
}

const struct pg_evidence *pg_function_selection(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child)
{
	if (!subject->origin || !subject->selection || subject->selection > 2) return NULL;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (!pg_pi_view(subject->origin->core, &domain, &binder, &body)) return NULL;
	if (subject->operand_count > 1 || (subject->selection == 1 && subject->operand_count)) return NULL;
	const struct pg_evidence *pi = pg_structure_input(typing, subject->origin, child);
	if (!pi) return NULL;
	if (subject->selection == 1) return pg_prove_pi_domain(typing, pi);
	if (!subject->operand_count) return pg_prove_pi_constant_codomain(typing, pi);
	const struct pg_evidence *argument = pg_structure_input(typing, subject->operands[0], child);
	return argument ? pg_check_pi_codomain(typing, pi, argument, subject) : NULL;
}

/* A binding retains its body and declared type (a terminal Universe for a
 * family). Further inputs are index formations in telescope order, descending
 * into a nested family before its terminal Universe. Pi keeps its body at 1,
 * Lambda at 0. A raw family signature is not treated as a value type. */
struct declaration_input {
	const struct pg_scope *scope;
	const struct pg_occurrence *type;
	struct declaration_input *next;
};

static int push_input(struct pg_graph *scratch, struct declaration_input **stack,
	const struct pg_scope *scope, const struct pg_occurrence *type)
{
	struct declaration_input *entry = pg_alloc(scratch, sizeof(*entry));
	if (!entry) return -1;
	*entry = (struct declaration_input){scope, type, *stack};
	*stack = entry;
	return 0;
}

static const struct pg_occurrence *binding_term(struct pg_typing *typing,
	const struct pg_context *context, enum pg_evidence_judgement judgement, uint64_t bound, size_t count,
	const struct pg_occurrence *const *operands)
{
	const struct pg_term *core, *classifier;
	if (judgement == PG_JUDGEMENT_TYPE_FAMILY) {
		core = pg_lambda(typing->graph, context->binder, operands[0]->core);
		classifier = pg_pi(typing->graph, context->declared_type, context->binder, operands[0]->classifier);
	} else {
		core = pg_pi(typing->graph, context->declared_type, context->binder, operands[1]->core);
		classifier = pg_universe(typing->graph, bound);
	}
	return core && classifier ? pg_occurrence(typing,
		judgement, context->parent, core, classifier, NULL, count, operands) : NULL;
}

const struct pg_occurrence *pg_function_binding(struct pg_typing *typing,
	const struct pg_evidence *declaration, const struct pg_occurrence *body, enum pg_evidence_judgement judgement)
{
	const struct pg_scope *scope = pg_evidence_scope(declaration);
	if (!scope || !body) return NULL;
	const struct pg_context *context = scope->context;
	uint64_t bound = 0;
	size_t body_index;
	if (judgement == PG_JUDGEMENT_COMPUTATION_TYPE) {
		if (!pg_universe_level(body->classifier, &bound)) return NULL;
		body_index = 1;
	} else if (judgement == PG_JUDGEMENT_TYPE_FAMILY) body_index = 0;
	else return NULL;
	if (!scope->indices) {
		const struct pg_occurrence *domain = scope->type;
		uint64_t level;
		if (!domain || !pg_universe_level(domain->classifier, &level)) return NULL;
		const struct pg_occurrence *operands[2];
		operands[body_index] = body;
		operands[1 - body_index] = domain;
		return binding_term(typing, context, judgement, level > bound ? level : bound, 2, operands);
	}
	struct pg_graph scratch = {0};
	struct declaration_input first = {scope, NULL, NULL}, *stack = &first, *inputs = NULL;
	size_t count = 0;
	const struct pg_occurrence *result = NULL;
	while (stack) {
		const struct pg_scope *input_scope = stack->scope;
		const struct pg_occurrence *type = stack->type;
		stack = stack->next;
		if (input_scope) {
			if (push_input(&scratch, &stack, NULL, input_scope->type)) goto done;
			if (input_scope->indices) {
				for (const struct pg_scope *indices = input_scope->indices;
					indices && indices->context != input_scope->context->parent; indices = indices->parent)
					if (push_input(&scratch, &stack, indices, NULL)) goto done;
			}
			continue;
		}
		uint64_t level;
		if (!type || !pg_universe_level(type->classifier, &level)) goto done;
		if (level > bound) bound = level;
		if (count == SIZE_MAX || push_input(&scratch, &inputs, NULL, type)) goto done;
		++count;
	}
	if (!count || count == SIZE_MAX) goto done;
	size_t arity = count + 1;
	if (arity > SIZE_MAX / sizeof(const struct pg_occurrence *)) goto done;
	const struct pg_occurrence **operands = pg_alloc(&scratch, arity * sizeof(*operands));
	if (!operands) goto done;
	operands[1 - body_index] = inputs->type;
	inputs = inputs->next;
	operands[body_index] = body;
	for (size_t i = count - 1; i; --i, inputs = inputs->next)
		operands[i + 1] = inputs->type;
	result = binding_term(typing, context, judgement, bound, arity, operands);
done:
	pg_graph_destroy(&scratch);
	return result;
}

struct declaration_scope {
	const struct pg_context *context;
	const struct pg_evidence *parent;
	struct declaration_scope *next;
};

static const struct pg_evidence *family_scope(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_occurrence *subject, size_t domain_index,
	const struct pg_context *scope, const struct pg_occurrence **child)
{
	struct pg_graph scratch = {0};
	struct declaration_scope first = {scope, NULL, NULL}, *stack = &first;
	const struct pg_evidence *current = parent;
	size_t index = 2;
	while (stack) {
		struct declaration_scope *frame = stack;
		const struct pg_context *declaration = frame->context;
		if (!frame->parent && declaration->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
			if (pg_evidence_context(current) != declaration->parent) goto fail;
			frame->parent = current;
			size_t count;
			if (pg_context_extension_size(declaration->indices, declaration->parent, &count) || !count) goto fail;
			for (const struct pg_context *at = declaration->indices; at != declaration->parent; at = at->parent) {
				struct declaration_scope *next = pg_alloc(&scratch, sizeof(*next));
				if (!next) goto fail;
				*next = (struct declaration_scope){at, NULL, stack};
				stack = next;
			}
			continue;
		}
		if (frame != &first && index == subject->operand_count) goto fail;
		size_t selected = frame == &first ? domain_index : index++;
		const struct pg_evidence *type = pg_structure_input(typing, subject->operands[selected], child);
		if (!type) goto fail;
		current = frame->parent
			? pg_prove_family_context_extension(typing, frame->parent, declaration->binder, current, type)
			: pg_prove_context_extension(typing, current, declaration->binder, type);
		if (!current || pg_evidence_context(current) != declaration) goto fail;
		stack = frame->next;
	}
	pg_graph_destroy(&scratch);
	return index == subject->operand_count ? current : NULL;
fail:
	pg_graph_destroy(&scratch);
	return NULL;
}

static const struct pg_evidence *binding_scope(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_occurrence *subject,
	size_t body_index, const struct pg_occurrence **child)
{
	if (subject->operand_count < 2) return NULL;
	const struct pg_occurrence *body = pg_occurrence_scoped_input(subject, body_index);
	if (!body) return NULL;
	const struct pg_evidence *scope;
	/* Selected domain inputs, not the first admission of this raw Context. */
	if (body->context->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
		scope = family_scope(typing, context, subject, 1 - body_index, body->context, child);
	} else {
		if (subject->operand_count != 2) return NULL;
		const struct pg_evidence *type = pg_structure_input(typing, subject->operands[1 - body_index], child);
		if (!type) return NULL;
		scope = pg_prove_context_extension(typing, context, body->context->binder, type);
	}
	return scope && pg_evidence_context(scope) == body->context ? scope : NULL;
}

const struct pg_evidence *pg_function_structure(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_occurrence *subject,
	const struct pg_occurrence **child)
{
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION_TYPE &&
		pg_pi_view(subject->core, &domain, &binder, &codomain)) {
		const struct pg_evidence *scope = binding_scope(typing, context, subject, 1, child);
		if (!scope) return NULL;
		const struct pg_evidence *result = pg_structure_input(typing, subject->operands[1], child);
		return result ? pg_prove_pi(typing, scope, result) : NULL;
	}
	if (subject->core->kind == PG_LAMBDA) {
		if (subject->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
			const struct pg_evidence *scope = binding_scope(typing, context, subject, 0, child);
			if (!scope) return NULL;
			const struct pg_evidence *body = pg_structure_input(typing, subject->operands[0], child);
			return body ? pg_prove_family_abstraction(typing, scope, body) : NULL;
		}
		if (subject->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
		if (subject->operand_count != 1 || !subject->type) return NULL;
		const struct pg_evidence *type = pg_structure_input(typing, subject->type, child);
		if (!type) return NULL;
		const struct pg_evidence *body = pg_structure_input(typing, subject->operands[0], child);
		return body ? pg_prove_lambda(typing, type, body) : NULL;
	}
	if (subject->core->kind != PG_APPLICATION || subject->operand_count != 2) return NULL;
	const struct pg_evidence *function = pg_structure_input(typing, subject->operands[0], child);
	if (!function) return NULL;
	const struct pg_evidence *argument = pg_structure_input(typing, subject->operands[1], child);
	if (pg_evidence_judgement(function) == PG_JUDGEMENT_TYPE_FAMILY)
		return argument ? pg_check_family_application(typing, function, argument, subject) : NULL;
	if (subject->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	return argument ? pg_check_application(typing, function, argument, subject) : NULL;
}
