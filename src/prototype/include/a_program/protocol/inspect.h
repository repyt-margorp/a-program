#ifndef A_PROGRAM_PROTOTYPE_PROTOCOL_INSPECT_H
#define A_PROGRAM_PROTOTYPE_PROTOCOL_INSPECT_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/protocol/graph.h"

#define PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY 64

enum prototype_core_inspect_kind {
	PROTOTYPE_CORE_INSPECT_COUNTS = 1,
	PROTOTYPE_CORE_INSPECT_TERM = 2,
	PROTOTYPE_CORE_INSPECT_CHILD_COUNT = 3,
	PROTOTYPE_CORE_INSPECT_CHILD = 4,
	PROTOTYPE_CORE_INSPECT_CONSTRUCTOR_SPINE = 5,
	PROTOTYPE_CORE_INSPECT_MATCH_CASE = 6,
	PROTOTYPE_CORE_INSPECT_CASE_BINDER = 7,
	PROTOTYPE_CORE_INSPECT_IH_SCOPE = 8,
	PROTOTYPE_CORE_INSPECT_COMPUTATION_FOLD_CLAUSE = 9,
	PROTOTYPE_CORE_INSPECT_PURE_FAMILY_PARTS = 10,
	PROTOTYPE_CORE_INSPECT_APP_SPINE = 11,
	PROTOTYPE_CORE_INSPECT_FREE_BINDING_OCCURS = 12
};

struct prototype_core_inspect_request {
	int kind;
	union {
		struct { uint32_t term_id; } term;
		struct { uint32_t term_id; } child_count;
		struct { uint32_t term_id; uint32_t child_index; } child;
		struct { uint32_t term_id; } constructor_spine;
		struct { uint32_t case_id; } match_case;
		struct { uint32_t binder_id; } case_binder;
		struct { uint32_t scope_id; } ih_scope;
		struct { uint32_t clause_id; } computation_fold_clause;
		struct { uint32_t term_id; } pure_family_parts;
		struct { uint32_t term_id; } app_spine;
		struct { uint32_t term_id; uint32_t binding_id; } free_binding_occurs;
	} as;
};

struct prototype_core_inspect_pure_family_parts {
	uint32_t binding_id;
	uint32_t body;
};

struct prototype_core_inspect_match_case {
	int label_symbol_id;
	struct prototype_match_case match_case;
};

struct prototype_core_inspect_constructor_spine {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t argument_count;
	uint32_t arguments[PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY];
};

struct prototype_core_inspect_app_spine {
	uint32_t head;
	uint32_t argument_count;
	uint32_t arguments[PROTOTYPE_CORE_INSPECT_SPINE_ARGUMENT_CAPACITY];
};

struct prototype_core_inspect_counts {
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t ih_scope_count;
	size_t computation_fold_clause_count;
};

/* A response is a detached Core value. It contains no TermDB pointer or read
 * capability and remains valid after the Core transition ends. */
struct prototype_core_inspect_response {
	int kind;
	uint64_t graph_revision;
	union {
		struct prototype_core_inspect_counts counts;
		struct prototype_term term;
		uint32_t child_count;
		struct prototype_term_child child;
		struct prototype_core_inspect_constructor_spine constructor_spine;
		struct prototype_core_inspect_match_case match_case;
		struct prototype_case_binder case_binder;
		struct prototype_ih_scope ih_scope;
		struct prototype_computation_fold_clause computation_fold_clause;
		struct prototype_core_inspect_pure_family_parts pure_family_parts;
		struct prototype_core_inspect_app_spine app_spine;
		int free_binding_occurs;
	} as;
};

#endif
