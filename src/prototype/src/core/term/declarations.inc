#include "a_program/core/term.h"
#include "a_program/core/alpha_slot_env.h"
#include "a_program/core/term_schema.h"

#include "a_program/dimension/operator.h"
#include "a_program/support/storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define PROTOTYPE_EVALUATION_DEPTH_LIMIT 100000
#define PROTOTYPE_CANONICAL_BINDER_CAPACITY 512
#define PROTOTYPE_CANONICAL_HASH_OFFSET 1469598103934665603ULL
#define PROTOTYPE_CANONICAL_HASH_PRIME 1099511628211ULL

#define reserve_slot prototype_storage_reserve_slot

static int qualified_names_equal(
	struct prototype_qualified_name left,
	struct prototype_qualified_name right
) {
	return left.namespace_symbol_id == right.namespace_symbol_id &&
		left.name_symbol_id == right.name_symbol_id;
}

struct prototype_host_type_descriptor {
	int type_id;
	const char* debug_name;
	int term_tag;
	int bit_width;
};

static const struct prototype_host_type_descriptor host_types[] = {
	{
		PROTOTYPE_HOST_TYPE_TEXT,
		"Text",
		PROTOTYPE_TERM_PRIMITIVE_TEXT,
		0
	},
	{
		PROTOTYPE_HOST_TYPE_INT32,
		"Int",
		PROTOTYPE_TERM_PRIMITIVE_INT,
		32
	},
	{
		PROTOTYPE_HOST_TYPE_INT64,
		"Int64",
		PROTOTYPE_TERM_PRIMITIVE_INT64,
		64
	}
};

static const struct prototype_core_pure_primitive_declaration
pure_primitive_declarations[] = {
	{ PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT, 1, PROTOTYPE_HOST_TYPE_INVALID },
	{ PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT, 1, PROTOTYPE_HOST_TYPE_TEXT },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_ADD, 2, PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_SUB, 2, PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_MUL, 2, PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_NEG, 1, PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_ADD, 2, PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_SUB, 2, PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_MUL, 2, PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_NEG, 1, PROTOTYPE_HOST_TYPE_INT64 }
};

static const struct prototype_core_effect_operation_declaration
effect_operation_declarations[] = {
	{ PROTOTYPE_EFFECT_OPERATION_PRINT, PROTOTYPE_HOST_EFFECT_TERMINAL, 1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT },
	{ PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT, PROTOTYPE_HOST_EFFECT_NONE, 1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT },
	{ PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE, PROTOTYPE_HOST_EFFECT_NONE, 1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT },
	{ PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT, PROTOTYPE_HOST_EFFECT_NONE, 1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE }
};

struct host_pure_primitive_implementation {
	int primitive_id;
	int oracle_kind;
};

static const struct host_pure_primitive_implementation
host_pure_primitive_implementations[] = {
	{ PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT, PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT },
	{ PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT, PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_ADD, PROTOTYPE_HOST_ORACLE_INT_ADD },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_SUB, PROTOTYPE_HOST_ORACLE_INT_SUB },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_MUL, PROTOTYPE_HOST_ORACLE_INT_MUL },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_NEG, PROTOTYPE_HOST_ORACLE_INT_NEG },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_ADD, PROTOTYPE_HOST_ORACLE_INT_ADD },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_SUB, PROTOTYPE_HOST_ORACLE_INT_SUB },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_MUL, PROTOTYPE_HOST_ORACLE_INT_MUL },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_NEG, PROTOTYPE_HOST_ORACLE_INT_NEG }
};

struct host_effect_operation_implementation {
	int operation_id;
	int oracle_kind;
};

static const struct host_effect_operation_implementation
host_effect_operation_implementations[] = {
	{ PROTOTYPE_EFFECT_OPERATION_PRINT, PROTOTYPE_HOST_ORACLE_PRINT }
};

static const struct prototype_host_type_descriptor* host_type_by_id(int type_id) {
	for (size_t i = 0; i < sizeof(host_types) / sizeof(host_types[0]); ++i) {
		if (host_types[i].type_id == type_id) {
			return &host_types[i];
		}
	}
	return NULL;
}

int prototype_term_host_type_from_term_tag(int tag, int* p_type_id) {
	if (!p_type_id) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(host_types) / sizeof(host_types[0]); ++i) {
		if (host_types[i].term_tag == tag) {
			*p_type_id = host_types[i].type_id;
			return 0;
		}
	}
	return 1;
}

int prototype_term_host_type_bit_width(int type_id) {
	const struct prototype_host_type_descriptor* type = host_type_by_id(type_id);
	return type ? type->bit_width : 0;
}

size_t prototype_term_host_type_count(void) {
	return sizeof(host_types) / sizeof(host_types[0]);
}

int prototype_term_host_type_at(size_t index, int* p_type_id) {
	if (!p_type_id || index >= prototype_term_host_type_count()) {
		return -1;
	}
	*p_type_id = host_types[index].type_id;
	return 0;
}

const struct prototype_core_pure_primitive_declaration*
prototype_core_pure_primitive_declaration(int primitive_id) {
	for (size_t i = 0;
		i < sizeof(pure_primitive_declarations) /
			sizeof(pure_primitive_declarations[0]);
		++i) {
		if (pure_primitive_declarations[i].primitive_id == primitive_id) {
			return &pure_primitive_declarations[i];
		}
	}
	return NULL;
}

const struct prototype_core_effect_operation_declaration*
prototype_core_effect_operation_declaration(int operation_id) {
	for (size_t i = 0;
		i < sizeof(effect_operation_declarations) / sizeof(effect_operation_declarations[0]);
		++i) {
		if (effect_operation_declarations[i].operation_id == operation_id) {
			return &effect_operation_declarations[i];
		}
	}
	return NULL;
}

static size_t core_pure_primitive_count(void) {
	return sizeof(pure_primitive_declarations) /
		sizeof(pure_primitive_declarations[0]);
}

static size_t core_effect_operation_count(void) {
	return sizeof(effect_operation_declarations) /
		sizeof(effect_operation_declarations[0]);
}

static void core_intrinsic_fingerprint_mix(uint64_t* p_hash, uint64_t value) {
	for (unsigned i = 0; i < 8; ++i) {
		*p_hash ^= (value >> (i * 8)) & UINT64_C(0xff);
		*p_hash *= UINT64_C(1099511628211);
	}
}

uint64_t prototype_core_intrinsic_fingerprint(void) {
	uint64_t hash = UINT64_C(1469598103934665603);
	core_intrinsic_fingerprint_mix(&hash, core_pure_primitive_count());
	for (size_t i = 0; i < core_pure_primitive_count(); ++i) {
		const struct prototype_core_pure_primitive_declaration* declaration =
			&pure_primitive_declarations[i];
		core_intrinsic_fingerprint_mix(&hash, (uint32_t)declaration->primitive_id);
		core_intrinsic_fingerprint_mix(&hash, declaration->arity);
		core_intrinsic_fingerprint_mix(&hash, (uint32_t)declaration->result_host_type);
	}
	core_intrinsic_fingerprint_mix(&hash, core_effect_operation_count());
	for (size_t i = 0; i < core_effect_operation_count(); ++i) {
		const struct prototype_core_effect_operation_declaration* declaration =
			&effect_operation_declarations[i];
		core_intrinsic_fingerprint_mix(&hash, (uint32_t)declaration->operation_id);
		core_intrinsic_fingerprint_mix(&hash, declaration->required_host_effects);
		core_intrinsic_fingerprint_mix(&hash, declaration->arity);
		core_intrinsic_fingerprint_mix(&hash, (uint32_t)declaration->inner_policy);
		core_intrinsic_fingerprint_mix(
			&hash, (uint32_t)declaration->resumption_multiplicity
		);
	}
	return hash;
}

static const struct host_pure_primitive_implementation*
host_pure_primitive_implementation(int primitive_id
) {
	for (size_t i = 0;
		i < sizeof(host_pure_primitive_implementations) /
			sizeof(host_pure_primitive_implementations[0]);
		++i) {
		if (host_pure_primitive_implementations[i].primitive_id == primitive_id) {
			return &host_pure_primitive_implementations[i];
		}
	}
	return NULL;
}

static const struct host_effect_operation_implementation*
host_effect_operation_implementation(int operation_id) {
	for (size_t i = 0;
		i < sizeof(host_effect_operation_implementations) /
			sizeof(host_effect_operation_implementations[0]);
		++i) {
		if (host_effect_operation_implementations[i].operation_id == operation_id) {
			return &host_effect_operation_implementations[i];
		}
	}
	return NULL;
}

int prototype_term_constructor_spine_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* p_owner,
	uint32_t* p_constructor_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
) {
	if (!db || !p_head || !p_owner || !p_constructor_id || !p_argument_count ||
		term_id >= db->term_count || (argument_capacity > 0 && !arguments)) {
		return -1;
	}
	uint32_t reverse_arguments[64];
	uint32_t argument_count = 0;
	uint32_t head = term_id;
	while (head < db->term_count && db->terms[head].tag == PROTOTYPE_TERM_APP) {
		if (argument_count >= 64 || argument_count >= argument_capacity) {
			return -1;
		}
		reverse_arguments[argument_count++] = db->terms[head].as.app.argument;
		head = db->terms[head].as.app.function;
	}
	if (head >= db->term_count || db->terms[head].tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		return 1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		arguments[i] = reverse_arguments[argument_count - i - 1];
	}
	*p_head = head;
	*p_owner = db->terms[head].as.constructor.owner;
	*p_constructor_id = db->terms[head].as.constructor.constructor_id;
	*p_argument_count = argument_count;
	return 0;
}

int prototype_term_semantics(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_semantics* p_ret
) {
	if (!db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	int tag = db->terms[term_id].tag;
	uint32_t ignored_head;
	uint32_t ignored_owner;
	uint32_t ignored_constructor;
	uint32_t ignored_arguments[64];
	uint32_t ignored_argument_count;
	int constructor_spine = tag == PROTOTYPE_TERM_APP &&
		prototype_term_constructor_spine_info(
			db,
			term_id,
			&ignored_head,
			&ignored_owner,
			&ignored_constructor,
			ignored_arguments,
			64,
			&ignored_argument_count
		) == 0;
	memset(p_ret, 0, sizeof(*p_ret));
	switch (tag) {
		case PROTOTYPE_TERM_VAR:
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_RETURN:
		case PROTOTYPE_TERM_FORCE:
		case PROTOTYPE_TERM_OPERATION_REQUEST:
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			p_ret->layer = PROTOTYPE_TERM_LAYER_LAMBDA_CORE;
			break;
		case PROTOTYPE_TERM_APP:
			p_ret->layer = constructor_spine ?
				PROTOTYPE_TERM_LAYER_DATA : PROTOTYPE_TERM_LAYER_LAMBDA_CORE;
			break;
		case PROTOTYPE_TERM_MATCH:
			p_ret->layer = PROTOTYPE_TERM_LAYER_ELIMINATOR;
			break;
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_TYPE_FORMER:
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			case PROTOTYPE_TERM_TYPE_VIEW:
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
		case PROTOTYPE_TERM_THUNK_TYPE:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
				p_ret->layer = PROTOTYPE_TERM_LAYER_TYPE_FORMER;
					break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
		case PROTOTYPE_TERM_THUNK:
		case PROTOTYPE_TERM_TEXT_LITERAL:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
				case PROTOTYPE_TERM_INT_LITERAL:
					p_ret->layer = PROTOTYPE_TERM_LAYER_DATA;
				break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			p_ret->layer = PROTOTYPE_TERM_LAYER_LINK;
			p_ret->link_boundary = 1;
			break;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			p_ret->layer = PROTOTYPE_TERM_LAYER_PURE_PRIMITIVE;
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			p_ret->layer = PROTOTYPE_TERM_LAYER_EFFECT_OPERATION;
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			p_ret->layer = PROTOTYPE_TERM_LAYER_INDUCTION;
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			p_ret->layer = PROTOTYPE_TERM_LAYER_DIMENSION_ACTION;
			break;
		default:
			return -1;
	}

	switch (tag) {
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
		case PROTOTYPE_TERM_THUNK_TYPE:
		case PROTOTYPE_TERM_CONSTRUCTOR:
		case PROTOTYPE_TERM_THUNK:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_INTRODUCTION;
			break;
		case PROTOTYPE_TERM_MATCH:
		case PROTOTYPE_TERM_RETURN:
		case PROTOTYPE_TERM_FORCE:
		case PROTOTYPE_TERM_OPERATION_REQUEST:
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_ELIMINATOR;
			break;
		case PROTOTYPE_TERM_APP:
			p_ret->whnf_role = constructor_spine ?
				PROTOTYPE_TERM_WHNF_INTRODUCTION : PROTOTYPE_TERM_WHNF_ELIMINATOR;
			break;
		case PROTOTYPE_TERM_VAR:
		case PROTOTYPE_TERM_EXTERNAL_REF:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_NEUTRAL;
			break;
		default:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_ATOMIC;
			break;
	}

	p_ret->binds_term_variable =
		tag == PROTOTYPE_TERM_LAMBDA ||
		tag == PROTOTYPE_TERM_PI ||
		tag == PROTOTYPE_TERM_MATCH;
	p_ret->evaluates_scrutinee = tag == PROTOTYPE_TERM_MATCH;
	if (tag == PROTOTYPE_TERM_APP) {
		p_ret->application_role = constructor_spine ?
			PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION :
			PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION;
	}
	p_ret->reduces_by_beta = tag == PROTOTYPE_TERM_APP && !constructor_spine;
	return 0;
}

int prototype_term_core_projection(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_core_term
) {
	if (!db || !p_core_term || term_id >= db->term_count) {
		return -1;
	}
	uint32_t current = term_id;
	while (current < db->term_count &&
		db->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		uint32_t next = db->terms[current].as.type_view.core;
		if (next >= db->term_count || next == current) {
			return -1;
		}
		current = next;
	}
	if (current >= db->term_count) {
		return -1;
	}
	*p_core_term = current;
	return 0;
}

int prototype_term_child_count(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_count
) {
	if (!db || !p_count || term_id >= db->term_count) {
		return -1;
	}
	const struct prototype_term* term = &db->terms[term_id];
	uint32_t count;
	if (prototype_term_schema_fixed_child_count(term, &count) != 0) {
		return -1;
	}
	if (term->tag == PROTOTYPE_TERM_MATCH) {
		if (term->as.match.first_case > db->case_count ||
			term->as.match.case_count >
				db->case_count - term->as.match.first_case ||
			term->as.match.case_count > UINT32_MAX - count) {
			return -1;
		}
		count += term->as.match.case_count;
	} else if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		if (term->as.computation_fold.first_clause >
				db->computation_fold_clause_count ||
			term->as.computation_fold.clause_count >
				db->computation_fold_clause_count -
					term->as.computation_fold.first_clause ||
			term->as.computation_fold.clause_count > (UINT32_MAX - count) / 2) {
			return -1;
		}
		count += 2 * term->as.computation_fold.clause_count;
	}
	*p_count = count;
	return 0;
}

static int term_child_result(
	const struct prototype_term_db* db,
	int role,
	uint32_t ordinal,
	uint32_t term,
	struct prototype_term_child* p_child
) {
	if (!db || !p_child || term >= db->term_count) {
		return -1;
	}
	p_child->role = role;
	p_child->ordinal = ordinal;
	p_child->term = term;
	return 0;
}

int prototype_term_child_at(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t child_index,
	struct prototype_term_child* p_child
) {
	uint32_t child_count;
	if (!p_child || prototype_term_child_count(db, term_id, &child_count) != 0 ||
		child_index >= child_count) {
		return -1;
	}
	const struct prototype_term* term = &db->terms[term_id];
	uint32_t fixed_count;
	if (prototype_term_schema_fixed_child_count(term, &fixed_count) != 0) {
		return -1;
	}
	if (child_index < fixed_count) {
		struct prototype_term_child child;
		if (prototype_term_schema_fixed_child_at(
				term, child_index, &child
			) != 0) {
			return -1;
		}
		return term_child_result(
			db, child.role, child.ordinal, child.term, p_child
		);
	}
	uint32_t slice_index = child_index - fixed_count;
	if (term->tag == PROTOTYPE_TERM_MATCH) {
		return term_child_result(
			db,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
			slice_index,
			db->cases[term->as.match.first_case + slice_index].body,
			p_child
		);
	}
	if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		uint32_t clause_offset = slice_index / 2;
		const struct prototype_computation_fold_clause* clause =
			&db->computation_fold_clauses[
				term->as.computation_fold.first_clause + clause_offset
			];
		return term_child_result(
			db,
			slice_index % 2 == 0 ?
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION :
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
			clause_offset,
			slice_index % 2 == 0 ? clause->operation : clause->body,
			p_child
		);
	}
	return -1;
}

struct term_scope_env {
	uint32_t left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t count;
	uint32_t frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_count;
	uint32_t unmatched_frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t unmatched_frame_left_count;
	uint32_t unmatched_frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t unmatched_frame_right_count;
	uint32_t visited_frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t visited_frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t visited_frame_count;
	uint64_t* node_visit_count;
};

enum prototype_type_view_compare_mode {
	PROTOTYPE_TYPE_VIEW_COMPARE_VIEW = 1,
	PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
	PROTOTYPE_TYPE_VIEW_COMPARE_SOURCE
};

struct canonical_binder_env {
	struct prototype_alpha_slot_env binders;
	struct prototype_alpha_slot_env frames;
};

struct substitution_ih_scope_clone {
	uint32_t old_ih_scope_id;
	uint32_t new_ih_scope_id;
	uint64_t scope_key;
	const struct substitution_ih_scope_clone* previous;
};

struct substitution_binding_scope {
	uint32_t binding_id;
	uint64_t scope_key;
	const struct substitution_binding_scope* previous;
};

struct substitution_memo_entry {
	uint32_t term_id;
	uint32_t result;
	uint32_t type_view_projection;
	uint64_t binding_scope_key;
	uint64_t ih_scope_clone_key;
	uint32_t next;
};

struct substitution_context {
	const struct substitution_ih_scope_clone* ih_scope_clone;
	const struct substitution_binding_scope* binding_scope;
	const struct prototype_binding_replacement* bindings;
	size_t binding_count;
	uint32_t exact_term;
	uint32_t exact_replacement;
	int replace_exact_term;
	int type_views_are_boundaries;
	uint32_t type_view_projection;
	struct substitution_memo_entry* memo;
	uint32_t* memo_buckets;
	size_t memo_count;
	size_t memo_capacity;
	size_t memo_bucket_count;
	uint64_t next_scope_key;
};
