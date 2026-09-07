#ifndef A_PROGRAM_POINTER_EVIDENCE_H
#define A_PROGRAM_POINTER_EVIDENCE_H

#include "typing.h"
#include "classifier.h"

enum pg_evidence_rule { PG_CONTEXT_EMPTY, PG_CONTEXT_EXTEND, PG_UNIVERSE_FORM, PG_VARIABLE };
struct pg_evidence;

/* Checked primitive derivations, owned by typing->graph. NULL means a failed
 * premise check or allocation, not a proof of negation. No mutable proof API. */
const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing);
const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type);
const struct pg_evidence *pg_prove_universe(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context, uint64_t level);
const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder);

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence);
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence);
/* Context formation has no term subject or classifier. */
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence);
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence);
size_t pg_evidence_premise_count(const struct pg_evidence *evidence);
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index);

#endif
