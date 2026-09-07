#include "evidence.h"
#include "computation.h"
#include "eval.h"

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

static const struct pg_evidence *accept_with_conversion(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion)
{
	uint64_t hash = ((uintptr_t)context ^ (uintptr_t)subject ^ (uintptr_t)classifier ^ rule) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->proofs, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_evidence *proof = (const struct pg_evidence *)candidate;
		if (proof->rule != rule) continue;
		if (proof->judgement != judgement) continue;
		if (proof->context != context) continue;
		if (proof->subject != subject) continue;
		if (proof->classifier != classifier) continue;
		if (proof->conversion != conversion) continue;
		if (proof->premise_count != count) continue;
		size_t i = 0;
		while (i < count && proof->premises[i] == premises[i]) ++i;
		if (i == count) return proof;
	}
	struct pg_evidence *proof = pg_alloc(typing->graph, sizeof(*proof) + count * sizeof(*premises));
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
	if (pg_index_insert(&typing->proofs, &proof->index, hash) != 0) return NULL;
	return proof;
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
	if (body->classifier != codomain) return NULL;
	const struct pg_term *term = pg_lambda(typing->graph, binder, body->subject->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, term, domain, 1, &body->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {pi, body};
	return accept(typing, PG_LAMBDA_INTRO, PG_JUDGEMENT_COMPUTATION,
		pi->context, subject, pi->subject->core, 2, premises);
}

const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!function || function->owner != typing) return NULL;
	if (!argument || argument->owner != typing) return NULL;
	if (function->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (function->context != argument->context) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(function->classifier, &domain, &binder, &codomain)) return NULL;
	if (domain != argument->classifier) return NULL;
	struct pg_binding_value substitution = {binder, argument->subject->core};
	const struct pg_term *classifier = pg_term_substitute(typing->graph, codomain, 1, &substitution);
	const struct pg_term *term = pg_application(typing->graph, function->subject->core, argument->subject->core);
	if (!classifier || !term) return NULL;
	const struct pg_occurrence *operands[] = {function->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, function->context, term, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {function, argument};
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
		if (!formation) return NULL;
		while (formation->rule == PG_CONTEXT_PROJECTION) formation = formation->premises[1];
		if (formation->rule != PG_THUNK_TYPE_FORM) return NULL;
		formation = formation->premises[0];
		break;
	}
	case PG_LAMBDA_INTRO:
		formation = term->premises[0];
		break;
	case PG_TYPE_CONVERSION:
		formation = term->premises[1];
		break;
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
