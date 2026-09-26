#include "evidence_structure.h"

/* A binding retains its body and declared type (a terminal Universe for a
 * family). Further inputs are index formations in telescope order, descending
 * into a nested family before its terminal Universe. Pi keeps its body at 1,
 * Lambda at 0. A raw family signature is not treated as a value type. */
struct declaration_input {
	const struct pg_evidence *proof;
	struct declaration_input *next;
};

static int push_input(struct pg_graph *scratch, struct declaration_input **stack,
	const struct pg_evidence *proof)
{
	struct declaration_input *entry = pg_alloc(scratch, sizeof(*entry));
	if (!entry) return -1;
	*entry = (struct declaration_input){proof, *stack};
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
	const struct pg_evidence *scope, const struct pg_occurrence *body, enum pg_evidence_judgement judgement)
{
	const struct pg_context *context = pg_evidence_context(scope);
	if (!context || !body) return NULL;
	uint64_t bound = 0;
	size_t body_index;
	if (judgement == PG_JUDGEMENT_COMPUTATION_TYPE) {
		if (!pg_universe_level(body->classifier, &bound)) return NULL;
		body_index = 1;
	} else if (judgement == PG_JUDGEMENT_TYPE_FAMILY) body_index = 0;
	else return NULL;
	if (pg_evidence_rule(scope) == PG_CONTEXT_EXTEND) {
		const struct pg_occurrence *domain = pg_evidence_subject(pg_evidence_premise(scope, 1));
		uint64_t level;
		if (!domain || !pg_universe_level(domain->classifier, &level)) return NULL;
		const struct pg_occurrence *operands[2];
		operands[body_index] = body;
		operands[1 - body_index] = domain;
		return binding_term(typing, context, judgement, level > bound ? level : bound, 2, operands);
	}
	struct pg_graph scratch = {0};
	struct declaration_input first = {scope, NULL}, *stack = &first, *inputs = NULL;
	size_t count = 0;
	const struct pg_occurrence *result = NULL;
	while (stack) {
		const struct pg_evidence *proof = stack->proof;
		stack = stack->next;
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT) {
			if (pg_evidence_rule(proof) == PG_CONTEXT_EXTEND) {
				if (push_input(&scratch, &stack, pg_evidence_premise(proof, 1))) goto done;
			} else if (pg_evidence_rule(proof) == PG_CONTEXT_FAMILY_EXTEND) {
				if (push_input(&scratch, &stack, pg_evidence_premise(proof, 2))) goto done;
				const struct pg_context *parent = pg_evidence_context(proof)->parent;
				for (const struct pg_evidence *indices = pg_evidence_premise(proof, 1);
					pg_evidence_context(indices) != parent; indices = pg_evidence_premise(indices, 0)) {
					if (!indices || push_input(&scratch, &stack, indices)) goto done;
				}
			} else goto done;
			continue;
		}
		uint64_t level;
		if (!pg_universe_level(pg_evidence_classifier(proof), &level)) goto done;
		if (level > bound) bound = level;
		if (count == SIZE_MAX || push_input(&scratch, &inputs, proof)) goto done;
		++count;
	}
	if (!count || count == SIZE_MAX) goto done;
	size_t arity = count + 1;
	if (arity > SIZE_MAX / sizeof(const struct pg_occurrence *)) goto done;
	const struct pg_occurrence **operands = pg_alloc(&scratch, arity * sizeof(*operands));
	if (!operands) goto done;
	operands[1 - body_index] = pg_evidence_subject(inputs->proof);
	inputs = inputs->next;
	operands[body_index] = body;
	for (size_t i = count - 1; i; --i, inputs = inputs->next)
		operands[i + 1] = pg_evidence_subject(inputs->proof);
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
