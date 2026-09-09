#ifndef A_PROGRAM_POINTER_IDENTITY_INTERNAL_H
#define A_PROGRAM_POINTER_IDENTITY_INTERNAL_H

#include "identity.h"
#include "eval.h"
#include "graph_io.h"

struct action_binding {
	const struct pg_object *source;
	const struct pg_object *arguments[3];
};
struct action_scope {
	const struct pg_term *source;
	const struct pg_term *body;
	size_t count;
	struct action_binding *bindings;
};
struct action_scope_work {
	struct action_scope scope;
	const struct pg_argument *arguments;
	const struct pg_term *cursor;
	size_t position;
	size_t center;
};
extern const struct pg_eval_work_operation pg_action_scope_operation;
struct pg_eval_configuration;
/* Raw scope discovery, before binding preparation. Cursor and caller argument
 * tails use the same configuration forest; reading does not scan the source. */
int pg_action_scope_work_write(FILE *file, const struct action_scope_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner);
int pg_action_scope_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_scope_work **work, size_t *count, const struct pg_eval_configuration **roots);
struct action_result_work {
	struct pg_graph *graph;
	const struct action_binding *bindings;
	const struct pg_term *result;
	size_t remaining;
	size_t discard;
};
extern const struct pg_eval_work_operation pg_action_result_operation;
/* One live result task can share binding storage with any retained scopes. */
int pg_action_ownership_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	const struct action_result_work *result, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *owner);
int pg_action_ownership_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *scope_count, struct action_scope *const **scopes, struct action_result_work **result,
	size_t *count, const struct pg_term *const **roots);
struct action_body_work {
	struct pg_comparison comparison;
	struct action_scope scope;
	const struct pg_term *answer;
	const struct pg_term *cursor;
	struct pg_graph *arena, *output;
	size_t position;
	enum { BODY_COMPARE, BODY_COLLECT, BODY_WRAP, BODY_READY } phase;
};

extern const struct pg_eval_work_operation pg_action_body_operation;
struct higher_scope_work {
	struct pg_graph *graph, *arena;
	const struct pg_term *source, *cursor, *body;
	const struct pg_object **binders;
	size_t arity, position, lambda_count;
	int wrapping, collecting;
};
extern const struct pg_eval_work_operation pg_higher_scope_operation;
/* Raw higher-scope progress, not an admitted Identity derivation. Extra roots
 * share one Term table. Restore actual work into arena/output, then use the
 * original descriptor; reading neither analyzes scope nor creates binders. */
int pg_higher_scope_write(FILE *file, const struct higher_scope_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_higher_scope_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct higher_scope_work **work, size_t *count, const struct pg_term *const **roots);
int pg_identity_continuation_uses_scope(const struct pg_eval_continuation *continuation);

/* Raw scope ownership, including unallocated/partially prepared bindings.
 * Extra Term roots use the same relocation table. No action or binder
 * synthesis is performed while reading. Storage belongs to arena/output. */
int pg_action_scopes_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_action_scopes_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *scope_count, struct action_scope *const **scopes, size_t *count, const struct pg_term *const **roots);
int pg_action_scope_write(FILE *file, const struct action_scope *scope,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_action_scope_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_scope **scope, size_t *count, const struct pg_term *const **roots);

/* Raw owner payload, not a full evaluator or accepted Identity evidence.
 * Extra roots share the comparison's relocation table. Restored work and its
 * binding array belong to arena, terms to output. Destroy via the operation
 * descriptor before either graph. The caller restores the surrounding machine. */
int pg_action_body_write(FILE *file, const struct action_body_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_action_body_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_body_work **work, size_t *count, const struct pg_term *const **roots);

struct pg_eval_configuration;
/* The same payload nested in a configuration forest, with one Term table.
 * This retains caller closures/arguments, not machine flags or Demand frames. */
int pg_action_body_configurations_write(FILE *file, const struct action_body_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner);
int pg_action_body_configurations_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_body_work **work, size_t *count, const struct pg_eval_configuration **roots);

#endif
