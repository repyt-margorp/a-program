#include "evidence.h"

struct pg_evidence {
	struct pg_index_entry index;
	const struct pg_typing *owner;
	enum pg_evidence_rule rule;
	const struct pg_context *context;
	const struct pg_occurrence *subject;
	const struct pg_term *classifier;
	size_t premise_count;
	const struct pg_evidence *premises[];
};

static const struct pg_evidence *accept(struct pg_typing *typing, enum pg_evidence_rule rule,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises)
{
	uint64_t hash = ((uintptr_t)context ^ (uintptr_t)subject ^ (uintptr_t)classifier ^ rule) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->proofs, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_evidence *proof = (const struct pg_evidence *)candidate;
		if (proof->rule != rule) continue;
		if (proof->context != context) continue;
		if (proof->subject != subject) continue;
		if (proof->classifier != classifier) continue;
		if (proof->premise_count != count) continue;
		size_t i = 0;
		while (i < count && proof->premises[i] == premises[i]) ++i;
		if (i == count) return proof;
	}
	struct pg_evidence *proof = pg_alloc(typing->graph, sizeof(*proof) + count * sizeof(*premises));
	if (!proof) return NULL;
	proof->owner = typing;
	proof->rule = rule;
	proof->context = context;
	proof->subject = subject;
	proof->classifier = classifier;
	proof->premise_count = count;
	for (size_t i = 0; i < count; ++i) proof->premises[i] = premises[i];
	if (pg_index_insert(&typing->proofs, &proof->index, hash) != 0) return NULL;
	return proof;
}

static int context_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!proof || proof->owner != typing) return 0;
	return proof->rule == PG_CONTEXT_EMPTY || proof->rule == PG_CONTEXT_EXTEND;
}

const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing)
{
	return accept(typing, PG_CONTEXT_EMPTY, NULL, NULL, NULL, 0, NULL);
}

const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type)
{
	if (!context_proof(typing, parent)) return NULL;
	if (!type || type->owner != typing) return NULL;
	if (!type->subject || type->context != parent->context) return NULL;
	uint64_t level;
	if (!pg_universe_level(type->classifier, &level)) return NULL;
	if (pg_context_lookup(parent->context, binder)) return NULL;
	const struct pg_context *context = pg_context_bind(typing, parent->context, binder, type->subject->core);
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, type};
	return accept(typing, PG_CONTEXT_EXTEND, context, NULL, NULL, 2, premises);
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
	return accept(typing, PG_UNIVERSE_FORM, context->context, subject, sort, 1, &context);
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
	return accept(typing, PG_VARIABLE, context->context, subject, declaration->declared_type, 1, &context);
}

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence) { return evidence->rule; }
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence) { return evidence->context; }
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence) { return evidence->subject; }
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence) { return evidence->classifier; }
size_t pg_evidence_premise_count(const struct pg_evidence *evidence) { return evidence->premise_count; }
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index)
{
	return index < evidence->premise_count ? evidence->premises[index] : NULL;
}
