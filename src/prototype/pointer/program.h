#ifndef A_PROGRAM_POINTER_PROGRAM_H
#define A_PROGRAM_POINTER_PROGRAM_H

#include "synthesis.h"
#include "effect_inference.h"

/* One owner, not another compilation state machine. Do not copy or move this
 * object: its stores point to each other. Source tokens borrow owned storage.
 * Advance synthesis directly; its root/status/evidence remain authoritative.
 * This in-memory entry does not implement .a serialization or host execution. */
struct pg_program {
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_whnf_work evaluation;
	struct pg_synthesis synthesis;
	/* Own imported immutable effect equations; ordinary Solve computes them. */
	struct pg_effect_inference imported_effects;
	struct pg_parser parser;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job *root;
};

/* Initialize the same stores without a source/root. Image loading can populate
 * this graph before scheduling the ordinary synthesis root. */
struct pg_program *pg_program_allocate(enum pg_definition_policy policy);
/* Copies the input and parses without advancing synthesis. NULL indicates
 * initialization failure. Syntax errors return an owned program with a NULL
 * root and the ordinary parser diagnostic; destroy it normally. */
struct pg_program *pg_program_create(const char *source, size_t length,
	enum pg_definition_policy policy);
/* Parse another owned source in a caller-selected scope of this program.
 * The returned root may be published using the existing module/import APIs.
 * No filesystem resolution or solver advancement occurs. The caller owns the
 * diagnostic parser object; its token text remains owned by the program. */
struct pg_synthesis_job *pg_program_source(struct pg_program *program,
	const struct pg_source_scope *scope, const char *source, size_t length,
	struct pg_parser *diagnostic);
/* Request pure WHNF (full == 0) or NF of closed accepted evidence. A stored
 * thunk is forced once. Reuses ordinary synthesis jobs; does not advance Solve
 * or execute host effects. Foreign/open/unaccepted evidence is rejected. */
struct pg_synthesis_job *pg_program_normalize(struct pg_program *program,
	const struct pg_evidence *proof, int full);
/* Publish a source module's local assignments over parent, without Solve.
 * Each name refers to a whole-module-checked selection, not copied evidence.
 * Imports are not re-exported. The returned scope and syntax are graph-owned. */
const struct pg_source_scope *pg_program_exports(struct pg_program *program,
	const struct pg_source_scope *parent, struct pg_synthesis_job *module);
void pg_program_destroy(struct pg_program *program);

#endif
