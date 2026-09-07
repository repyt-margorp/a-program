#ifndef A_PROGRAM_POINTER_SYNTHESIS_H
#define A_PROGRAM_POINTER_SYNTHESIS_H

#include "syntax.h"
#include "evidence.h"

enum pg_synthesis_status { PG_SYNTHESIS_PENDING, PG_SYNTHESIS_DONE,
	PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_UNSUPPORTED, PG_SYNTHESIS_ERROR };
struct pg_source_scope;
struct pg_synthesis_job;
struct pg_synthesis {
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	struct pg_beta_work *beta;
	struct pg_index jobs;
	struct pg_synthesis_job *ready;
	uint64_t steps;
};

/* Syntax, source buffers, typing, classifiers and beta work outlive this store.
 * Requesting an expression only creates pending work; advance performs it. */
int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_beta_work *beta);
void pg_synthesis_destroy(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context);
struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget);
enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job);
const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job);

#endif
