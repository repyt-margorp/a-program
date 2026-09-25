#ifndef A_PROGRAM_POINTER_SYNTHESIS_SOURCE_H
#define A_PROGRAM_POINTER_SYNTHESIS_SOURCE_H

#include "synthesis_work.h"

/* Interned lexical inputs shared by elaboration owners. Handler and definition
 * progress remain opaque here; this is not a universal synthesis payload. */
struct pg_source_scope {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder, *associated_binder;
	enum pg_source_association association;
	struct pg_synthesis_job *context_job;
	struct definition_state *definitions;
	struct pg_synthesis_job *registration, *producer;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	const struct pg_source_scope *imports;
	struct handler_state *effect_owner;
	struct pg_synthesis_job *clause;
};

const struct pg_source_scope *pg_synthesis_intern_scope(struct pg_synthesis *, struct pg_source_scope);
const struct pg_evidence *pg_synthesis_scope_context(const struct pg_source_scope *);
int pg_synthesis_scope_wait(struct pg_synthesis *, struct pg_synthesis_job *, const struct pg_source_scope *);
const struct pg_object *pg_synthesis_source_binder(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_syntax *, size_t slot, const struct pg_object *);
const struct pg_source_scope *pg_synthesis_scope_bind(struct pg_synthesis *,
	const struct pg_source_scope *, struct pg_token, const struct pg_object *,
	struct pg_synthesis_job *, const struct pg_object *, struct pg_synthesis_job *, enum pg_source_association);
struct pg_synthesis_job *pg_synthesis_plain_rule(struct pg_synthesis *, enum pg_evidence_rule,
	const struct pg_object *, size_t, struct pg_synthesis_job *const *);
struct pg_synthesis_job *pg_synthesis_rule_premise(struct pg_synthesis *, const struct pg_synthesis_job *, size_t);
const struct pg_synthesis_job *pg_synthesis_operation_origin(const struct pg_synthesis_job *);
/* Follow an existing lexical alias edge, without evaluation or acceptance. */
struct pg_synthesis_job *pg_synthesis_source_origin(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_body(struct pg_synthesis *, struct pg_synthesis_job *, struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_body_input(const struct pg_synthesis_job *);
int pg_synthesis_typed_input(struct pg_synthesis *, const struct pg_evidence *, const struct pg_evidence *);
int pg_synthesis_source_value_kind(const struct pg_synthesis_job *);
int pg_synthesis_await_preparation(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_synthesis_job *, struct pg_synthesis_job **);
enum pg_comparison_status pg_synthesis_probe_advance(struct pg_synthesis *, struct pg_synthesis_job *, struct pg_comparison *);
enum pg_comparison_status pg_synthesis_independence(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_comparison *, const struct pg_term *, const struct pg_object *);

#endif
