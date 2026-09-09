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
