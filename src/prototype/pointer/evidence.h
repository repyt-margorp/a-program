#ifndef A_PROGRAM_POINTER_EVIDENCE_H
#define A_PROGRAM_POINTER_EVIDENCE_H

#include "typing.h"
#include "classifier.h"

enum pg_evidence_rule { PG_CONTEXT_EMPTY, PG_CONTEXT_EXTEND, PG_UNIVERSE_FORM, PG_VARIABLE,
	PG_TYPE_FROM_VALUE, PG_RETURN_TYPE_FORM, PG_THUNK_TYPE_FORM, PG_PI_FORM,
	PG_RETURN_INTRO, PG_THUNK_INTRO, PG_FORCE_ELIM, PG_LAMBDA_INTRO, PG_APP_ELIM };
enum pg_evidence_judgement { PG_JUDGEMENT_CONTEXT, PG_JUDGEMENT_VALUE_TYPE,
	PG_JUDGEMENT_COMPUTATION_TYPE, PG_JUDGEMENT_VALUE, PG_JUDGEMENT_COMPUTATION };
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
/* A value in a value-type universe determines a value type. This operation
 * cannot turn a computation-type formation into a value-type formation. */
const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value);
const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value_type);
const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation_type);
const struct pg_evidence *pg_prove_pi(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *domain, const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain);

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence);
const struct pg_evidence *pg_prove_return(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value);
const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation);
const struct pg_evidence *pg_prove_force(struct pg_typing *typing,
	const struct pg_evidence *value);
/* Exact classifier premises; conversion must be supplied as a separate
 * derivation rather than silently changing the input or running a solver. */
const struct pg_evidence *pg_prove_lambda(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *body);
const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence);
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence);
/* Context formation has no term subject or classifier. */
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence);
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence);
size_t pg_evidence_premise_count(const struct pg_evidence *evidence);
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index);

#endif
