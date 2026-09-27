#ifndef A_PROGRAM_POINTER_EVIDENCE_STRUCTURE_H
#define A_PROGRAM_POINTER_EVIDENCE_STRUCTURE_H

#include "evidence.h"

/* Direct construction rules retain typed inputs instead of premise history.
 * Fill at most three borrowed edges; zero means this owner does not handle the
 * rule. These views neither evaluate nor certify their inputs. */
size_t pg_function_proof_inputs(const struct pg_evidence *, const struct pg_occurrence **);
size_t pg_cbpv_proof_inputs(const struct pg_evidence *, const struct pg_occurrence **);
size_t pg_identity_proof_inputs(const struct pg_evidence *, const struct pg_occurrence **);
const struct pg_evidence *pg_identity_export_input(const struct pg_typing *typing,
	const struct pg_evidence *proof, size_t index, const struct pg_evidence *input);

/* Owner-local checking in the existing structural dependency walk. Missing
 * input selects one child; a ready owner calls the ordinary kernel rule.
 * No receipt is recovered by searching for another use of the same Core. */
const struct pg_evidence *pg_structure_input(struct pg_typing *typing,
	const struct pg_occurrence *input, const struct pg_occurrence **child);
const struct pg_evidence *pg_function_structure(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_occurrence *subject,
	const struct pg_occurrence **child);
const struct pg_evidence *pg_function_selection(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child);
/* Retain a checked declaration's formations, not its acceptance history. */
const struct pg_occurrence *pg_function_binding(struct pg_typing *typing,
	const struct pg_evidence *scope, const struct pg_occurrence *body, enum pg_evidence_judgement judgement);
/* Extend a checked map through selected declarations, at an already computed
 * target allocation. Raw Context identity must not select formation bounds. */
const struct pg_evidence *pg_check_scope_action(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_scope *source,
	const struct pg_context *target);
const struct pg_evidence *pg_cbpv_structure(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child);
const struct pg_evidence *pg_cbpv_selection(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child);
const struct pg_evidence *pg_identity_structure(struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_occurrence **child);
/* The same APP rule, with a retained result allocation instead of allocating
 * a second Pi result and requiring pointer equality with its fresh binders. */
const struct pg_evidence *pg_check_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument,
	const struct pg_occurrence *subject);
const struct pg_evidence *pg_check_family_application(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *index,
	const struct pg_occurrence *subject);
const struct pg_evidence *pg_check_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument,
	const struct pg_occurrence *subject);

#endif
