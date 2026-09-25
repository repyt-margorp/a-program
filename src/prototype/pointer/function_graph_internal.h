#ifndef A_PROGRAM_POINTER_FUNCTION_GRAPH_INTERNAL_H
#define A_PROGRAM_POINTER_FUNCTION_GRAPH_INTERNAL_H

#include "function_graph.h"

/* Shared source plan; schema construction and witness construction do not
 * independently rediscover call sites or constructor refinements. */
/* Published calls form an immutable prefix. Schema placement belongs to
 * a case's layout, not to the shared source call. */
struct graph_call {
	size_t field, hypothesis;
	size_t field_arity;
	const struct pg_evidence **field_arguments;
	const struct pg_evidence *child;
	const struct pg_evidence **arguments;
	const struct pg_evidence *result_context;
	const struct pg_evidence *helper_source, *helper_environment;
	const struct pg_evidence **helper_arguments;
	size_t helper_arity;
	const struct pg_function_graph_work *helper;
	const struct graph_call *previous;
};

struct graph_call_layout {
	const struct graph_call *call;
	size_t slot;
	const struct pg_evidence *layout_output;
	struct graph_call_layout *field_next;
};

struct graph_case {
	size_t field_count, hypothesis_count, call_count;
	size_t first_call, child_count;
	/* Telescope preparation finishes before schema construction assigns a leaf. */
	union { size_t next_scope; size_t leaf; };
	const struct pg_evidence **scopes;
	size_t *hypothesis_fields;
	const struct pg_evidence *context, *computation, *output;
	struct pg_whnf_job *normalization;
	const struct graph_call *calls;
	struct graph_call_layout *executed, **ordered;
	const struct pg_function_graph_order *source_order;
	struct graph_continuation *continuations;
	struct graph_case *parent, *children, *pending, *leaf_next, *build_next;
	const struct pg_evidence *origin_map, *refinement, *discriminant, *split_formation;
	const struct pg_evidence *schema_refinement, *schema_base, *schema_map, *schema_input, *base_input, *result;
	const struct pg_object *constructor;
};

struct graph_continuation {
	const struct pg_evidence *function;
	const struct pg_evidence *argument;
	struct graph_continuation *next;
};

/* Suspended helper inspection, not another source/result cache. */
struct helper_cursor {
	struct pg_graph temporary;
	const struct pg_evidence *function, *environment;
	union { const struct pg_evidence *body; struct pg_function_source_cursor source; };
	struct graph_continuation *arguments, *next_argument;
	size_t count, forces;
	enum { HELPER_ARGUMENTS, HELPER_SOURCE, HELPER_BODY, HELPER_APPLY, HELPER_MATCH, HELPER_READY } phase;
};

struct case_cursor {
	const struct pg_evidence *map, *context, *relation;
	const struct pg_evidence **arguments;
	size_t slot, mapped, argument;
};

struct pg_function_graph_state {
	struct pg_graph temporary;
	struct pg_typing *typing;
	struct pg_whnf_work *evaluation;
	const struct pg_evidence *body, *outer_context, *context, *argument_context;
	const struct pg_evidence *recursive_function;
	const struct pg_evidence *source_function;
	const struct pg_evidence *captured_input;
	const struct pg_evidence *specialization;
	struct graph_continuation *head_arguments;
	size_t specialized_arity;
	const struct pg_evidence *domain, *range, *self, *indices;
	const struct pg_evidence *result_context;
	const struct pg_evidence **arguments;
	size_t arity;
	const struct pg_evidence **index_arguments;
	size_t index_count;
	struct pg_inductive_instance input;
	const struct pg_evidence **results;
	struct graph_case *plans;
	struct graph_case *pending, *leaves, **leaf_tail;
	struct graph_case *building, **build_tail;
	/* Helper discovery ends before Self and its case schemas are built. */
	union { struct graph_call *waiting; struct case_cursor *branch; };
	struct pg_typed_query *view;
	union { struct pg_function_source_cursor source; struct helper_cursor helper; };
	size_t leaf_count;
	const struct pg_evidence *formation, *declaration;
	const struct pg_data_schema *schema;
	const struct pg_evidence *packet, *witness, *motive_context, *motive;
	const struct pg_evidence **witness_branches;
	size_t witness_next;
	enum pg_function_graph_status witness_status;
	size_t count, next;
	int cases, induction;
	enum { GRAPH_SOURCE, GRAPH_HEAD, GRAPH_INPUT, GRAPH_PARAMETERS, GRAPH_READY } preparation;
	uint64_t level;
	enum pg_totality totality;
	enum pg_function_graph_status status;
};

const struct pg_evidence *pg_function_plan_projection(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *proof);
const struct pg_evidence *pg_function_plan_argument_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument);
const struct pg_evidence *pg_function_plan_input_substitution(struct pg_function_graph_state *s,
	const struct pg_evidence *context, const struct pg_evidence *argument,
	const struct pg_evidence *const *arguments);
const struct pg_evidence *pg_function_plan_map_value(struct pg_function_graph_state *s,
	const struct pg_evidence *map, const struct pg_evidence *value);
const struct pg_evidence *pg_function_plan_apply_relation(struct pg_function_graph_state *s,
	const struct pg_evidence *relation, const struct pg_evidence *map,
	const struct pg_evidence *output);
const struct pg_evidence *pg_function_plan_parameters_at(struct pg_function_graph_state *s,
	const struct pg_evidence *context);
const struct pg_evidence *pg_function_plan_helper_function(struct pg_function_graph_state *s,
	const struct graph_call *call, const struct pg_evidence *map,
	const struct pg_evidence *proof);

const struct pg_evidence *pg_function_plan_case_source_map(struct pg_function_graph_state *s,
	const struct graph_case *plan, const struct pg_evidence *context,
	const struct pg_evidence *argument, const struct pg_evidence *const *fields,
	const struct pg_evidence **arguments);

#endif
