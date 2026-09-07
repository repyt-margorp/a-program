#include "evidence.h"
#include "computation.h"
#include "eval.h"
#include "identity.h"
#include <stdlib.h>

struct pg_evidence {
	struct pg_index_entry index;
	const struct pg_typing *owner;
	enum pg_evidence_rule rule;
	enum pg_evidence_judgement judgement;
	const struct pg_context *context;
	const struct pg_occurrence *subject;
	const struct pg_term *classifier;
	const struct pg_conversion_certificate *conversion;
	size_t premise_count;
	const struct pg_evidence *premises[];
};

static int derived_output(enum pg_evidence_rule rule)
{
	switch (rule) {
	case PG_REINDEX: case PG_APP_ELIM: case PG_PI_CODOMAIN:
	case PG_FOLD_ELIM: case PG_PI_CONSTANT_CODOMAIN:
		return 1;
	default: return 0;
	}
}

static const struct pg_evidence *find_record(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion, uint64_t *hash_out)
{
	/* For these rules, immutable premises determine the output. Check this key
	 * before substitution or independence checks allocate temporary binders. */
	if (derived_output(rule)) { subject = NULL; classifier = NULL; }
	uint64_t hash = ((uintptr_t)context ^ (uintptr_t)subject ^ (uintptr_t)classifier ^ rule) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	*hash_out = hash;
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->proofs, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_evidence *proof = (const struct pg_evidence *)candidate;
		if (proof->rule != rule) continue;
		if (proof->judgement != judgement) continue;
		if (proof->context != context) continue;
		if (!derived_output(rule)) {
			if (proof->subject != subject) continue;
			if (proof->classifier != classifier) continue;
		}
		if (proof->conversion != conversion) continue;
		if (proof->premise_count != count) continue;
		size_t i = 0;
		while (i < count && proof->premises[i] == premises[i]) ++i;
		if (i == count) return proof;
	}
	return NULL;
}

static const struct pg_evidence *accept_record(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion,
	size_t binding_count, const struct pg_binding_value *bindings)
{
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, rule, judgement, context,
		subject, classifier, count, premises, conversion, &hash);
	if (existing) return existing;
	if (count > (SIZE_MAX - sizeof(struct pg_evidence)) / sizeof(*premises)) return NULL;
	size_t size = sizeof(struct pg_evidence) + count * sizeof(*premises);
	if (binding_count > (SIZE_MAX - size) / sizeof(*bindings)) return NULL;
	struct pg_evidence *proof = pg_alloc(typing->graph, size + binding_count * sizeof(*bindings));
	if (!proof) return NULL;
	proof->owner = typing;
	proof->rule = rule;
	proof->judgement = judgement;
	proof->context = context;
	proof->subject = subject;
	proof->classifier = classifier;
	proof->conversion = conversion;
	proof->premise_count = count;
	for (size_t i = 0; i < count; ++i) proof->premises[i] = premises[i];
	struct pg_binding_value *mapping = (struct pg_binding_value *)(proof->premises + count);
	for (size_t i = 0; i < binding_count; ++i) mapping[i] = bindings[i];
	if (pg_index_insert(&typing->proofs, &proof->index, hash) != 0) return NULL;
	return proof;
}

static const struct pg_evidence *accept_with_conversion(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion)
{
	return accept_record(typing, rule, judgement, context, subject, classifier, count, premises, conversion, 0, NULL);
}

static const struct pg_evidence *accept(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises)
{
	return accept_with_conversion(typing, rule, judgement, context, subject, classifier, count, premises, NULL);
}

static int context_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return 0;
	return proof->judgement == PG_JUDGEMENT_CONTEXT;
}

const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing)
{
	return accept(typing, PG_CONTEXT_EMPTY, PG_JUDGEMENT_CONTEXT, NULL, NULL, NULL, 0, NULL);
}

const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!value || value->owner != typing) return NULL;
	if (value->judgement == PG_JUDGEMENT_VALUE_TYPE) return value;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	uint64_t level;
	if (!pg_universe_level(value->classifier, &level)) return NULL;
	return accept(typing, PG_TYPE_FROM_VALUE, PG_JUDGEMENT_VALUE_TYPE,
		value->context, value->subject, value->classifier, 1, &value);
}

const struct pg_evidence *pg_prove_type_value(struct pg_typing *typing, const struct pg_evidence *type)
{
	if (!type || type->owner != typing) return NULL;
	if (type->judgement != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	return accept(typing, PG_VALUE_FROM_TYPE, PG_JUDGEMENT_VALUE,
		type->context, type->subject, type->classifier, 1, &type);
}

const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type)
{
	if (!context_proof(typing, parent)) return NULL;
	type = pg_prove_value_type(typing, type);
	if (!type) return NULL;
	if (!type->subject || type->context != parent->context) return NULL;
	uint64_t level;
	if (!pg_universe_level(type->classifier, &level)) return NULL;
	if (pg_context_lookup(parent->context, binder)) return NULL;
	const struct pg_context *context = pg_context_bind(typing, parent->context, binder, type->subject->core);
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, type};
	return accept(typing, PG_CONTEXT_EXTEND, PG_JUDGEMENT_CONTEXT, context, NULL, NULL, 2, premises);
}

const struct pg_evidence *pg_prove_universe(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context, uint64_t level)
{
	if (!context_proof(typing, context)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	if (level == UINT64_MAX) return NULL;
	const struct pg_term *term = pg_universe(classifiers, level);
	const struct pg_term *sort = pg_universe(classifiers, level + 1);
	if (!term || !sort) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context, term, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_UNIVERSE_FORM, PG_JUDGEMENT_VALUE_TYPE, context->context, subject, sort, 1, &context);
}

const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder)
{
	if (!context_proof(typing, context)) return NULL;
	const struct pg_context *declaration = pg_context_lookup(context->context, binder);
	if (!declaration) return NULL;
	const struct pg_term *term = pg_reference(typing->graph, binder);
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context, term, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_VARIABLE, PG_JUDGEMENT_VALUE, context->context, subject, declaration->declared_type, 1, &context);
}

static const struct pg_evidence *unary_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *argument,
	enum pg_evidence_rule rule)
{
	if (!argument || argument->owner != typing) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	enum pg_evidence_judgement output;
	const struct pg_term *term;
	if (rule == PG_RETURN_TYPE_FORM) {
		argument = pg_prove_value_type(typing, argument);
		if (!argument) return NULL;
		output = PG_JUDGEMENT_COMPUTATION_TYPE;
		term = pg_return_type(classifiers, argument->subject->core);
	} else {
		if (argument->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		output = PG_JUDGEMENT_VALUE_TYPE;
		term = pg_thunk_type(classifiers, argument->subject->core);
	}
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, argument->context,
		term, NULL, 1, &argument->subject);
	if (!subject) return NULL;
	return accept(typing, rule, output, argument->context, subject,
		argument->classifier, 1, &argument);
}

const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value_type)
{
	return unary_formation(typing, classifiers, value_type, PG_RETURN_TYPE_FORM);
}

static int endpoint(const struct pg_typing *typing, const struct pg_evidence *term,
	enum pg_evidence_judgement judgement, const struct pg_context *context,
	const struct pg_term *type)
{
	if (!term || term->owner != typing) return 0;
	if (term->judgement != judgement) return 0;
	if (term->context != context) return 0;
	return pg_alpha_equal(term->classifier, type) == 1;
}

const struct pg_evidence *pg_prove_identity_type(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *left,
	const struct pg_evidence *right)
{
	if (!type || type->owner != typing) return NULL;
	enum pg_evidence_judgement elements;
	switch (type->judgement) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!endpoint(typing, left, elements, type->context, type->subject->core)) return NULL;
	if (!endpoint(typing, right, elements, type->context, type->subject->core)) return NULL;
	const struct pg_term *family = pg_identity_action(typing->graph, type->subject->core);
	const struct pg_term *core = pg_identity_instance(typing->graph, family,
		left->subject->core, right->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {type->subject, left->subject, right->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, type->context, core, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {type, left, right};
	return accept(typing, PG_IDENTITY_FORM, type->judgement, type->context,
		subject, type->classifier, 3, premises);
}

const struct pg_evidence *pg_prove_identity_instance(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (classifiers->graph != typing->graph) return NULL;
	if (!family || family->owner != typing) return NULL;
	if (family->judgement != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_term *universe, *left_type, *right_type;
	if (!pg_identity_view(family->classifier, &universe, &left_type, &right_type)) return NULL;
	uint64_t level;
	if (!pg_universe_level(universe, &level)) return NULL;
	if (!endpoint(typing, left, PG_JUDGEMENT_VALUE, family->context, left_type)) return NULL;
	if (!endpoint(typing, right, PG_JUDGEMENT_VALUE, family->context, right_type)) return NULL;
	const struct pg_term *core = pg_identity_instance(typing->graph, family->subject->core,
		left->subject->core, right->subject->core);
	if (!core) return NULL;
	const struct pg_term *sort = pg_universe(classifiers, level);
	if (!sort) return NULL;
	const struct pg_occurrence *operands[] = {family->subject, left->subject, right->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context, core, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {family, left, right};
	return accept(typing, PG_IDENTITY_INSTANCE, PG_JUDGEMENT_VALUE_TYPE, family->context,
		subject, sort, 3, premises);
}

const struct pg_evidence *pg_prove_reflexivity(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *term)
{
	const struct pg_evidence *identity = pg_prove_identity_type(typing, type, term, term);
	if (!identity) return NULL;
	const struct pg_term *core = pg_identity_action(typing->graph, term->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, term->context, core, NULL, 1, &term->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {identity, term};
	return accept(typing, PG_REFLEXIVITY, term->judgement, term->context,
		subject, identity->subject->core, 2, premises);
}

const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation_type)
{
	return unary_formation(typing, classifiers, computation_type, PG_THUNK_TYPE_FORM);
}

const struct pg_evidence *pg_prove_pi(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *domain, const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain)
{
	if (classifiers->graph != typing->graph) return NULL;
	domain = pg_prove_value_type(typing, domain);
	if (!domain) return NULL;
	if (!context_proof(typing, extended_context)) return NULL;
	const struct pg_context *scope = extended_context->context;
	if (!scope || scope->parent != domain->context) return NULL;
	if (scope->declared_type != domain->subject->core) return NULL;
	if (!codomain || codomain->owner != typing) return NULL;
	if (codomain->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (codomain->context != scope) return NULL;
	uint64_t left, right;
	if (!pg_universe_level(domain->classifier, &left)) return NULL;
	if (!pg_universe_level(codomain->classifier, &right)) return NULL;
	const struct pg_term *bound = pg_universe(classifiers, left > right ? left : right);
	const struct pg_term *term = pg_pi(classifiers, domain->subject->core,
		scope->binder, codomain->subject->core);
	if (!bound || !term) return NULL;
	const struct pg_occurrence *operands[] = {domain->subject, codomain->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, domain->context, term, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {domain, extended_context, codomain};
	return accept(typing, PG_PI_FORM, PG_JUDGEMENT_COMPUTATION_TYPE,
		domain->context, subject, bound, 3, premises);
}

static const struct pg_evidence *unary_term(struct pg_typing *typing,
	const struct pg_evidence *argument, const struct pg_object *operation,
	const struct pg_term *classifier, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement)
{
	if (!classifier) return NULL;
	const struct pg_term *term = pg_application(typing->graph,
		pg_reference(typing->graph, operation), argument->subject->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, argument->context,
		term, NULL, 1, &argument->subject);
	if (!subject) return NULL;
	return accept(typing, rule, judgement, argument->context, subject, classifier, 1, &argument);
}

const struct pg_evidence *pg_prove_return(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value)
{
	if (!value || value->owner != typing) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	return unary_term(typing, value, &pg_return_operation,
		pg_return_type(classifiers, value->classifier), PG_RETURN_INTRO, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation)
{
	if (!computation || computation->owner != typing) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	return unary_term(typing, computation, &pg_thunk_operation,
		pg_thunk_type(classifiers, computation->classifier), PG_THUNK_INTRO, PG_JUDGEMENT_VALUE);
}

const struct pg_evidence *pg_prove_force(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!value || value->owner != typing) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_term *classifier;
	if (!pg_thunk_type_view(value->classifier, &classifier)) return NULL;
	return unary_term(typing, value, &pg_force_operation, classifier, PG_FORCE_ELIM, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_lambda(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *body)
{
	if (!pi || pi->owner != typing) return NULL;
	if (pi->rule != PG_PI_FORM) return NULL;
	if (!body || body->owner != typing) return NULL;
	if (body->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	if (body->context != pi->premises[1]->context) return NULL;
	if (pg_alpha_equal(body->classifier, codomain) != 1) return NULL;
	const struct pg_term *term = pg_lambda(typing->graph, binder, body->subject->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, term, domain, 1, &body->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {pi, body};
	return accept(typing, PG_LAMBDA_INTRO, PG_JUDGEMENT_COMPUTATION,
		pi->context, subject, pi->subject->core, 2, premises);
}

static int context_action(const struct pg_evidence *proof)
{
	return proof->rule == PG_CONTEXT_PROJECTION || proof->rule == PG_REINDEX;
}

static const struct pg_evidence *apply_context_action(struct pg_typing *typing,
	const struct pg_evidence *action, const struct pg_evidence *content)
{
	if (action->rule == PG_CONTEXT_PROJECTION)
		return pg_prove_projection(typing, action->premises[0], content);
	if (action->rule == PG_REINDEX)
		return pg_prove_reindex(typing, action->premises[0], content);
	return NULL;
}

static const struct pg_evidence *prepare_beta(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *application,
	const struct pg_evidence **body)
{
	*body = NULL;
	if (!context_proof(typing, context)) return NULL;
	if (!application || application->owner != typing) return NULL;
	if (application->rule != PG_APP_ELIM || application->context != context->context) return NULL;
	const struct pg_evidence *function = application->premises[0];
	size_t action_count = 0;
	while (context_action(function)) {
		++action_count;
		function = function->premises[1];
	}
	if (function->rule != PG_LAMBDA_INTRO) return NULL;
	const struct pg_evidence *body_context = function->premises[0]->premises[1];
	const struct pg_evidence *base_context = body_context->premises[0];
	size_t count = 0;
	for (const struct pg_context *scope = body_context->context; scope; scope = scope->parent) ++count;
	if (!count || count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	if (action_count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	const struct pg_evidence **actions = pg_alloc(&temporary, action_count * sizeof(*actions));
	const struct pg_evidence *result = NULL;
	if (!images) goto done;
	if (action_count && !actions) goto done;
	const struct pg_evidence *action = application->premises[0];
	for (size_t i = action_count; i; --i) {
		actions[i - 1] = action;
		action = action->premises[1];
	}
	images[count - 1] = application->premises[1];
	const struct pg_context *scope = body_context->context->parent;
	for (size_t i = count - 1; i; --i) {
		images[i - 1] = pg_prove_variable(typing, base_context, scope->binder);
		for (size_t j = 0; j < action_count; ++j)
			images[i - 1] = apply_context_action(typing, actions[j], images[i - 1]);
		if (!images[i - 1]) goto done;
		scope = scope->parent;
	}
	const struct pg_evidence *substitution = pg_prove_substitution(typing, body_context, context, count, images);
	if (!substitution) goto done;
	result = substitution;
	*body = function->premises[1];
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_reduce_beta(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *application)
{
	const struct pg_evidence *body;
	const struct pg_evidence *substitution = prepare_beta(typing, context, application, &body);
	if (!substitution) return NULL;
	const struct pg_evidence *result = pg_prove_reindex(typing, substitution, body);
	return result && pg_alpha_equal(result->classifier, application->classifier) == 1 ? result : NULL;
}

/* Invert introductions through the same context actions as their contents. */
static const struct pg_evidence *introduced_content(struct pg_typing *typing,
	const struct pg_evidence *proof, enum pg_evidence_rule introduction)
{
	if (proof->rule == introduction) return proof->premises[0];
	if (!context_action(proof)) return NULL;
	const struct pg_evidence *content = introduced_content(typing, proof->premises[1], introduction);
	return apply_context_action(typing, proof, content);
}

const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!computation || computation->owner != typing) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	return introduced_content(typing, computation, PG_RETURN_INTRO);
}

const struct pg_evidence *pg_prove_thunk_computation(struct pg_typing *typing,
	const struct pg_evidence *value)
{
	if (!value || value->owner != typing) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	return introduced_content(typing, value, PG_THUNK_INTRO);
}

int pg_prepare_context_action(struct pg_typing *typing, const struct pg_evidence *context,
	const struct pg_evidence *computation, struct pg_reduction *step)
{
	if (!step) return -1;
	*step = (struct pg_reduction){0};
	if (!context_proof(typing, context)) return -1;
	if (!computation || computation->owner != typing) return -1;
	if (computation->context != context->context) return -1;
	switch (computation->rule) {
	case PG_CONTEXT_PROJECTION: {
		const struct pg_evidence *source = context;
		while (source->context != computation->premises[1]->context) {
			if (source->rule != PG_CONTEXT_EXTEND) return -1;
			source = source->premises[0];
		}
		step->context = source;
		step->input = computation->premises[1];
		return 0;
	}
	case PG_REINDEX:
		step->context = computation->premises[0]->premises[0];
		step->input = computation->premises[1];
		return 0;
	default: return -1;
	}
}

int pg_prepare_reduction(struct pg_typing *typing, const struct pg_evidence *context,
	const struct pg_evidence *computation, struct pg_reduction *step)
{
	if (!step) return -1;
	*step = (struct pg_reduction){0};
	if (!context_proof(typing, context)) return -1;
	if (!computation || computation->owner != typing) return -1;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return -1;
	if (computation->context != context->context) return -1;
	const struct pg_evidence *result = NULL, *value;
	step->context = context;
	switch (computation->rule) {
	case PG_CONTEXT_PROJECTION: case PG_REINDEX:
		return pg_prepare_context_action(typing, context, computation, step);
	case PG_APP_ELIM:
		step->substitution = prepare_beta(typing, context, computation, &step->input);
		if (!step->substitution) {
			step->input = computation->premises[0];
		}
		return 0;
	case PG_FORCE_ELIM:
		result = pg_prove_thunk_computation(typing, computation->premises[0]);
		break;
	case PG_FOLD_ELIM:
		value = pg_prove_return_value(typing, computation->premises[0]);
		if (value) result = pg_prove_application(typing, computation->premises[1], value);
		else {
			step->input = computation->premises[0];
			return 0;
		}
		break;
	default:
		return -1;
	}
	if (!result) return -1;
	if (pg_alpha_equal(result->classifier, computation->classifier) != 1) return -1;
	step->result = result;
	return 0;
}

const struct pg_evidence *pg_prove_computation_operand(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *operand)
{
	if (!computation || computation->owner != typing) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_evidence *result;
	switch (computation->rule) {
	case PG_CONTEXT_PROJECTION: case PG_REINDEX:
		result = apply_context_action(typing, computation, operand);
		break;
	case PG_APP_ELIM:
		result = pg_prove_application(typing, operand, computation->premises[1]);
		break;
	case PG_FOLD_ELIM:
		result = pg_prove_fold(typing, operand, computation->premises[1]);
		break;
	default: return NULL;
	}
	if (!result) return NULL;
	if (result->context != computation->context) return NULL;
	if (pg_alpha_equal(result->classifier, computation->classifier) != 1) return NULL;
	return result;
}

const struct pg_evidence *pg_reduce_computation(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *computation)
{
	struct pg_reduction step;
	if (pg_prepare_reduction(typing, context, computation, &step) != 0) return NULL;
	if (step.result) return step.result;
	if (step.substitution) {
		const struct pg_evidence *result = pg_prove_reindex(typing, step.substitution, step.input);
		return result && pg_alpha_equal(result->classifier, computation->classifier) == 1 ? result : NULL;
	}
	const struct pg_evidence *operand = pg_reduce_computation(typing, step.context, step.input);
	return pg_prove_computation_operand(typing, computation, operand);
}

const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!function || function->owner != typing) return NULL;
	if (!argument || argument->owner != typing) return NULL;
	if (function->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (function->context != argument->context) return NULL;
	const struct pg_evidence *premises[] = {function, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_APP_ELIM,
		PG_JUDGEMENT_COMPUTATION, function->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(function->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, argument->classifier) != 1) return NULL;
	struct pg_binding_value substitution = {binder, argument->subject->core};
	const struct pg_term *classifier = pg_term_substitute(typing->graph, codomain, 1, &substitution);
	const struct pg_term *term = pg_application(typing->graph, function->subject->core, argument->subject->core);
	if (!classifier || !term) return NULL;
	const struct pg_occurrence *operands[] = {function->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, function->context, term, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_APP_ELIM, PG_JUDGEMENT_COMPUTATION,
		function->context, subject, classifier, 2, premises);
}

const struct pg_evidence *pg_prove_conversion(struct pg_typing *typing,
	const struct pg_evidence *term, const struct pg_evidence *target_type,
	const struct pg_conversion_certificate *certificate)
{
	if (!certificate) return NULL;
	if (!term || term->owner != typing) return NULL;
	if (!target_type || target_type->owner != typing) return NULL;
	if (term->context != target_type->context) return NULL;
	switch (term->judgement) {
	case PG_JUDGEMENT_VALUE:
		target_type = pg_prove_value_type(typing, target_type);
		if (!target_type) return NULL;
		break;
	case PG_JUDGEMENT_COMPUTATION:
		if (target_type->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		break;
	default:
		return NULL;
	}
	if (pg_conversion_left(certificate) != term->classifier) return NULL;
	if (pg_conversion_right(certificate) != target_type->subject->core) return NULL;
	const struct pg_evidence *premises[] = {term, target_type};
	return accept_with_conversion(typing, PG_TYPE_CONVERSION, term->judgement,
		term->context, term->subject, target_type->subject->core, 2, premises, certificate);
}

const struct pg_conversion_certificate *pg_evidence_conversion(const struct pg_evidence *evidence) { return evidence->conversion; }
const struct pg_evidence *pg_prove_projection(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!context_proof(typing, context)) return NULL;
	if (!proof || proof->owner != typing) return NULL;
	if (proof->judgement == PG_JUDGEMENT_CONTEXT) return NULL;
	if (proof->judgement == PG_JUDGEMENT_SUBSTITUTION) return NULL;
	const struct pg_context *cursor = context->context;
	while (cursor != proof->context) {
		if (!cursor) return NULL;
		cursor = cursor->parent;
	}
	if (context->context == proof->context) return proof;
	const struct pg_occurrence *old = proof->subject;
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context,
		old->core, old->annotation, old->operand_count, old->operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {context, proof};
	return accept(typing, PG_CONTEXT_PROJECTION, proof->judgement,
		context->context, subject, proof->classifier, 2, premises);
}

const struct pg_evidence *pg_prove_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, const struct pg_evidence *const *images)
{
	if (!context_proof(typing, source)) return NULL;
	if (!context_proof(typing, destination)) return NULL;
	if (count && !images) return NULL;
	size_t arity = 0;
	for (const struct pg_context *scope = source->context; scope; scope = scope->parent) ++arity;
	if (arity != count) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_binding_value)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_context *)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *) - 2) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_context **declarations = pg_alloc(&temporary, count * sizeof(*declarations));
	struct pg_binding_value *bindings = pg_alloc(&temporary, count * sizeof(*bindings));
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 2) * sizeof(*premises));
	if (!premises) goto done;
	if (count && (!declarations || !bindings)) goto done;
	const struct pg_context *scope = source->context;
	for (size_t i = count; i; --i) { declarations[i - 1] = scope; scope = scope->parent; }
	premises[0] = source;
	premises[1] = destination;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		if (!image || image->owner != typing) goto done;
		if (image->judgement != PG_JUDGEMENT_VALUE) goto done;
		if (image->context != destination->context) goto done;
		premises[i + 2] = image;
	}
	uint64_t hash;
	result = find_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, count + 2, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		const struct pg_term *expected = pg_term_substitute(typing->graph, declarations[i]->declared_type, i, bindings);
		if (!expected) goto done;
		if (pg_alpha_equal(expected, image->classifier) != 1) goto done;
		bindings[i] = (struct pg_binding_value){declarations[i]->binder, image->subject->core};
	}
	result = accept_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, count + 2, premises, NULL, count, bindings);
done:
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_reindex_state {
	struct pg_typing *typing;
	const struct pg_evidence *premises[2];
	const struct pg_term *inputs[3], *outputs[3];
	struct pg_substitution substitution;
	size_t next;
	uint64_t steps;
	enum pg_reindex_status status;
	const struct pg_evidence *result;
};

static int reindex_prepare(struct pg_reindex_state *state, struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!substitution || substitution->owner != typing) return -1;
	if (substitution->rule != PG_CONTEXT_SUBSTITUTION) return -1;
	if (!proof || proof->owner != typing) return -1;
	if (!proof->subject) return -1;
	if (proof->context != substitution->premises[0]->context) return -1;
	state->typing = typing;
	state->premises[0] = substitution;
	state->premises[1] = proof;
	state->inputs[0] = proof->subject->core;
	state->inputs[1] = proof->classifier;
	state->inputs[2] = proof->subject->annotation;
	const struct pg_evidence *premises[] = {substitution, proof};
	uint64_t hash;
	state->result = find_record(typing, PG_REINDEX, proof->judgement,
		substitution->context, NULL, NULL, 2, premises, NULL, &hash);
	state->status = state->result ? PG_REINDEX_DONE : PG_REINDEX_PENDING;
	return 0;
}

int pg_reindex_init(struct pg_reindex *work, struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	work->state = calloc(1, sizeof(*work->state));
	if (!work->state) return -1;
	if (reindex_prepare(work->state, typing, substitution, proof) == 0) return 0;
	pg_reindex_destroy(work);
	return -1;
}

static enum pg_reindex_status reindex_step(struct pg_reindex_state *state)
{
	struct pg_typing *typing = state->typing;
	const struct pg_evidence *substitution = state->premises[0], *proof = state->premises[1];
	if (state->next < 3) {
		if (!state->inputs[state->next]) { ++state->next; return PG_REINDEX_PENDING; }
		if (!state->substitution.state) {
			size_t count = substitution->premise_count - 2;
			const struct pg_binding_value *bindings = (const struct pg_binding_value *)(substitution->premises + substitution->premise_count);
			if (pg_substitution_init(&state->substitution, typing->graph,
				state->inputs[state->next], count, bindings) != 0) return PG_REINDEX_ERROR;
			return PG_REINDEX_PENDING;
		}
		switch (pg_substitution_advance(&state->substitution, 1)) {
		case PG_SUBSTITUTION_PENDING: return PG_REINDEX_PENDING;
		case PG_SUBSTITUTION_ERROR: return PG_REINDEX_ERROR;
		case PG_SUBSTITUTION_DONE:
			state->outputs[state->next++] = pg_substitution_result(&state->substitution);
			pg_substitution_destroy(&state->substitution);
			return PG_REINDEX_PENDING;
		}
	}
	const struct pg_occurrence *old = proof->subject;
	const struct pg_occurrence *subject = pg_occurrence(typing, substitution->context, state->outputs[0],
		state->outputs[2], old->operand_count, old->operands);
	if (!subject) return PG_REINDEX_ERROR;
	state->result = accept(typing, PG_REINDEX, proof->judgement, substitution->context,
		subject, state->outputs[1], 2, state->premises);
	return state->result ? PG_REINDEX_DONE : PG_REINDEX_ERROR;
}

enum pg_reindex_status pg_reindex_status(const struct pg_reindex *work)
{
	return work->state ? work->state->status : PG_REINDEX_ERROR;
}

enum pg_reindex_status pg_reindex_advance(struct pg_reindex *work, uint64_t budget)
{
	while (pg_reindex_status(work) == PG_REINDEX_PENDING && budget) {
		--budget;
		++work->state->steps;
		work->state->status = reindex_step(work->state);
	}
	return pg_reindex_status(work);
}

const struct pg_evidence *pg_reindex_result(const struct pg_reindex *work)
{
	return pg_reindex_status(work) == PG_REINDEX_DONE ? work->state->result : NULL;
}

uint64_t pg_reindex_steps(const struct pg_reindex *work)
{
	return work->state ? work->state->steps : 0;
}

void pg_reindex_destroy(struct pg_reindex *work)
{
	if (!work->state) return;
	pg_substitution_destroy(&work->state->substitution);
	free(work->state);
	work->state = NULL;
}

const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	struct pg_reindex_state state = {0};
	struct pg_reindex work = {&state};
	if (reindex_prepare(&state, typing, substitution, proof) != 0) return NULL;
	while (pg_reindex_advance(&work, UINT64_MAX) == PG_REINDEX_PENDING) {}
	const struct pg_evidence *result = pg_reindex_result(&work);
	pg_substitution_destroy(&state.substitution);
	return result;
}

const struct pg_evidence *pg_prove_reindexed_variable(struct pg_typing *typing,
	const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return NULL;
	if (proof->rule != PG_REINDEX || proof->judgement != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_evidence *original = proof->premises[1];
	while (original->rule == PG_CONTEXT_PROJECTION) original = original->premises[1];
	if (original->rule != PG_VARIABLE) return NULL;
	const struct pg_evidence *substitution = proof->premises[0];
	size_t count = substitution->premise_count - 2;
	const struct pg_binding_value *bindings = (const struct pg_binding_value *)(substitution->premises + substitution->premise_count);
	for (size_t i = 0; i < count; ++i) {
		if (bindings[i].binder != original->subject->core->as.reference) continue;
		const struct pg_evidence *image = substitution->premises[i + 2];
		return pg_alpha_equal(image->classifier, proof->classifier) == 1 ? image : NULL;
	}
	return NULL;
}

const struct pg_evidence *pg_prove_reindexed_elimination(struct pg_typing *typing,
	const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return NULL;
	if (proof->rule != PG_REINDEX || proof->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_evidence *substitution = proof->premises[0], *original = proof->premises[1];
	const struct pg_evidence *left, *right, *result;
	switch (original->rule) {
	case PG_FORCE_ELIM:
		left = pg_prove_reindex(typing, substitution, original->premises[0]);
		result = pg_prove_force(typing, left);
		break;
	case PG_APP_ELIM: case PG_FOLD_ELIM:
		left = pg_prove_reindex(typing, substitution, original->premises[0]);
		right = pg_prove_reindex(typing, substitution, original->premises[1]);
		result = original->rule == PG_APP_ELIM ? pg_prove_application(typing, left, right)
			: pg_prove_fold(typing, left, right);
		break;
	default: return NULL;
	}
	if (!result) return NULL;
	return pg_alpha_equal(result->classifier, proof->classifier) == 1 ? result : NULL;
}

static int substitution_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return 0;
	return proof->rule == PG_CONTEXT_SUBSTITUTION;
}

const struct pg_evidence *pg_prove_reindexed_premise(struct pg_typing *typing,
	const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return NULL;
	if (proof->rule != PG_REINDEX) return NULL;
	const struct pg_evidence *substitution = proof->premises[0], *original = proof->premises[1];
	switch (original->rule) {
	case PG_TYPE_CONVERSION:
		return pg_prove_reindex(typing, substitution, original->premises[0]);
	case PG_REINDEX:
		substitution = pg_prove_substitution_compose(typing, original->premises[0], substitution);
		return pg_prove_reindex(typing, substitution, original->premises[1]);
	case PG_CONTEXT_PROJECTION: {
		const struct pg_evidence *source = substitution->premises[0];
		size_t count = substitution->premise_count - 2;
		original = original->premises[1];
		while (source->context != original->context) {
			if (source->rule != PG_CONTEXT_EXTEND || !count) return NULL;
			source = source->premises[0];
			--count;
		}
		substitution = pg_prove_substitution(typing, source, substitution->premises[1],
			count, substitution->premises + 2);
		return pg_prove_reindex(typing, substitution, original);
	}
	default: return NULL;
	}
}

const struct pg_evidence *pg_prove_substitution_compose(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second)
{
	if (!substitution_proof(typing, first)) return NULL;
	if (!substitution_proof(typing, second)) return NULL;
	if (first->context != second->premises[0]->context) return NULL;
	size_t count = first->premise_count - 2;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	const struct pg_evidence *result = NULL;
	if (count && !images) goto done;
	for (size_t i = 0; i < count; ++i) {
		images[i] = pg_prove_reindex(typing, second, first->premises[i + 2]);
		if (!images[i]) goto done;
	}
	result = pg_prove_substitution(typing, first->premises[0], second->premises[1], count, images);
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* Pairing and lifting differ only in the destination and final image. Prefix
 * images are shared, or weakened into the extended destination for a lift. */
static const struct pg_evidence *substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *destination, const struct pg_evidence *image)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	if (!context_proof(typing, destination)) return NULL;
	size_t count = substitution->premise_count - 2;
	if (count >= SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, (count + 1) * sizeof(*images));
	const struct pg_evidence *result = NULL;
	if (!images) goto done;
	for (size_t i = 0; i < count; ++i) {
		images[i] = pg_prove_projection(typing, destination, substitution->premises[i + 2]);
		if (!images[i]) goto done;
	}
	images[count] = image;
	result = pg_prove_substitution(typing, source_extension, destination, count + 1, images);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *image)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	return substitution_pair(typing, substitution, source_extension,
		substitution->premises[1], image);
}

const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	const struct pg_evidence *domain = pg_prove_reindex(typing, substitution, source_extension->premises[1]);
	const struct pg_evidence *destination = pg_prove_context_extension(typing, substitution->premises[1], binder, domain);
	if (!destination) return NULL;
	const struct pg_evidence *image = pg_prove_variable(typing, destination, binder);
	return substitution_pair(typing, substitution, source_extension, destination, image);
}

const struct pg_evidence *pg_prove_thunk_content(struct pg_typing *typing,
	const struct pg_evidence *thunk_type)
{
	thunk_type = pg_prove_value_type(typing, thunk_type);
	if (!thunk_type) return NULL;
	const struct pg_term *content;
	if (!pg_thunk_type_view(thunk_type->subject->core, &content)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, thunk_type->context, content,
		NULL, 1, &thunk_type->subject);
	if (!subject) return NULL;
	return accept(typing, PG_THUNK_CONTENT, PG_JUDGEMENT_COMPUTATION_TYPE,
		thunk_type->context, subject, thunk_type->classifier, 1, &thunk_type);
}

const struct pg_evidence *pg_prove_return_content(struct pg_typing *typing,
	const struct pg_evidence *return_type)
{
	if (!return_type || return_type->owner != typing) return NULL;
	if (return_type->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *content;
	if (!pg_return_type_view(return_type->subject->core, &content)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, return_type->context, content,
		NULL, 1, &return_type->subject);
	if (!subject) return NULL;
	return accept(typing, PG_RETURN_CONTENT, PG_JUDGEMENT_VALUE_TYPE,
		return_type->context, subject, return_type->classifier, 1, &return_type);
}

static const struct pg_term *constant_codomain(struct pg_typing *typing, const struct pg_term *pi)
{
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi, &domain, &binder, &codomain)) return NULL;
	const struct pg_term *fresh = pg_reference(typing->graph, pg_binder(typing->graph));
	if (!fresh) return NULL;
	struct pg_binding_value binding = {binder, fresh};
	const struct pg_term *renamed = pg_term_substitute(typing->graph, codomain, 1, &binding);
	if (!renamed || pg_alpha_equal(codomain, renamed) != 1) return NULL;
	return codomain;
}

const struct pg_evidence *pg_prove_pi_constant_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pi || pi->owner != typing) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CONSTANT_CODOMAIN,
		PG_JUDGEMENT_COMPUTATION_TYPE, pi->context, NULL, NULL, 1, &pi, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *codomain = constant_codomain(typing, pi->subject->core);
	if (!codomain) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, codomain, NULL, 1, &pi->subject);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CONSTANT_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 1, &pi);
}

const struct pg_evidence *pg_prove_fold(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *continuation)
{
	if (!computation || computation->owner != typing) return NULL;
	if (!continuation || continuation->owner != typing) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (continuation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (computation->context != continuation->context) return NULL;
	const struct pg_evidence *premises[] = {computation, continuation};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_FOLD_ELIM,
		PG_JUDGEMENT_COMPUTATION, computation->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *value_type, *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_return_type_view(computation->classifier, &value_type)) return NULL;
	if (!pg_pi_view(continuation->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, value_type) != 1) return NULL;
	codomain = constant_codomain(typing, continuation->classifier);
	if (!codomain) return NULL;
	const struct pg_term *head = pg_application(typing->graph,
		pg_reference(typing->graph, &pg_fold_operation), computation->subject->core);
	const struct pg_term *core = pg_application(typing->graph, head, continuation->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {computation->subject, continuation->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, computation->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_FOLD_ELIM, PG_JUDGEMENT_COMPUTATION,
		computation->context, subject, codomain, 2, premises);
}

const struct pg_evidence *pg_prove_pi_domain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pi || pi->owner != typing) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, domain, NULL, 1, &pi->subject);
	if (!subject) return NULL;
	return accept(typing, PG_PI_DOMAIN, PG_JUDGEMENT_VALUE_TYPE,
		pi->context, subject, pi->classifier, 1, &pi);
}

const struct pg_evidence *pg_prove_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument)
{
	if (!pi || pi->owner != typing) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (!argument || argument->owner != typing) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (pi->context != argument->context) return NULL;
	const struct pg_evidence *premises[] = {pi, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CODOMAIN,
		PG_JUDGEMENT_COMPUTATION_TYPE, pi->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, argument->classifier) != 1) return NULL;
	struct pg_binding_value binding = {binder, argument->subject->core};
	const struct pg_term *type = pg_term_substitute(typing->graph, codomain, 1, &binding);
	if (!type) return NULL;
	const struct pg_occurrence *operands[] = {pi->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, type, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 2, premises);
}

const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	if (!context_proof(typing, context)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	if (!term || term->owner != typing) return NULL;
	if (context->context != term->context) return NULL;
	/* A projected term keeps its original derivation; inspect it without copying
	 * the DAG, then transport the recovered formation to the requested context. */
	while (term->rule == PG_CONTEXT_PROJECTION) term = term->premises[1];
	const struct pg_evidence *formation = NULL;
	switch (term->rule) {
	case PG_REFLEXIVITY:
		formation = term->premises[0];
		break;
	case PG_VARIABLE: {
		const struct pg_evidence *declaration = term->premises[0];
		const struct pg_object *binder = term->subject->core->as.reference;
		while (declaration->context->binder != binder) declaration = declaration->premises[0];
		formation = declaration->premises[1];
		break;
	}
	case PG_VALUE_FROM_TYPE: {
		uint64_t level;
		if (!pg_universe_level(term->classifier, &level)) return NULL;
		return pg_prove_universe(typing, classifiers, context, level);
	}
	case PG_RETURN_INTRO:
	case PG_THUNK_INTRO: {
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[0]);
		formation = pg_prove_classifier(typing, classifiers, context, argument);
		if (term->rule == PG_RETURN_INTRO) return pg_prove_return_type(typing, classifiers, formation);
		return pg_prove_thunk_type(typing, classifiers, formation);
	}
	case PG_FORCE_ELIM: {
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[0]);
		formation = pg_prove_classifier(typing, classifiers, context, argument);
		formation = pg_prove_thunk_content(typing, formation);
		break;
	}
	case PG_LAMBDA_INTRO:
		formation = term->premises[0];
		break;
	case PG_APP_ELIM: {
		const struct pg_evidence *function = pg_prove_projection(typing, context, term->premises[0]);
		const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, context, function);
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[1]);
		formation = pg_prove_pi_codomain(typing, pi, argument);
		break;
	}
	case PG_TYPE_CONVERSION:
		formation = term->premises[1];
		break;
	case PG_FOLD_ELIM: {
		const struct pg_evidence *continuation = pg_prove_projection(typing, context, term->premises[1]);
		const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, context, continuation);
		formation = pg_prove_pi_constant_codomain(typing, pi);
		break;
	}
	case PG_REINDEX: {
		const struct pg_evidence *substitution = term->premises[0];
		formation = pg_prove_classifier(typing, classifiers, substitution->premises[0], term->premises[1]);
		formation = pg_prove_reindex(typing, substitution, formation);
		break;
	}
	default:
		return NULL;
	}
	return pg_prove_projection(typing, context, formation);
}

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence) { return evidence->rule; }
enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence) { return evidence->judgement; }
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence) { return evidence->context; }
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence) { return evidence->subject; }
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence) { return evidence->classifier; }
size_t pg_evidence_premise_count(const struct pg_evidence *evidence) { return evidence->premise_count; }
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index)
{
	return index < evidence->premise_count ? evidence->premises[index] : NULL;
}
