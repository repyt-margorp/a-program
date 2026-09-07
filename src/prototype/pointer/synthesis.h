#ifndef A_PROGRAM_POINTER_SYNTHESIS_H
#define A_PROGRAM_POINTER_SYNTHESIS_H

#include "syntax.h"
#include "evidence.h"

enum pg_synthesis_status { PG_SYNTHESIS_PENDING, PG_SYNTHESIS_DONE,
	PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_UNSUPPORTED, PG_SYNTHESIS_ERROR };
struct pg_source_scope;
struct pg_synthesis_job;
enum pg_definition_policy { PG_DEFINITION_IMPLICIT_THUNK, PG_DEFINITION_EXPLICIT_THUNK };
struct pg_synthesis {
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	struct pg_beta_work *beta;
	struct pg_index jobs;
	struct pg_synthesis_job *ready;
	uint64_t steps;
	enum pg_definition_policy definition_policy;
};

/* Syntax, source buffers, typing, classifiers and beta work outlive this store.
 * Requesting an expression only creates pending work; advance performs it. */
int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_beta_work *beta,
	enum pg_definition_policy definition_policy);
void pg_synthesis_destroy(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context);
struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Pure checked computation -> returned value, using the same job table and
 * scheduler. The immutable context/evidence pair is the key, never bare Core.
 * Requests do not reduce; unsupported neutral heads are not negative proofs.
 * Evidence outlives this store. Primitive rule traversal is not yet budgeted. */
struct pg_synthesis_job *pg_synthesis_return(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *computation);
/* One demanded reduction step. Operand reductions are shared dependencies
 * in the same scheduler, not recursive calls hidden inside this request. */
struct pg_synthesis_job *pg_synthesis_reduce(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *computation);
void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget);
enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job);
const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job);
/* Inspect the active subscription, not historical premises. A cycle query
 * returns one job on a reachable waiting cycle, or NULL. It neither advances
 * work nor rejects recursion, and an absent cycle does not prove progress. */
const struct pg_synthesis_job *pg_synthesis_dependency(const struct pg_synthesis_job *job);
const struct pg_synthesis_job *pg_synthesis_cycle(const struct pg_synthesis_job *job);
/* After a definition root has been indexed, retrieve its producer job without
 * resynthesizing it. NULL means not indexed or no such local definition.
 * A completed unselected library root has no expression result of its own. */
struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name);

#endif
