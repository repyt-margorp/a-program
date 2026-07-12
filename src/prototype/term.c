#include "term.h"

#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#define PROTOTYPE_EVALUATION_DEPTH_LIMIT 100000
#define PROTOTYPE_CANONICAL_BINDER_CAPACITY 512
#define PROTOTYPE_CANONICAL_HASH_OFFSET 1469598103934665603ULL
#define PROTOTYPE_CANONICAL_HASH_PRIME 1099511628211ULL

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

static int qualified_names_equal(
	struct prototype_qualified_name left,
	struct prototype_qualified_name right
) {
	return left.namespace_symbol_id == right.namespace_symbol_id &&
		left.name_symbol_id == right.name_symbol_id;
}

struct prototype_host_type_descriptor {
	int type_id;
	const char* source_name;
	const char* debug_name;
	int term_tag;
	int type_expr_tag;
	int bit_width;
	const char* aliases[2];
};

static const struct prototype_host_type_descriptor host_types[] = {
	{
		PROTOTYPE_HOST_TYPE_TEXT,
		"Text",
		"Text",
		PROTOTYPE_TERM_PRIMITIVE_TEXT,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT,
		0,
		{ NULL, NULL }
	},
	{
		PROTOTYPE_HOST_TYPE_INT32,
		"Int",
		"Int",
		PROTOTYPE_TERM_PRIMITIVE_INT,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT,
		32,
		{ "Int32", NULL }
	},
	{
		PROTOTYPE_HOST_TYPE_INT64,
		"Int64",
		"Int64",
		PROTOTYPE_TERM_PRIMITIVE_INT64,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64,
		64,
		{ NULL, NULL }
	}
};

static const struct prototype_host_intrinsic_signature host_intrinsics[] = {
	{
			PROTOTYPE_TERM_INTRINSIC_PRINT,
			"print",
			PROTOTYPE_HOST_ORACLE_PRINT,
			PROTOTYPE_HOST_EFFECT_TERMINAL,
			1,
			{ PROTOTYPE_HOST_TYPE_TEXT, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_TEXT
	},
	{
			PROTOTYPE_TERM_INTRINSIC_TEXT_TO_NAT,
			"text_to_nat",
			PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT,
			PROTOTYPE_HOST_EFFECT_NONE,
			1,
			{ PROTOTYPE_HOST_TYPE_TEXT, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INVALID
	},
	{
			PROTOTYPE_TERM_INTRINSIC_NAT_TO_TEXT,
			"nat_to_text",
			PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT,
			PROTOTYPE_HOST_EFFECT_NONE,
			1,
			{ PROTOTYPE_HOST_TYPE_INVALID, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_TEXT
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT_ADD,
			"int_add",
			PROTOTYPE_HOST_ORACLE_INT_ADD,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT_SUB,
			"int_sub",
			PROTOTYPE_HOST_ORACLE_INT_SUB,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT_MUL,
			"int_mul",
			PROTOTYPE_HOST_ORACLE_INT_MUL,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT_NEG,
			"int_neg",
			PROTOTYPE_HOST_ORACLE_INT_NEG,
			PROTOTYPE_HOST_EFFECT_NONE,
			1,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT64_ADD,
			"int64_add",
			PROTOTYPE_HOST_ORACLE_INT_ADD,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT64_SUB,
			"int64_sub",
			PROTOTYPE_HOST_ORACLE_INT_SUB,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT64_MUL,
			"int64_mul",
			PROTOTYPE_HOST_ORACLE_INT_MUL,
			PROTOTYPE_HOST_EFFECT_NONE,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_TERM_INTRINSIC_INT64_NEG,
			"int64_neg",
			PROTOTYPE_HOST_ORACLE_INT_NEG,
			PROTOTYPE_HOST_EFFECT_NONE,
			1,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INT64
	}
};

static const struct prototype_host_type_descriptor* host_type_by_id(int type_id) {
	for (size_t i = 0; i < sizeof(host_types) / sizeof(host_types[0]); ++i) {
		if (host_types[i].type_id == type_id) {
			return &host_types[i];
		}
	}
	return NULL;
}

int prototype_term_host_type_from_source_name(const char* name, int* p_type_id) {
	if (!name || !p_type_id) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(host_types) / sizeof(host_types[0]); ++i) {
		if (strcmp(name, host_types[i].source_name) == 0) {
			*p_type_id = host_types[i].type_id;
			return 0;
		}
		for (size_t j = 0; j < sizeof(host_types[i].aliases) / sizeof(host_types[i].aliases[0]); ++j) {
			if (host_types[i].aliases[j] && strcmp(name, host_types[i].aliases[j]) == 0) {
				*p_type_id = host_types[i].type_id;
				return 0;
			}
		}
	}
	return 1;
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

int prototype_term_host_type_from_type_expr_tag(int tag, int* p_type_id) {
	if (!p_type_id) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(host_types) / sizeof(host_types[0]); ++i) {
		if (host_types[i].type_expr_tag == tag) {
			*p_type_id = host_types[i].type_id;
			return 0;
		}
	}
	return 1;
}

const char* prototype_term_host_type_source_name(int type_id) {
	const struct prototype_host_type_descriptor* type = host_type_by_id(type_id);
	return type ? type->source_name : NULL;
}

const char* prototype_term_host_type_debug_name(int type_id) {
	const struct prototype_host_type_descriptor* type = host_type_by_id(type_id);
	return type ? type->debug_name : NULL;
}

int prototype_term_host_type_term_tag(int type_id) {
	const struct prototype_host_type_descriptor* type = host_type_by_id(type_id);
	return type ? type->term_tag : 0;
}

int prototype_term_host_type_expr_tag(int type_id) {
	const struct prototype_host_type_descriptor* type = host_type_by_id(type_id);
	return type ? type->type_expr_tag : 0;
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

const struct prototype_host_intrinsic_signature* prototype_term_host_intrinsic_signature(
	int intrinsic_id
) {
	for (size_t i = 0; i < sizeof(host_intrinsics) / sizeof(host_intrinsics[0]); ++i) {
		if (host_intrinsics[i].intrinsic_id == intrinsic_id) {
			return &host_intrinsics[i];
		}
	}
	return NULL;
}

int prototype_term_host_intrinsic_from_source_name(const char* name, int* p_intrinsic_id) {
	if (!name || !p_intrinsic_id) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(host_intrinsics) / sizeof(host_intrinsics[0]); ++i) {
		if (strcmp(name, host_intrinsics[i].source_name) == 0) {
			*p_intrinsic_id = host_intrinsics[i].intrinsic_id;
			return 0;
		}
	}
	return 1;
}

int prototype_term_semantics(int tag, struct prototype_term_semantics* p_ret) {
	if (!p_ret) {
		return -1;
	}
	memset(p_ret, 0, sizeof(*p_ret));
	switch (tag) {
		case PROTOTYPE_TERM_VAR:
		case PROTOTYPE_TERM_APP:
		case PROTOTYPE_TERM_LAMBDA:
			p_ret->layer = PROTOTYPE_TERM_LAYER_LAMBDA_CORE;
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
				case PROTOTYPE_TERM_EFFECT_LABEL:
				case PROTOTYPE_TERM_EFFECT_TYPE:
					p_ret->layer = PROTOTYPE_TERM_LAYER_TYPE_FORMER;
					break;
			case PROTOTYPE_TERM_CONSTRUCTOR:
				case PROTOTYPE_TERM_TEXT_LITERAL:
				case PROTOTYPE_TERM_INT_LITERAL:
					p_ret->layer = PROTOTYPE_TERM_LAYER_DATA;
				break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			p_ret->layer = PROTOTYPE_TERM_LAYER_LINK;
			p_ret->link_boundary = 1;
			break;
		case PROTOTYPE_TERM_INTRINSIC:
			p_ret->layer = PROTOTYPE_TERM_LAYER_HOST;
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			p_ret->layer = PROTOTYPE_TERM_LAYER_INDUCTION;
			break;
		default:
			return -1;
	}

	switch (tag) {
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_CONSTRUCTOR:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_INTRODUCTION;
			break;
		case PROTOTYPE_TERM_APP:
		case PROTOTYPE_TERM_MATCH:
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			p_ret->whnf_role = PROTOTYPE_TERM_WHNF_ELIMINATOR;
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
	p_ret->reduces_by_beta = tag == PROTOTYPE_TERM_APP;
	return 0;
}

struct shape_binder_env {
	uint32_t left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t count;
};

enum prototype_type_view_compare_mode {
	PROTOTYPE_TYPE_VIEW_COMPARE_VIEW = 1,
	PROTOTYPE_TYPE_VIEW_COMPARE_CORE
};

struct canonical_binder_env {
	uint32_t binder_id[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t slot[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
};

struct substitution_frame_scope {
	uint32_t old_frame_id;
	uint32_t new_frame_id;
	const struct substitution_frame_scope* previous;
};

struct substitution_context {
	const struct substitution_frame_scope* frame_scope;
	struct prototype_type_declaration_db* type_declarations;
};

static int substitute_term(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	uint32_t* p_ret
);

static int substitute_term_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	struct substitution_context* ctx,
	uint32_t* p_ret
);

static void canonical_hash_mix_u32(uint64_t* p_hash, uint32_t value) {
	*p_hash ^= (uint64_t)value;
	*p_hash *= PROTOTYPE_CANONICAL_HASH_PRIME;
}

static void canonical_hash_mix_tag(uint64_t* p_hash, uint32_t tag) {
	canonical_hash_mix_u32(p_hash, 0x9e3779b9U);
	canonical_hash_mix_u32(p_hash, tag);
}

static int shape_env_push(
	struct shape_binder_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	if (!env || env->count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return -1;
	}
	env->left[env->count] = left_binder;
	env->right[env->count] = right_binder;
	env->count++;
	return 0;
}

static int shape_env_lookup_left(
	const struct shape_binder_env* env,
	uint32_t left_binder,
	uint32_t* p_right_binder
) {
	if (!env || !p_right_binder) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->left[index] == left_binder) {
			*p_right_binder = env->right[index];
			return 1;
		}
	}
	return 0;
}

static int shape_env_contains_right(
	const struct shape_binder_env* env,
	uint32_t right_binder
) {
	if (!env) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		if (env->right[i - 1] == right_binder) {
			return 1;
		}
	}
	return 0;
}

static int shape_vars_equal(
	const struct shape_binder_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	uint32_t mapped_right;
	if (shape_env_lookup_left(env, left_binder, &mapped_right)) {
		return mapped_right == right_binder;
	}
	if (shape_env_contains_right(env, right_binder)) {
		return 0;
	}
	return left_binder == right_binder;
}

static int shape_terms_equal_at_depth(
	const struct prototype_term_db* db,
	uint32_t left_id,
	uint32_t right_id,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
);

static int shape_match_cases_equal_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_match_case* left_case,
	const struct prototype_match_case* right_case,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
) {
	if (left_case->constructor_id != right_case->constructor_id ||
		left_case->binder_count != right_case->binder_count) {
		return 0;
	}
	if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		right_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		if (left_case->constructor_owner != right_case->constructor_owner) {
			return 0;
		}
		} else if (!shape_terms_equal_at_depth(
				db,
				left_case->constructor_owner,
				right_case->constructor_owner,
				env,
				type_view_compare_mode,
				ignore_match_frames,
				depth + 1
			)) {
		return 0;
	}

	uint32_t saved_count = env->count;
	for (uint32_t i = 0; i < left_case->binder_count; ++i) {
		const struct prototype_case_binder* left_binder =
			&db->case_binders[left_case->first_binder + i];
		const struct prototype_case_binder* right_binder =
			&db->case_binders[right_case->first_binder + i];
		if (shape_env_push(env, left_binder->binder_id, right_binder->binder_id) != 0) {
			env->count = saved_count;
			return 0;
		}
	}
	int equal = shape_terms_equal_at_depth(
		db,
		left_case->body,
		right_case->body,
		env,
		type_view_compare_mode,
		ignore_match_frames,
		depth + 1
	);
	env->count = saved_count;
	return equal;
}

static uint32_t shape_term_compare_id(
	const struct prototype_term_db* db,
	uint32_t term_id,
	int type_view_compare_mode
) {
	uint32_t current = term_id;
	for (uint32_t depth = 0;
		type_view_compare_mode == PROTOTYPE_TYPE_VIEW_COMPARE_CORE &&
			current < db->term_count &&
			db->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW &&
			depth < 32;
		++depth) {
		current = db->terms[current].as.type_view.core;
	}
	return current;
}

static int shape_terms_equal_at_depth(
	const struct prototype_term_db* db,
	uint32_t left_id,
	uint32_t right_id,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
) {
	if (!db || !env ||
		left_id >= db->term_count ||
		right_id >= db->term_count ||
		depth > 256) {
		return 0;
	}
	left_id = shape_term_compare_id(db, left_id, type_view_compare_mode);
	right_id = shape_term_compare_id(db, right_id, type_view_compare_mode);
	if (left_id >= db->term_count || right_id >= db->term_count) {
		return 0;
	}
	const struct prototype_term* left = &db->terms[left_id];
	const struct prototype_term* right = &db->terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return shape_vars_equal(env, left->as.var.binder_id, right->as.var.binder_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id == right->as.constructor.constructor_id &&
				shape_terms_equal_at_depth(
					db,
					left->as.constructor.owner,
					right->as.constructor.owner,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				);
		case PROTOTYPE_TERM_APP:
			return shape_terms_equal_at_depth(
					db,
					left->as.app.function,
					right->as.app.function,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				) &&
				shape_terms_equal_at_depth(
					db,
					left->as.app.argument,
					right->as.app.argument,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			if (shape_env_push(env, left->as.lambda.binder_id, right->as.lambda.binder_id) != 0) {
				env->count = saved_count;
				return 0;
			}
			int equal = shape_terms_equal_at_depth(
				db,
					left->as.lambda.body,
					right->as.lambda.body,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_PI:
			return shape_terms_equal_at_depth(
					db,
						left->as.pi.domain,
						right->as.pi.domain,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
				shape_terms_equal_at_depth(
					db,
						left->as.pi.codomain_family,
						right->as.pi.codomain_family,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
		case PROTOTYPE_TERM_MATCH:
			if (!shape_terms_equal_at_depth(
					db,
						left->as.match.scrutinee,
						right->as.match.scrutinee,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) ||
				left->as.match.case_count != right->as.match.case_count ||
				(!ignore_match_frames &&
					left->as.match.frame_id != right->as.match.frame_id)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&db->cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&db->cases[right->as.match.first_case + i];
				if (!shape_match_cases_equal_at_depth(
						db,
							left_case,
							right_case,
							env,
							type_view_compare_mode,
							ignore_match_frames,
							depth + 1
						)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return left->as.type_former.representation_id == right->as.type_former.representation_id;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return left->as.type_declaration.type_id == right->as.type_declaration.type_id;
		case PROTOTYPE_TERM_TYPE_VIEW:
			return left->as.type_view.view_type_id == right->as.type_view.view_type_id &&
				shape_terms_equal_at_depth(
					db,
						left->as.type_view.core,
						right->as.type_view.core,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
				shape_terms_equal_at_depth(
					db,
						left->as.type_view.source,
						right->as.type_view.source,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (!ignore_match_frames &&
				left->as.induction_hypothesis.frame_id !=
					right->as.induction_hypothesis.frame_id) {
				return 0;
			}
			if (ignore_match_frames &&
				left->as.induction_hypothesis.frame_id !=
					right->as.induction_hypothesis.frame_id) {
				if (left->as.induction_hypothesis.frame_id >= db->match_frame_count ||
					right->as.induction_hypothesis.frame_id >= db->match_frame_count ||
					!db->match_frames[left->as.induction_hypothesis.frame_id].key.is_linkable ||
					!db->match_frames[right->as.induction_hypothesis.frame_id].key.is_linkable ||
					db->match_frames[left->as.induction_hypothesis.frame_id].key.match_key.hash !=
						db->match_frames[right->as.induction_hypothesis.frame_id].key.match_key.hash ||
					db->match_frames[left->as.induction_hypothesis.frame_id].key.match_key.node_count !=
						db->match_frames[right->as.induction_hypothesis.frame_id].key.match_key.node_count) {
					return 0;
				}
			}
			return
				shape_terms_equal_at_depth(
					db,
						left->as.induction_hypothesis.argument,
						right->as.induction_hypothesis.argument,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return left->as.universe_var.level_var == right->as.universe_var.level_var;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 1;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 1;
			case PROTOTYPE_TERM_INT_LITERAL:
				return left->as.int_literal.value == right->as.int_literal.value;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return qualified_names_equal(left->as.external_ref.name, right->as.external_ref.name);
			case PROTOTYPE_TERM_INTRINSIC:
				return left->as.intrinsic.intrinsic_id == right->as.intrinsic.intrinsic_id &&
					left->as.intrinsic.symbol_id == right->as.intrinsic.symbol_id &&
					left->as.intrinsic.type_symbol_id == right->as.intrinsic.type_symbol_id;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				return left->as.effect_label.effects == right->as.effect_label.effects;
			case PROTOTYPE_TERM_EFFECT_TYPE:
				return shape_terms_equal_at_depth(
						db,
						left->as.effect_type.label,
						right->as.effect_type.label,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
					shape_terms_equal_at_depth(
						db,
						left->as.effect_type.result,
						right->as.effect_type.result,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
			default:
				return 0;
		}
	}

static int canonical_keys_equal_local(
	const struct prototype_term_canonical_key* left,
	const struct prototype_term_canonical_key* right
) {
	return left && right &&
		left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->bound_binder_count == right->bound_binder_count &&
		left->free_binder_count == right->free_binder_count &&
		left->has_frame_local_reference == right->has_frame_local_reference &&
		left->has_type_local_reference == right->has_type_local_reference &&
		left->has_type_name_reference == right->has_type_name_reference &&
		left->has_type_universe_reference == right->has_type_universe_reference;
}

static int cross_shape_terms_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_id,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_id,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
);

static int cross_type_formers_equal(
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_type_id,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_type_id
) {
	if (left_type_declarations && right_type_declarations &&
		left_type_declarations != right_type_declarations) {
		return 0;
	}
	return left_type_id == right_type_id;
}

static int cross_frame_keys_equal(
	const struct prototype_term_db* left_db,
	uint32_t left_frame,
	const struct prototype_term_db* right_db,
	uint32_t right_frame
) {
	if (!left_db || !right_db ||
		left_frame >= left_db->match_frame_count ||
		right_frame >= right_db->match_frame_count ||
		!left_db->match_frames[left_frame].key.is_linkable ||
		!right_db->match_frames[right_frame].key.is_linkable) {
		return 0;
	}
	const struct prototype_match_frame_key* left = &left_db->match_frames[left_frame].key;
	const struct prototype_match_frame_key* right = &right_db->match_frames[right_frame].key;
	return left->case_count == right->case_count &&
		canonical_keys_equal_local(&left->match_key, &right->match_key);
}

static int cross_shape_match_cases_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	const struct prototype_match_case* left_case,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	const struct prototype_match_case* right_case,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
) {
	if (!left_db || !left_case || !right_db || !right_case || !env || depth > 256) {
		return 0;
	}
	if (left_case->constructor_id != right_case->constructor_id ||
		left_case->binder_count != right_case->binder_count) {
		return 0;
	}
	if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		right_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		if (left_case->constructor_owner != right_case->constructor_owner) {
			return 0;
		}
	} else if (!cross_shape_terms_equal_at_depth(
			left_db,
			left_type_declarations,
			left_case->constructor_owner,
			right_db,
				right_type_declarations,
				right_case->constructor_owner,
				env,
				type_view_compare_mode,
				ignore_match_frames,
				depth + 1
			)) {
		return 0;
	}

	uint32_t saved_count = env->count;
	for (uint32_t i = 0; i < left_case->binder_count; ++i) {
		const struct prototype_case_binder* left_binder =
			&left_db->case_binders[left_case->first_binder + i];
		const struct prototype_case_binder* right_binder =
			&right_db->case_binders[right_case->first_binder + i];
		if (shape_env_push(env, left_binder->binder_id, right_binder->binder_id) != 0) {
			env->count = saved_count;
			return 0;
		}
	}
	int equal = cross_shape_terms_equal_at_depth(
		left_db,
		left_type_declarations,
		left_case->body,
		right_db,
		right_type_declarations,
		right_case->body,
		env,
		type_view_compare_mode,
		ignore_match_frames,
		depth + 1
	);
	env->count = saved_count;
	return equal;
}

static int cross_shape_terms_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_id,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_id,
	struct shape_binder_env* env,
	int type_view_compare_mode,
	int ignore_match_frames,
	uint32_t depth
) {
	if (!left_db || !right_db || !env ||
		left_id >= left_db->term_count ||
		right_id >= right_db->term_count ||
		depth > 256) {
		return 0;
	}
	left_id = shape_term_compare_id(left_db, left_id, type_view_compare_mode);
	right_id = shape_term_compare_id(right_db, right_id, type_view_compare_mode);
	if (left_id >= left_db->term_count || right_id >= right_db->term_count) {
		return 0;
	}
	const struct prototype_term* left = &left_db->terms[left_id];
	const struct prototype_term* right = &right_db->terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return shape_vars_equal(env, left->as.var.binder_id, right->as.var.binder_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id == right->as.constructor.constructor_id &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.constructor.owner,
					right_db,
						right_type_declarations,
						right->as.constructor.owner,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
		case PROTOTYPE_TERM_APP:
			return cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.app.function,
					right_db,
						right_type_declarations,
						right->as.app.function,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.app.argument,
					right_db,
						right_type_declarations,
						right->as.app.argument,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			if (shape_env_push(env, left->as.lambda.binder_id, right->as.lambda.binder_id) != 0) {
				env->count = saved_count;
				return 0;
			}
			int equal = cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.lambda.body,
				right_db,
					right_type_declarations,
					right->as.lambda.body,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_PI:
			return cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.pi.domain,
					right_db,
						right_type_declarations,
						right->as.pi.domain,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.pi.codomain_family,
					right_db,
						right_type_declarations,
						right->as.pi.codomain_family,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
		case PROTOTYPE_TERM_MATCH:
			if (!cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.match.scrutinee,
					right_db,
						right_type_declarations,
						right->as.match.scrutinee,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) ||
				left->as.match.case_count != right->as.match.case_count ||
				(!ignore_match_frames &&
					left->as.match.frame_id != right->as.match.frame_id)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&left_db->cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&right_db->cases[right->as.match.first_case + i];
				if (!cross_shape_match_cases_equal_at_depth(
						left_db,
						left_type_declarations,
						left_case,
						right_db,
							right_type_declarations,
							right_case,
							env,
							type_view_compare_mode,
							ignore_match_frames,
							depth + 1
						)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return cross_type_formers_equal(
				left_type_declarations,
				left->as.type_former.representation_id,
				right_type_declarations,
				right->as.type_former.representation_id
			);
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return cross_type_formers_equal(
				left_type_declarations,
				left->as.type_declaration.type_id,
				right_type_declarations,
				right->as.type_declaration.type_id
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return left->as.type_view.view_type_id == right->as.type_view.view_type_id &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.type_view.core,
					right_db,
						right_type_declarations,
						right->as.type_view.core,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.type_view.source,
					right_db,
						right_type_declarations,
						right->as.type_view.source,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (!ignore_match_frames &&
				left->as.induction_hypothesis.frame_id !=
					right->as.induction_hypothesis.frame_id) {
				return 0;
			}
			if (ignore_match_frames &&
				left->as.induction_hypothesis.frame_id !=
					right->as.induction_hypothesis.frame_id &&
				!cross_frame_keys_equal(
					left_db,
					left->as.induction_hypothesis.frame_id,
					right_db,
					right->as.induction_hypothesis.frame_id
				)) {
				return 0;
			}
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.induction_hypothesis.argument,
				right_db,
					right_type_declarations,
					right->as.induction_hypothesis.argument,
					env,
					type_view_compare_mode,
					ignore_match_frames,
					depth + 1
				);
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return left->as.universe_var.level_var == right->as.universe_var.level_var;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 1;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 1;
			case PROTOTYPE_TERM_INT_LITERAL:
				return left->as.int_literal.value == right->as.int_literal.value;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return qualified_names_equal(left->as.external_ref.name, right->as.external_ref.name);
			case PROTOTYPE_TERM_INTRINSIC:
				return left->as.intrinsic.intrinsic_id == right->as.intrinsic.intrinsic_id &&
					left->as.intrinsic.symbol_id == right->as.intrinsic.symbol_id &&
					left->as.intrinsic.type_symbol_id == right->as.intrinsic.type_symbol_id;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				return left->as.effect_label.effects == right->as.effect_label.effects;
			case PROTOTYPE_TERM_EFFECT_TYPE:
				return cross_shape_terms_equal_at_depth(
						left_db,
						left_type_declarations,
						left->as.effect_type.label,
						right_db,
						right_type_declarations,
						right->as.effect_type.label,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					) &&
					cross_shape_terms_equal_at_depth(
						left_db,
						left_type_declarations,
						left->as.effect_type.result,
						right_db,
						right_type_declarations,
						right->as.effect_type.result,
						env,
						type_view_compare_mode,
						ignore_match_frames,
						depth + 1
					);
			default:
				return 0;
		}
	}

static int shape_equal_cross_db_with_type_view_mode(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int type_view_compare_mode,
	int ignore_match_frames,
	int* p_equal
) {
	if (!left_db || !right_db || !p_equal ||
		left >= left_db->term_count ||
		right >= right_db->term_count) {
		return -1;
	}

	struct shape_binder_env env;
	memset(&env, 0, sizeof(env));
	*p_equal = cross_shape_terms_equal_at_depth(
			left_db,
			left_type_declarations,
			left,
			right_db,
			right_type_declarations,
			right,
			&env,
			type_view_compare_mode,
			ignore_match_frames,
			0
		);
	return 0;
}

int prototype_term_view_shape_equal_cross_db(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int ignore_match_frames,
	int* p_equal
) {
	return shape_equal_cross_db_with_type_view_mode(
		left_db,
		left_type_declarations,
		left,
		right_db,
		right_type_declarations,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
		ignore_match_frames,
		p_equal
	);
}

int prototype_term_core_shape_equal_cross_db(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int ignore_match_frames,
	int* p_equal
) {
	return shape_equal_cross_db_with_type_view_mode(
		left_db,
		left_type_declarations,
		left,
		right_db,
		right_type_declarations,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
		ignore_match_frames,
		p_equal
	);
}

static int shape_equal_with_type_view_mode(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int type_view_compare_mode,
	int* p_equal
) {
	if (!db || !p_equal || left >= db->term_count || right >= db->term_count) {
		return -1;
	}

	struct shape_binder_env env;
	memset(&env, 0, sizeof(env));
	*p_equal = shape_terms_equal_at_depth(
		db,
		left,
		right,
		&env,
		type_view_compare_mode,
		1,
		0
	);
	return 0;
}

int prototype_term_view_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_with_type_view_mode(
		db,
		left,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
		p_equal
	);
}

int prototype_term_core_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_with_type_view_mode(
		db,
		left,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
		p_equal
	);
}

static int canonical_env_lookup(
	const struct canonical_binder_env* env,
	uint32_t binder_id,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->binder_id[index] == binder_id) {
			*p_slot = env->slot[index];
			return 1;
		}
	}
	return 0;
}

static int canonical_env_push(
	struct canonical_binder_env* env,
	uint32_t binder_id
) {
	if (!env || env->count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return -1;
	}
	env->binder_id[env->count] = binder_id;
	env->slot[env->count] = env->next_slot++;
	env->count++;
	return 0;
}

static int canonical_hash_term_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
);

static int canonical_hash_match_case_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
) {
	if (!db || !match_case || !env || !key || !p_hash || depth > 256) {
		return -1;
	}
	canonical_hash_mix_u32(p_hash, match_case->constructor_id);
	canonical_hash_mix_u32(p_hash, match_case->binder_count);
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		canonical_hash_mix_u32(p_hash, PROTOTYPE_INVALID_ID);
	} else if (canonical_hash_term_at_depth(
			db,
			type_declarations,
			match_case->constructor_owner,
			env,
			key,
			p_hash,
			canonicalize_frame_refs,
			depth + 1
		) != 0) {
		return -1;
	}

	uint32_t saved_count = env->count;
	uint32_t saved_next_slot = env->next_slot;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&db->case_binders[match_case->first_binder + i];
		if (canonical_env_push(env, binder->binder_id) != 0) {
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return -1;
		}
		key->bound_binder_count++;
	}
	int status = canonical_hash_term_at_depth(
		db,
		type_declarations,
		match_case->body,
		env,
		key,
		p_hash,
		canonicalize_frame_refs,
		depth + 1
	);
	env->count = saved_count;
	env->next_slot = saved_next_slot;
	return status;
}

static int canonical_hash_term_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
) {
	if (!db || !env || !key || !p_hash || term_id >= db->term_count || depth > 256) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	key->node_count++;
	canonical_hash_mix_tag(p_hash, (uint32_t)term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			uint32_t slot;
			if (canonical_env_lookup(env, term->as.var.binder_id, &slot)) {
				canonical_hash_mix_u32(p_hash, 1);
				canonical_hash_mix_u32(p_hash, slot);
			} else {
				canonical_hash_mix_u32(p_hash, 0);
				canonical_hash_mix_u32(p_hash, term->as.var.binder_id);
				key->free_binder_count++;
			}
			return 0;
		}
		case PROTOTYPE_TERM_CONSTRUCTOR:
			canonical_hash_mix_u32(p_hash, term->as.constructor.constructor_id);
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.constructor.owner,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.app.function,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.app.argument,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (canonical_env_push(env, term->as.lambda.binder_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.lambda.body,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return status;
		}
		case PROTOTYPE_TERM_PI:
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.pi.domain,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.pi.codomain_family,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			key->has_frame_local_reference =
				key->has_frame_local_reference ||
				(term->as.match.frame_id != PROTOTYPE_INVALID_ID &&
					!canonicalize_frame_refs &&
					(term->as.match.frame_id >= db->match_frame_count ||
						!db->match_frames[term->as.match.frame_id].key.is_linkable));
			canonical_hash_mix_u32(p_hash, term->as.match.case_count);
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.match.scrutinee,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				if (canonical_hash_match_case_at_depth(
						db,
						type_declarations,
						match_case,
						env,
						key,
						p_hash,
						canonicalize_frame_refs,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			key->has_type_local_reference = 1;
			canonical_hash_mix_u32(p_hash, term->as.type_former.representation_id);
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			key->has_type_local_reference = 1;
			canonical_hash_mix_u32(p_hash, term->as.type_declaration.type_id);
			return 0;
		case PROTOTYPE_TERM_TYPE_VIEW:
			key->has_type_local_reference = 1;
			canonical_hash_mix_u32(p_hash, term->as.type_view.view_type_id);
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.type_view.core,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.type_view.source,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (canonicalize_frame_refs) {
				canonical_hash_mix_u32(p_hash, 0);
			} else if (term->as.induction_hypothesis.frame_id < db->match_frame_count &&
				db->match_frames[term->as.induction_hypothesis.frame_id].key.is_linkable) {
				const struct prototype_match_frame_key* frame_key =
					&db->match_frames[term->as.induction_hypothesis.frame_id].key;
				canonical_hash_mix_u32(p_hash, 1);
				canonical_hash_mix_u32(p_hash, (uint32_t)frame_key->match_key.hash);
				canonical_hash_mix_u32(p_hash, (uint32_t)(frame_key->match_key.hash >> 32));
				canonical_hash_mix_u32(p_hash, frame_key->match_key.node_count);
				canonical_hash_mix_u32(p_hash, frame_key->case_count);
			} else {
				key->has_frame_local_reference = 1;
				canonical_hash_mix_u32(p_hash, 2);
				canonical_hash_mix_u32(p_hash, term->as.induction_hypothesis.frame_id);
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.induction_hypothesis.argument,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			canonical_hash_mix_u32(p_hash, term->as.universe_var.level_var);
			return 0;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.text_literal.text_symbol_id);
				return 0;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.int_literal.value);
				canonical_hash_mix_u32(p_hash, (uint32_t)((uint64_t)term->as.int_literal.value >> 32));
				return 0;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				canonical_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.namespace_symbol_id
				);
				canonical_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.name_symbol_id
				);
				return 0;
			case PROTOTYPE_TERM_INTRINSIC:
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.intrinsic_id);
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.symbol_id);
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.type_symbol_id);
				return 0;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				canonical_hash_mix_u32(p_hash, term->as.effect_label.effects);
				return 0;
			case PROTOTYPE_TERM_EFFECT_TYPE:
				if (canonical_hash_term_at_depth(
						db,
						type_declarations,
						term->as.effect_type.label,
						env,
						key,
						p_hash,
						canonicalize_frame_refs,
						depth + 1
					) != 0) {
					return -1;
				}
				return canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.effect_type.result,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				);
			default:
				return -1;
		}
	}

int prototype_term_canonical_key(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
) {
	return prototype_term_canonical_key_with_types(db, NULL, term_id, p_key);
}

int prototype_term_canonical_key_with_types(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
) {
	if (!db || !p_key || term_id >= db->term_count) {
		return -1;
	}

	struct canonical_binder_env env;
	uint64_t hash = PROTOTYPE_CANONICAL_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	memset(p_key, 0, sizeof(*p_key));
	if (canonical_hash_term_at_depth(
			db,
			type_declarations,
			term_id,
			&env,
			p_key,
			&hash,
			0,
			0
		) != 0) {
		return -1;
	}
	p_key->hash = hash;
	return 0;
}

static int compute_match_frame_key(
	const struct prototype_term_db* db,
	uint32_t match_term,
	struct prototype_match_frame_key* p_key
) {
	if (!db || !p_key || match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}

	struct canonical_binder_env env;
	uint64_t hash = PROTOTYPE_CANONICAL_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	memset(p_key, 0, sizeof(*p_key));
	if (canonical_hash_term_at_depth(
			db,
			NULL,
			match_term,
			&env,
			&p_key->match_key,
			&hash,
			1,
			0
		) != 0) {
		return -1;
	}
	p_key->match_key.hash = hash;
	p_key->case_count = db->terms[match_term].as.match.case_count;
	p_key->is_linkable =
		p_key->match_key.free_binder_count == 0 &&
		!p_key->match_key.has_frame_local_reference;
	return 0;
}

void prototype_term_db_init(
	struct prototype_term_db* db,
	struct prototype_term* terms,
	size_t term_capacity,
	struct prototype_match_case* cases,
	int* case_label_symbols,
	size_t case_capacity,
	struct prototype_case_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_match_frame* match_frames,
	size_t match_frame_capacity
) {
	memset(db, 0, sizeof(*db));
	db->terms = terms;
	db->term_capacity = term_capacity;
	db->cases = cases;
	db->case_label_symbols = case_label_symbols;
	db->case_capacity = case_capacity;
	db->case_binders = case_binders;
	db->case_binder_capacity = case_binder_capacity;
	db->match_frames = match_frames;
	db->match_frame_capacity = match_frame_capacity;
	for (uint32_t i = 0; i < PROTOTYPE_SCOPE_BINDER_CAPACITY; ++i) {
		db->scope_binders[i] = PROTOTYPE_INVALID_ID;
	}
	db->normalization_graph_revision = 1;
}

void prototype_term_normalization_cache_clear(struct prototype_term_db* db) {
	if (!db) {
		return;
	}
	memset(db->normalization_cache, 0, sizeof(db->normalization_cache));
	memset(&db->normalization_cache_stats, 0, sizeof(db->normalization_cache_stats));
	db->normalization_cache_next = 0;
}

void prototype_term_normalization_cache_get_stats(
	const struct prototype_term_db* db,
	struct prototype_term_normalization_cache_stats* p_stats
) {
	if (!p_stats) {
		return;
	}
	memset(p_stats, 0, sizeof(*p_stats));
	if (db) {
		*p_stats = db->normalization_cache_stats;
	}
}

static void invalidate_normalization_cache(struct prototype_term_db* db) {
	if (!db) {
		return;
	}
	db->normalization_graph_revision++;
	if (db->normalization_graph_revision == 0) {
		db->normalization_graph_revision = 1;
		prototype_term_normalization_cache_clear(db);
	}
}

void prototype_term_notify_graph_mutation(struct prototype_term_db* db) {
	invalidate_normalization_cache(db);
}

static int add_term(struct prototype_term_db* db, struct prototype_term term, uint32_t* p_ret) {
	if (!db || !p_ret || reserve_slot(db->term_count, db->term_capacity) != 0) {
		return -1;
	}

	uint32_t temp_id = (uint32_t)db->term_count;
	size_t saved_term_count = db->term_count;
	db->terms[temp_id] = term;
	db->term_count = saved_term_count + 1;
	for (uint32_t i = 0; i < temp_id; ++i) {
		uint32_t existing = i;
		int ignore_match_frames =
			term.tag == PROTOTYPE_TERM_LAMBDA ||
			term.tag == PROTOTYPE_TERM_MATCH;
		struct shape_binder_env env;
		memset(&env, 0, sizeof(env));
		if (shape_terms_equal_at_depth(
				db,
				existing,
				temp_id,
				&env,
				PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
				ignore_match_frames,
				0
			)) {
			db->term_count = saved_term_count;
			*p_ret = i;
			return 0;
		}
	}
	db->term_count = saved_term_count;

	uint32_t id = (uint32_t)db->term_count;
	db->terms[id] = term;
	db->term_count++;
	*p_ret = id;
	return 0;
}

uint32_t prototype_term_binder_for_scope_slot(struct prototype_term_db* db, uint32_t scope_slot) {
	if (!db || scope_slot >= PROTOTYPE_SCOPE_BINDER_CAPACITY) {
		return PROTOTYPE_INVALID_ID;
	}
	if (db->scope_binders[scope_slot] != PROTOTYPE_INVALID_ID) {
		return db->scope_binders[scope_slot];
	}
	if (db->next_binder_id >= PROTOTYPE_PI_UNUSED_BINDER_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t binder_id = db->next_binder_id++;
	db->scope_binders[scope_slot] = binder_id;
	return binder_id;
}

uint32_t prototype_term_fresh_binder(struct prototype_term_db* db) {
	if (!db || db->next_binder_id >= PROTOTYPE_PI_UNUSED_BINDER_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_binder_id++;
}

uint32_t prototype_term_new_match_frame(struct prototype_term_db* db) {
	if (!db || reserve_slot(db->match_frame_count, db->match_frame_capacity) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t id = (uint32_t)db->match_frame_count;
	db->match_frames[id].match_term = PROTOTYPE_INVALID_ID;
	memset(&db->match_frames[id].key, 0, sizeof(db->match_frames[id].key));
	db->match_frame_count++;
	return id;
}

int prototype_term_set_match_frame_term(
	struct prototype_term_db* db,
	uint32_t frame_id,
	uint32_t match_term
) {
	if (!db || frame_id >= db->match_frame_count || match_term >= db->term_count) {
		return -1;
	}
	db->match_frames[frame_id].match_term = match_term;
	if (compute_match_frame_key(db, match_term, &db->match_frames[frame_id].key) != 0) {
		memset(&db->match_frames[frame_id].key, 0, sizeof(db->match_frames[frame_id].key));
		return -1;
	}
	invalidate_normalization_cache(db);
	return 0;
}

int prototype_term_match_frame_key(
	const struct prototype_term_db* db,
	uint32_t frame_id,
	struct prototype_match_frame_key* p_key
) {
	if (!db || !p_key || frame_id >= db->match_frame_count) {
		return -1;
	}
	*p_key = db->match_frames[frame_id].key;
	return p_key->is_linkable ? 0 : -1;
}

int prototype_term_var(struct prototype_term_db* db, uint32_t binder_id, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_VAR;
	term.as.var.binder_id = binder_id;
	return add_term(db, term, p_ret);
}

int prototype_term_constructor(
	struct prototype_term_db* db,
	uint32_t owner,
	uint32_t constructor_id,
	uint32_t* p_ret
) {
	if (!db || owner >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_CONSTRUCTOR;
	term.as.constructor.owner = owner;
	term.as.constructor.constructor_id = constructor_id;
	return add_term(db, term, p_ret);
}

int prototype_term_app(struct prototype_term_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_APP;
	term.as.app.function = function;
	term.as.app.argument = argument;
	return add_term(db, term, p_ret);
}

static int prototype_term_lambda_with_tag(
	struct prototype_term_db* db,
	int tag,
	uint32_t binder_id,
	uint32_t body,
	uint32_t* p_ret
) {
	if (!db || body >= db->term_count) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = tag;
	term.as.lambda.binder_id = binder_id;
	term.as.lambda.body = body;
	return add_term(db, term, p_ret);
}

int prototype_term_lambda(
	struct prototype_term_db* db,
	uint32_t binder_id,
	uint32_t body,
	uint32_t* p_ret
) {
	return prototype_term_lambda_with_tag(
		db,
		PROTOTYPE_TERM_LAMBDA,
		binder_id,
		body,
		p_ret
	);
}

int prototype_term_pi_family(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain_family,
	uint32_t* p_ret
) {
	if (!db || !p_ret ||
		domain >= db->term_count ||
		codomain_family >= db->term_count ||
		db->terms[codomain_family].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PI;
	term.as.pi.domain = domain;
	term.as.pi.codomain_family = codomain_family;
	return add_term(db, term, p_ret);
}

int prototype_term_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t* p_ret
) {
	return prototype_term_match_with_frame(
		db,
		scrutinee,
		cases,
		case_count,
		PROTOTYPE_INVALID_ID,
		p_ret
	);
}

int prototype_term_match_with_frame(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t frame_id,
	uint32_t* p_ret
) {
	if (!db || !cases || !p_ret || !db->case_label_symbols || scrutinee >= db->term_count) {
		return -1;
	}
	if (frame_id != PROTOTYPE_INVALID_ID && frame_id >= db->match_frame_count) {
		return -1;
	}
	if (db->case_count + case_count > db->case_capacity) {
		return -1;
	}

	size_t needed_binders = 0;
	for (uint32_t i = 0; i < case_count; ++i) {
		if (cases[i].body >= db->term_count) {
			return -1;
		}
		if (cases[i].constructor_owner != PROTOTYPE_INVALID_ID &&
			cases[i].constructor_owner >= db->term_count) {
			return -1;
		}
		if (cases[i].binder_count > 0 && !cases[i].binders) {
			return -1;
		}
		needed_binders += cases[i].binder_count;
	}
	if (db->case_binder_count + needed_binders > db->case_binder_capacity) {
		return -1;
	}

	size_t saved_case_count = db->case_count;
	size_t saved_case_binder_count = db->case_binder_count;
	size_t saved_term_count = db->term_count;
	uint32_t first_case = (uint32_t)db->case_count;
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t stored_case_id = (uint32_t)db->case_count++;
		struct prototype_match_case* stored_case = &db->cases[stored_case_id];
		db->case_label_symbols[stored_case_id] = cases[i].case_label_symbol_id;
		stored_case->constructor_owner = cases[i].constructor_owner;
		stored_case->constructor_id = cases[i].constructor_id;
		stored_case->first_binder = (uint32_t)db->case_binder_count;
		stored_case->binder_count = cases[i].binder_count;
		stored_case->body = cases[i].body;

		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			struct prototype_case_binder binder = cases[i].binders[j];
			db->case_binders[db->case_binder_count++] = binder;
		}
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_MATCH;
	term.as.match.scrutinee = scrutinee;
	term.as.match.first_case = first_case;
	term.as.match.case_count = case_count;
	term.as.match.frame_id = frame_id;
	if (add_term(db, term, p_ret) != 0) {
		db->case_count = saved_case_count;
		db->case_binder_count = saved_case_binder_count;
		return -1;
	}
	if (*p_ret != saved_term_count) {
		db->case_count = saved_case_count;
		db->case_binder_count = saved_case_binder_count;
	}
	return 0;
}

int prototype_term_resolve_match_case(
	struct prototype_term_db* db,
	uint32_t match_term,
	uint32_t case_index,
	uint32_t constructor_owner,
	uint32_t constructor_id
) {
	if (!db ||
		match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		constructor_owner >= db->term_count ||
		constructor_id == PROTOTYPE_INVALID_ID ||
		case_index >= db->terms[match_term].as.match.case_count) {
		return -1;
	}

	uint32_t stored_case_id = db->terms[match_term].as.match.first_case + case_index;
	if (stored_case_id >= db->case_count) {
		return -1;
	}
	if (db->cases[stored_case_id].constructor_owner != constructor_owner ||
		db->cases[stored_case_id].constructor_id != constructor_id) {
		db->cases[stored_case_id].constructor_owner = constructor_owner;
		db->cases[stored_case_id].constructor_id = constructor_id;
		invalidate_normalization_cache(db);
	}
	return 0;
}

int prototype_term_erase_constructor_view_owners(struct prototype_term_db* db) {
	if (!db) {
		return -1;
	}
	int changed = 0;
	for (size_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* term = &db->terms[i];
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
			continue;
		}
		uint32_t owner = term->as.constructor.owner;
		for (uint32_t depth = 0;
			owner < db->term_count && db->terms[owner].tag == PROTOTYPE_TERM_TYPE_VIEW && depth < 32;
			++depth) {
			owner = db->terms[owner].as.type_view.core;
		}
		if (owner >= db->term_count) {
			return -1;
		}
		if (owner != term->as.constructor.owner) {
			term->as.constructor.owner = owner;
			changed = 1;
		}
	}
	for (size_t i = 0; i < db->case_count; ++i) {
		struct prototype_match_case* match_case = &db->cases[i];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t owner = match_case->constructor_owner;
		for (uint32_t depth = 0;
			owner < db->term_count && db->terms[owner].tag == PROTOTYPE_TERM_TYPE_VIEW && depth < 32;
			++depth) {
			owner = db->terms[owner].as.type_view.core;
		}
		if (owner >= db->term_count) {
			return -1;
		}
		if (owner != match_case->constructor_owner) {
			match_case->constructor_owner = owner;
			changed = 1;
		}
	}
	if (changed) {
		invalidate_normalization_cache(db);
	}
	return 0;
}

static int prototype_term_type_former(
	struct prototype_term_db* db,
	uint32_t representation_id,
	uint32_t* p_ret
) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_FORMER;
	term.as.type_former.representation_id = representation_id;
	return add_term(db, term, p_ret);
}

static int prototype_term_type_declaration(
	struct prototype_term_db* db,
	uint32_t type_id,
	uint32_t* p_ret
) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	term.as.type_declaration.type_id = type_id;
	return add_term(db, term, p_ret);
}

static int prototype_term_type_view(
	struct prototype_term_db* db,
	uint32_t view_type_id,
	uint32_t core,
	uint32_t source,
	uint32_t* p_ret
) {
	if (!db || !p_ret || core >= db->term_count || source >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_VIEW;
	term.as.type_view.view_type_id = view_type_id;
	term.as.type_view.core = core;
	term.as.type_view.source = source;
	return add_term(db, term, p_ret);
}

int prototype_term_type_instance_make(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || arg_count > 16 ||
		type_id >= type_declarations->type_count) {
		return -1;
	}
	if (arg_count > 0 && !args) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (args[i] >= db->term_count) {
			return -1;
		}
	}

	uint32_t representation_id;
	if (prototype_type_declaration_intern_representation(
				db,
				type_declarations,
				type_id,
				&representation_id
		) != 0) {
		return -1;
	}
	uint32_t current;
	if (prototype_term_type_former(db, representation_id, &current) != 0) {
		return -1;
	}
	uint32_t source_current;
	if (prototype_term_type_declaration(db, type_id, &source_current) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t argument = args[i];
		if (db->terms[argument].tag == PROTOTYPE_TERM_TYPE_VIEW) {
			argument = db->terms[argument].as.type_view.core;
		}
		uint32_t next;
		if (prototype_term_app(db, current, argument, &next) != 0) {
			return -1;
		}
		current = next;
		if (prototype_term_app(db, source_current, args[i], &next) != 0) {
			return -1;
		}
		source_current = next;
	}
	return prototype_term_type_view(db, type_id, current, source_current, p_ret);
}

static int type_instance_app_spine_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_type_id,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!db || !p_type_id || !p_arg_count || term_id >= db->term_count) {
		return -1;
	}
	if (db->terms[term_id].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		const struct prototype_term* view = &db->terms[term_id];
		if (view->as.type_view.source >= db->term_count ||
			type_instance_app_spine_info(
				db,
				view->as.type_view.source,
				p_type_id,
				args,
				p_arg_count
			) != 0) {
			return -1;
		}
		*p_type_id = view->as.type_view.view_type_id;
		return 0;
	}

	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = term_id;
	while (current < db->term_count && db->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16) {
			return -1;
		}
		reversed[count++] = db->terms[current].as.app.argument;
		current = db->terms[current].as.app.function;
	}
	if (current >= db->term_count ||
		db->terms[current].tag != PROTOTYPE_TERM_TYPE_DECLARATION) {
		return -1;
	}
	if (count > 0 && !args) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		args[i] = reversed[count - i - 1];
	}
	*p_type_id = db->terms[current].as.type_declaration.type_id;
	*p_arg_count = count;
	return 0;
}

int prototype_term_type_instance_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_type_id,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!db || !p_type_id || !p_arg_count || term_id >= db->term_count) {
		return -1;
	}
	return type_instance_app_spine_info(db, term_id, p_type_id, args, p_arg_count);
}

int prototype_term_rebind_type_former_anchors(
	struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations
) {
	if (!db || !type_declarations || type_declarations->representations_dirty) {
		return -1;
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* term = &db->terms[i];
		if (term->tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t declaration_anchor = term->as.type_former.representation_id;
		if (declaration_anchor >= type_declarations->type_count ||
			type_declarations->type_declarations[declaration_anchor].representation_id ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		term->as.type_former.representation_id =
			type_declarations->type_declarations[declaration_anchor].representation_id;
	}
	return 0;
}

int prototype_term_type_instance_extend(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || argument >= db->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(db, instance, &type_id, args, &arg_count) != 0 ||
		arg_count >= 16) {
		return -1;
	}
	args[arg_count] = argument;
	return prototype_term_type_instance_make(
		db,
		type_declarations,
		type_id,
		args,
		arg_count + 1,
		p_ret
	);
}

static int prototype_term_type_view_rebuild_from_source(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t view_type_id,
	uint32_t core,
	uint32_t source,
	uint32_t* p_ret
) {
	if (!db || !p_ret || core >= db->term_count || source >= db->term_count) {
		return -1;
	}
	if (type_declarations) {
		uint32_t source_type_id;
		uint32_t args[16];
		uint32_t arg_count;
		if (prototype_term_type_instance_info(
				db,
				source,
				&source_type_id,
				args,
				&arg_count
			) == 0 &&
			source_type_id == view_type_id) {
			return prototype_term_type_instance_make(
				db,
				type_declarations,
				view_type_id,
				args,
				arg_count,
				p_ret
			);
		}
	}
	return prototype_term_type_view(db, view_type_id, core, source, p_ret);
}

int prototype_term_type_instance_is_saturated(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!db || !type_declarations) {
		return 0;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(db, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count) {
		return 0;
	}
	return arg_count == type_declarations->type_declarations[type_id].parameter_count;
}

int prototype_term_induction_hypothesis(
	struct prototype_term_db* db,
	uint32_t frame_id,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!db || argument >= db->term_count || frame_id >= db->match_frame_count) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_INDUCTION_HYPOTHESIS;
	term.as.induction_hypothesis.frame_id = frame_id;
	term.as.induction_hypothesis.argument = argument;
	return add_term(db, term, p_ret);
}

int prototype_term_universe_var(struct prototype_term_db* db, uint32_t level_var, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_UNIVERSE_VAR;
	term.as.universe_var.level_var = level_var;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_text(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_TEXT;
	return add_term(db, term, p_ret);
}

int prototype_term_text_literal(struct prototype_term_db* db, int text_symbol_id, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TEXT_LITERAL;
	term.as.text_literal.text_symbol_id = text_symbol_id;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_int(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_INT;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_int64(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_INT64;
	return add_term(db, term, p_ret);
}

int prototype_term_int_literal(struct prototype_term_db* db, int64_t value, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_INT_LITERAL;
	term.as.int_literal.value = value;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_label(struct prototype_term_db* db, unsigned effects, uint32_t* p_ret) {
	if (!db || !p_ret || effects == PROTOTYPE_HOST_EFFECT_NONE) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_LABEL;
	term.as.effect_label.effects = effects;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_type(
	struct prototype_term_db* db,
	uint32_t label,
	uint32_t result,
	uint32_t* p_ret
) {
	if (!db || !p_ret || label >= db->term_count || result >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_TYPE;
	term.as.effect_type.label = label;
	term.as.effect_type.result = result;
	return add_term(db, term, p_ret);
}

int prototype_term_make_host_type(
	struct prototype_term_db* db,
	int type_id,
	uint32_t* p_ret
) {
	switch (type_id) {
		case PROTOTYPE_HOST_TYPE_TEXT:
			return prototype_term_primitive_text(db, p_ret);
		case PROTOTYPE_HOST_TYPE_INT32:
			return prototype_term_primitive_int(db, p_ret);
		case PROTOTYPE_HOST_TYPE_INT64:
			return prototype_term_primitive_int64(db, p_ret);
		default:
			return -1;
	}
}

int prototype_term_external_ref(
	struct prototype_term_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
) {
	if (name.name_symbol_id < 0) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EXTERNAL_REF;
	term.as.external_ref.name = name;
	return add_term(db, term, p_ret);
}

int prototype_term_intrinsic(
	struct prototype_term_db* db,
	int intrinsic_id,
	int symbol_id,
	int type_symbol_id,
	uint32_t* p_ret
) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_INTRINSIC;
	term.as.intrinsic.intrinsic_id = intrinsic_id;
	term.as.intrinsic.symbol_id = symbol_id;
	term.as.intrinsic.type_symbol_id = type_symbol_id;
	return add_term(db, term, p_ret);
}

static int term_contains_free_binder_at_depth(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t depth
) {
	if (!db || term_id >= db->term_count || depth > 128) {
		return 0;
	}
	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			return term->as.var.binder_id == binder_id;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return term_contains_free_binder_at_depth(
				db,
				term->as.constructor.owner,
				binder_id,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			return term_contains_free_binder_at_depth(
					db,
					term->as.app.function,
					binder_id,
					depth + 1
				) ||
				term_contains_free_binder_at_depth(
					db,
					term->as.app.argument,
					binder_id,
					depth + 1
				);
		case PROTOTYPE_TERM_LAMBDA:
			if (term->as.lambda.binder_id == binder_id) {
				return 0;
			}
			return term_contains_free_binder_at_depth(
				db,
				term->as.lambda.body,
				binder_id,
				depth + 1
			);
		case PROTOTYPE_TERM_PI: {
			if (term_contains_free_binder_at_depth(
					db,
					term->as.pi.domain,
					binder_id,
					depth + 1
				)) {
				return 1;
			}
			const struct prototype_term* family = &db->terms[term->as.pi.codomain_family];
			if (family->tag == PROTOTYPE_TERM_LAMBDA &&
				family->as.lambda.binder_id == binder_id) {
				return 0;
			}
			return term_contains_free_binder_at_depth(
				db,
				term->as.pi.codomain_family,
				binder_id,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			if (term_contains_free_binder_at_depth(
				db,
				term->as.match.scrutinee,
				binder_id,
				depth + 1
			)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				int shadowed = 0;
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					const struct prototype_case_binder* case_binder =
						&db->case_binders[match_case->first_binder + j];
					if (case_binder->binder_id == binder_id) {
						shadowed = 1;
					}
				}
				if (!shadowed &&
					term_contains_free_binder_at_depth(db, match_case->body, binder_id, depth + 1)) {
					return 1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW:
			return term_contains_free_binder_at_depth(
					db,
					term->as.type_view.core,
					binder_id,
					depth + 1
				) ||
				term_contains_free_binder_at_depth(
					db,
					term->as.type_view.source,
					binder_id,
					depth + 1
				);
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return term_contains_free_binder_at_depth(
					db,
					term->as.induction_hypothesis.argument,
					binder_id,
					depth + 1
				);
			case PROTOTYPE_TERM_EFFECT_TYPE:
				return term_contains_free_binder_at_depth(
						db,
						term->as.effect_type.label,
						binder_id,
						depth + 1
					) ||
					term_contains_free_binder_at_depth(
						db,
						term->as.effect_type.result,
						binder_id,
						depth + 1
					);
			default:
				return 0;
		}
	}

int prototype_term_contains_free_binder(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binder_id
) {
	return term_contains_free_binder_at_depth(db, term_id, binder_id, 0);
}

int prototype_term_pi(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
) {
	if (!db || domain >= db->term_count || codomain >= db->term_count) {
		return -1;
	}

	uint32_t binder_id;
	uint32_t codomain_family;
	binder_id = PROTOTYPE_PI_UNUSED_BINDER_ID;
	if (prototype_term_lambda(
		db,
		binder_id,
		codomain,
		&codomain_family
	) != 0) {
		return -1;
	}
	return prototype_term_pi_family(db, domain, codomain_family, p_ret);
}

static int substitute_term(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	struct substitution_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.type_declarations = type_declarations;
	return substitute_term_internal(
		db,
		term_id,
		binder_id,
		replacement,
		&ctx,
		p_ret
	);
}

static int substitution_lookup_frame(
	const struct substitution_context* ctx,
	uint32_t old_frame_id,
	uint32_t* p_new_frame_id
) {
	if (!ctx || !p_new_frame_id) {
		return 0;
	}
	for (const struct substitution_frame_scope* scope = ctx->frame_scope;
		scope;
		scope = scope->previous) {
		if (scope->old_frame_id == old_frame_id) {
			*p_new_frame_id = scope->new_frame_id;
			return 1;
		}
	}
	return 0;
}

static int substitution_has_frame_scope(
	const struct substitution_context* ctx,
	uint32_t frame_id
) {
	uint32_t ignored;
	return substitution_lookup_frame(ctx, frame_id, &ignored);
}

static int term_contains_frame_scope_reference_at_depth(
	const struct prototype_term_db* db,
	const struct substitution_context* ctx,
	uint32_t term_id,
	unsigned depth
) {
	if (!db || !ctx || term_id >= db->term_count || depth > 100000) {
		return 0;
	}
	if (!ctx->frame_scope) {
		return 0;
	}
	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_APP:
			return term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.app.function,
					depth + 1
				) ||
				term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.app.argument,
					depth + 1
				);
		case PROTOTYPE_TERM_LAMBDA:
			return term_contains_frame_scope_reference_at_depth(
				db,
				ctx,
				term->as.lambda.body,
				depth + 1
			);
		case PROTOTYPE_TERM_PI:
			return term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.pi.domain,
					depth + 1
				) ||
				term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.pi.codomain_family,
					depth + 1
				);
		case PROTOTYPE_TERM_MATCH:
			if (term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.match.scrutinee,
					depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= db->case_count) {
					return 0;
				}
				const struct prototype_match_case* match_case = &db->cases[case_id];
				if ((match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
						term_contains_frame_scope_reference_at_depth(
							db,
							ctx,
							match_case->constructor_owner,
							depth + 1
						)) ||
					term_contains_frame_scope_reference_at_depth(
						db,
						ctx,
						match_case->body,
						depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return term_contains_frame_scope_reference_at_depth(
				db,
				ctx,
				term->as.constructor.owner,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.type_view.core,
					depth + 1
				) ||
				term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.type_view.source,
					depth + 1
				);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return substitution_has_frame_scope(
					ctx,
					term->as.induction_hypothesis.frame_id
				) ||
				term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.induction_hypothesis.argument,
					depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_TYPE:
			return term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.effect_type.label,
					depth + 1
				) ||
				term_contains_frame_scope_reference_at_depth(
					db,
					ctx,
					term->as.effect_type.result,
					depth + 1
				);
		default:
			return 0;
	}
}

static int term_contains_frame_scope_reference(
	const struct prototype_term_db* db,
	const struct substitution_context* ctx,
	uint32_t term_id
) {
	return term_contains_frame_scope_reference_at_depth(db, ctx, term_id, 0);
}

static int substitute_term_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	struct substitution_context* ctx,
	uint32_t* p_ret
) {
	if (!db || !p_ret || !ctx || term_id >= db->term_count ||
		(binder_id != PROTOTYPE_INVALID_ID && replacement >= db->term_count)) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			if (binder_id != PROTOTYPE_INVALID_ID &&
				term->as.var.binder_id == binder_id) {
				*p_ret = replacement;
			} else {
				*p_ret = term_id;
			}
			return 0;
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (substitute_term_internal(
					db,
					term->as.app.function,
					binder_id,
					replacement,
					ctx,
					&function
				) != 0) {
				return -1;
			}
			if (substitute_term_internal(
					db,
					term->as.app.argument,
					binder_id,
					replacement,
					ctx,
					&argument
				) != 0) {
				return -1;
			}
			if (function == term->as.app.function && argument == term->as.app.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_app(db, function, argument, p_ret);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (substitute_term_internal(
					db,
					term->as.pi.domain,
					binder_id,
					replacement,
					ctx,
					&domain
				) != 0 ||
				substitute_term_internal(
					db,
					term->as.pi.codomain_family,
					binder_id,
					replacement,
					ctx,
					&codomain_family
				) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain && codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(db, domain, codomain_family, p_ret);
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			uint32_t argument;
			if (substitute_term_internal(
				db,
				term->as.induction_hypothesis.argument,
				binder_id,
				replacement,
				ctx,
				&argument
			) != 0) {
				return -1;
			}
			uint32_t frame_id = term->as.induction_hypothesis.frame_id;
			substitution_lookup_frame(ctx, frame_id, &frame_id);
			if (argument == term->as.induction_hypothesis.argument &&
				frame_id == term->as.induction_hypothesis.frame_id) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				db,
				frame_id,
				argument,
				p_ret
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t scrutinee;
			if (substitute_term_internal(
					db,
					term->as.match.scrutinee,
					binder_id,
					replacement,
					ctx,
					&scrutinee
				) != 0) {
				return -1;
			}

			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			int changed = scrutinee != term->as.match.scrutinee;
			if (term->as.match.case_count > 64) {
				return -1;
			}

			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				if (old_case->binder_count > 0 && binder_cursor + old_case->binder_count > 256) {
					return -1;
				}

				int shadows = 0;
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					struct prototype_case_binder case_binder = db->case_binders[old_case->first_binder + j];
					binder_storage[binder_cursor + j] = case_binder;
					if (case_binder.binder_id == binder_id) {
						shadows = 1;
					}
				}
				uint32_t body_binder_id = shadows ? PROTOTYPE_INVALID_ID : binder_id;
				if ((body_binder_id != PROTOTYPE_INVALID_ID &&
						prototype_term_contains_free_binder(
							db,
							old_case->body,
							body_binder_id
						)) ||
					term_contains_frame_scope_reference(db, ctx, old_case->body)) {
					changed = 1;
				}
				if (old_case->constructor_owner != PROTOTYPE_INVALID_ID &&
					((binder_id != PROTOTYPE_INVALID_ID &&
							prototype_term_contains_free_binder(
								db,
								old_case->constructor_owner,
								binder_id
							)) ||
						term_contains_frame_scope_reference(
							db,
							ctx,
							old_case->constructor_owner
						))) {
					changed = 1;
				}
				binder_cursor += old_case->binder_count;
			}

			if (!changed) {
				*p_ret = term_id;
				return 0;
			}

			uint32_t frame_id = term->as.match.frame_id;
			const struct substitution_frame_scope* saved_frame_scope = ctx->frame_scope;
			struct substitution_frame_scope frame_scope;
			if (frame_id != PROTOTYPE_INVALID_ID) {
				uint32_t new_frame_id = prototype_term_new_match_frame(db);
				if (new_frame_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				frame_scope.old_frame_id = frame_id;
				frame_scope.new_frame_id = new_frame_id;
				frame_scope.previous = saved_frame_scope;
				ctx->frame_scope = &frame_scope;
				frame_id = new_frame_id;
			}

			binder_cursor = 0;
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				uint32_t body = old_case->body;
				uint32_t constructor_owner = old_case->constructor_owner;
				int shadows = 0;
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					struct prototype_case_binder case_binder =
						db->case_binders[old_case->first_binder + j];
					binder_storage[binder_cursor + j] = case_binder;
					if (case_binder.binder_id == binder_id) {
						shadows = 1;
					}
				}
				uint32_t body_binder_id = shadows ? PROTOTYPE_INVALID_ID : binder_id;
				if (substitute_term_internal(
						db,
						old_case->body,
						body_binder_id,
						replacement,
						ctx,
						&body
					) != 0 ||
					(constructor_owner != PROTOTYPE_INVALID_ID &&
						substitute_term_internal(
							db,
							constructor_owner,
							binder_id,
							replacement,
							ctx,
							&constructor_owner
						) != 0)) {
					ctx->frame_scope = saved_frame_scope;
					return -1;
				}
				case_inputs[i].case_label_symbol_id =
					db->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = body;
				binder_cursor += old_case->binder_count;
			}

			uint32_t match_term;
			int status = prototype_term_match_with_frame(
				db,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				frame_id,
				&match_term
			);
			if (status == 0 && frame_id != PROTOTYPE_INVALID_ID) {
				status = prototype_term_set_match_frame_term(db, frame_id, match_term);
			}
			ctx->frame_scope = saved_frame_scope;
			if (status != 0) {
				return -1;
			}
			*p_ret = match_term;
			return 0;
		}
		case PROTOTYPE_TERM_LAMBDA: {
			int tag = term->tag;
			if (term->as.lambda.binder_id == binder_id) {
				if (!ctx->frame_scope) {
					*p_ret = term_id;
					return 0;
				}
				binder_id = PROTOTYPE_INVALID_ID;
			}
			uint32_t body;
			if (substitute_term_internal(
					db,
					term->as.lambda.body,
					binder_id,
					replacement,
					ctx,
					&body
				) != 0) {
				return -1;
			}
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda_with_tag(
				db,
				tag,
				term->as.lambda.binder_id,
				body,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (substitute_term_internal(
					db,
					term->as.constructor.owner,
					binder_id,
					replacement,
					ctx,
					&owner
				) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(
				db,
				owner,
				term->as.constructor.constructor_id,
				p_ret
			);
			}
			case PROTOTYPE_TERM_TYPE_FORMER:
			case PROTOTYPE_TERM_TYPE_DECLARATION:
				case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				case PROTOTYPE_TERM_INT_LITERAL:
				case PROTOTYPE_TERM_INTRINSIC:
				case PROTOTYPE_TERM_EFFECT_LABEL:
					*p_ret = term_id;
					return 0;
			case PROTOTYPE_TERM_EFFECT_TYPE: {
				uint32_t label;
				uint32_t result;
				if (substitute_term_internal(
						db,
						term->as.effect_type.label,
						binder_id,
						replacement,
						ctx,
						&label
					) != 0 ||
					substitute_term_internal(
						db,
						term->as.effect_type.result,
						binder_id,
						replacement,
						ctx,
						&result
					) != 0) {
					return -1;
				}
				if (label == term->as.effect_type.label &&
					result == term->as.effect_type.result) {
					*p_ret = term_id;
					return 0;
				}
				return prototype_term_effect_type(db, label, result, p_ret);
			}
			case PROTOTYPE_TERM_TYPE_VIEW: {
			uint32_t core;
			uint32_t source;
			if (substitute_term_internal(
					db,
					term->as.type_view.core,
					binder_id,
					replacement,
					ctx,
					&core
				) != 0 ||
				substitute_term_internal(
					db,
					term->as.type_view.source,
					binder_id,
					replacement,
					ctx,
					&source
				) != 0) {
				return -1;
			}
			if (core == term->as.type_view.core &&
				source == term->as.type_view.source) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_type_view_rebuild_from_source(
				db,
				ctx->type_declarations,
				term->as.type_view.view_type_id,
				core,
				source,
				p_ret
			);
		}
			default:
				*p_ret = term_id;
				return 0;
	}
}

int prototype_term_substitute(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	return substitute_term(db, type_declarations, term_id, binder_id, replacement, p_ret);
}

static int resolve_external_ref_term(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name symbol_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	if (!db || !p_ret || term_id >= db->term_count || replacement >= db->term_count) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			*p_ret = qualified_names_equal(term->as.external_ref.name, symbol_id) ? replacement : term_id;
			return 0;
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (resolve_external_ref_term(db, term->as.app.function, symbol_id, replacement, &function) != 0 ||
				resolve_external_ref_term(db, term->as.app.argument, symbol_id, replacement, &argument) != 0) {
				return -1;
			}
			if (function == term->as.app.function && argument == term->as.app.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_app(db, function, argument, p_ret);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (resolve_external_ref_term(db, term->as.pi.domain, symbol_id, replacement, &domain) != 0 ||
				resolve_external_ref_term(db, term->as.pi.codomain_family, symbol_id, replacement, &codomain_family) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain && codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(db, domain, codomain_family, p_ret);
		}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			uint32_t core;
			uint32_t source;
			if (resolve_external_ref_term(
					db,
					term->as.type_view.core,
					symbol_id,
					replacement,
					&core
				) != 0 ||
				resolve_external_ref_term(
					db,
					term->as.type_view.source,
					symbol_id,
					replacement,
					&source
				) != 0) {
				return -1;
			}
			if (core == term->as.type_view.core &&
				source == term->as.type_view.source) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_type_view(
				db,
				term->as.type_view.view_type_id,
				core,
				source,
				p_ret
			);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (resolve_external_ref_term(db, term->as.lambda.body, symbol_id, replacement, &body) != 0) {
				return -1;
			}
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda_with_tag(db, term->tag, term->as.lambda.binder_id, body, p_ret);
		}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t scrutinee;
			if (resolve_external_ref_term(db, term->as.match.scrutinee, symbol_id, replacement, &scrutinee) != 0) {
				return -1;
			}

			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			int changed = scrutinee != term->as.match.scrutinee;
			if (term->as.match.case_count > 64) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				uint32_t body;
				if (binder_cursor + old_case->binder_count > 256 ||
					resolve_external_ref_term(db, old_case->body, symbol_id, replacement, &body) != 0) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binder_storage[binder_cursor + j] =
						db->case_binders[old_case->first_binder + j];
				}
				if (body != old_case->body) {
					changed = 1;
				}
				case_inputs[i].case_label_symbol_id =
					db->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = old_case->constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = body;
				binder_cursor += old_case->binder_count;
			}
			if (!changed) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_match_with_frame(
				db,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				term->as.match.frame_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (resolve_external_ref_term(db, term->as.constructor.owner, symbol_id, replacement, &owner) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(db, owner, term->as.constructor.constructor_id, p_ret);
		}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
				uint32_t argument;
				if (resolve_external_ref_term(
					db,
					term->as.induction_hypothesis.argument,
					symbol_id,
					replacement,
					&argument
				) != 0) {
				return -1;
			}
			if (argument == term->as.induction_hypothesis.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				db,
				term->as.induction_hypothesis.frame_id,
				argument,
				p_ret
				);
			}
			case PROTOTYPE_TERM_EFFECT_TYPE: {
				uint32_t label;
				uint32_t result;
				if (resolve_external_ref_term(
						db,
						term->as.effect_type.label,
						symbol_id,
						replacement,
						&label
					) != 0 ||
					resolve_external_ref_term(
						db,
						term->as.effect_type.result,
						symbol_id,
						replacement,
						&result
					) != 0) {
					return -1;
				}
				if (label == term->as.effect_type.label &&
					result == term->as.effect_type.result) {
					*p_ret = term_id;
					return 0;
				}
				return prototype_term_effect_type(db, label, result, p_ret);
			}
			default:
				*p_ret = term_id;
				return 0;
		}
}

int prototype_term_resolve_external_ref(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name name,
	uint32_t replacement,
	uint32_t* p_ret
) {
	return resolve_external_ref_term(db, term_id, name, replacement, p_ret);
}

static void decompose_app(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	uint32_t reversed_args[64];
	uint32_t reversed_count = 0;
	uint32_t current = term_id;

	while (current < db->term_count &&
		db->terms[current].tag == PROTOTYPE_TERM_APP &&
		reversed_count < 64) {
		reversed_args[reversed_count++] = db->terms[current].as.app.argument;
		current = db->terms[current].as.app.function;
	}

	*p_head = current;
	*p_arg_count = reversed_count;
	for (uint32_t i = 0; i < reversed_count; ++i) {
		args[i] = reversed_args[reversed_count - i - 1];
	}
}

static int type_instance_type_id(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t* p_type_id
) {
	if (!terms || !p_type_id || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t arg_count;
	uint32_t args[16];
	if (prototype_term_type_instance_info(terms, term_id, p_type_id, args, &arg_count) == 0) {
		return 0;
	}
	return -1;
}

static int constructor_owner_representation_id(
	const struct prototype_term_db* terms,
	uint32_t owner,
	uint32_t* p_representation_id
) {
	if (!terms || !p_representation_id || owner >= terms->term_count) {
		return -1;
	}
	uint32_t current = owner;
	while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count || terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return -1;
	}
	*p_representation_id = terms->terms[current].as.type_former.representation_id;
	return 0;
}

static int constructor_member_matches(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t term_id,
	const struct prototype_match_case* match_case
) {
	if (!terms || !match_case || term_id >= terms->term_count) {
		return 0;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		return 0;
	}
	if (term->as.constructor.constructor_id != match_case->constructor_id) {
		return 0;
	}
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t term_representation_id;
	uint32_t case_representation_id;
	(void)type_declarations;
	(void)definitions;
	/*
	 * Constructor matching belongs to the erased computation graph. A source
	 * TYPE_VIEW may remain inside a substituted type application, so structural
	 * comparison of owner terms would make List Nat fail to match itself after
	 * beta substitution. The representation and ordinal are the runtime tag.
	 */
	return constructor_owner_representation_id(
		terms,
		term->as.constructor.owner,
		&term_representation_id
	) == 0 && constructor_owner_representation_id(
		terms,
		match_case->constructor_owner,
		&case_representation_id
	) == 0 && term_representation_id == case_representation_id;
}

static int term_is_constructor_like(const struct prototype_term* term) {
	if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		return -1;
	}
	return 0;
}

static int lookup_transparent_definition(
	const struct prototype_term_definition_env* definitions,
	struct prototype_qualified_name name,
	uint32_t* p_term
) {
	if (!definitions || !p_term || name.name_symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < definitions->definition_count; ++i) {
		const struct prototype_term_definition* definition = &definitions->definitions[i];
		if (qualified_names_equal(definition->name, name) &&
			definition->transparency == PROTOTYPE_TERM_DEFINITION_TRANSPARENT) {
			*p_term = definition->term;
			return 1;
		}
	}
	return 0;
}

static int evaluate_steps(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int perform_host_step(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int normalization_equal_at_depth(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	int* p_equal,
	uint32_t depth
);

static struct prototype_term_reduction_options normalize_reduction_options(
	struct prototype_term_reduction_options options
) {
	if ((options.flags & PROTOTYPE_TERM_PERFORM_HOST_EFFECT) != 0) {
		/*
		 * Uniform match is a pure structural normalization rule. It may inspect
		 * every branch body before the scrutinee is known, so it must not run in
		 * an effect-performing strategy where branch order is observable.
		 */
		options.flags &= ~PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH;
	}
	return options;
}

static int options_allow_uniform_match(
	struct prototype_term_reduction_options options
) {
	return (options.flags & PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH) != 0;
}

static int normalization_profile_options(
	int profile,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options* p_options
) {
	if (!p_options) {
		return -1;
	}
	memset(p_options, 0, sizeof(*p_options));
	switch (profile) {
		case PROTOTYPE_TERM_NORMALIZATION_LAMBDA_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA |
				PROTOTYPE_TERM_REDUCE_MATCH |
				PROTOTYPE_TERM_REDUCE_INDUCTION;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_KERNEL_CONVERSION_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA |
				PROTOTYPE_TERM_REDUCE_MATCH |
				PROTOTYPE_TERM_REDUCE_INDUCTION |
				PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH;
			break;
		default:
			return -1;
	}
	if (definitions) {
		p_options->flags |= PROTOTYPE_TERM_REDUCE_DEFINITIONS;
	}
	return 0;
}

static int normalization_profile_from_options(
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options
) {
	if (definitions ||
		(options.flags & (PROTOTYPE_TERM_REDUCE_DEFINITIONS |
			PROTOTYPE_TERM_REDUCE_HOST_PURE |
			PROTOTYPE_TERM_PERFORM_HOST_EFFECT)) != 0) {
		return 0;
	}
	if (options.flags == PROTOTYPE_TERM_REDUCE_BETA) {
		return PROTOTYPE_TERM_NORMALIZATION_LAMBDA_WHNF;
	}
	if (options.flags == (PROTOTYPE_TERM_REDUCE_BETA |
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION)) {
		return PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF;
	}
	if (options.flags == (PROTOTYPE_TERM_REDUCE_BETA |
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION |
			PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH)) {
		return PROTOTYPE_TERM_NORMALIZATION_KERNEL_CONVERSION_WHNF;
	}
	return 0;
}

static int normalization_cache_lookup(
	struct prototype_term_db* db,
	int profile,
	uint32_t term_id,
	uint32_t* p_result
) {
	if (!db || !p_result) {
		return 0;
	}
	for (size_t i = 0; i < PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY; ++i) {
		const struct prototype_term_normalization_cache_entry* entry =
			&db->normalization_cache[i];
		if (entry->state == PROTOTYPE_TERM_NORMALIZATION_CACHE_COMPLETE &&
			entry->graph_revision == db->normalization_graph_revision &&
			entry->profile == profile &&
			entry->term_id == term_id &&
			entry->result_term_id < db->term_count) {
			*p_result = entry->result_term_id;
			return 1;
		}
	}
	return 0;
}

static struct prototype_term_normalization_cache_entry* normalization_cache_reserve(
	struct prototype_term_db* db,
	int profile,
	uint32_t term_id
) {
	if (!db) {
		return NULL;
	}
	for (size_t i = 0; i < PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY; ++i) {
		struct prototype_term_normalization_cache_entry* entry =
			&db->normalization_cache[i];
		if (entry->state == PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY ||
			entry->graph_revision != db->normalization_graph_revision) {
			entry->term_id = term_id;
			entry->result_term_id = PROTOTYPE_INVALID_ID;
			entry->graph_revision = db->normalization_graph_revision;
			entry->profile = profile;
			entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS;
			return entry;
		}
	}
	struct prototype_term_normalization_cache_entry* entry =
		&db->normalization_cache[
			db->normalization_cache_next++ % PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY
		];
	entry->term_id = term_id;
	entry->result_term_id = PROTOTYPE_INVALID_ID;
	entry->graph_revision = db->normalization_graph_revision;
	entry->profile = profile;
	entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS;
	return entry;
}

static int uniform_match_body(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t match_term,
	uint32_t* p_body,
	unsigned depth
) {
	options.flags &= ~PROTOTYPE_TERM_PERFORM_HOST_EFFECT;
	options.effect_output = NULL;
	options.effect_capabilities = 0;
	options.p_effect_performed = NULL;

	if (!db || !type_declarations || !p_body ||
		match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		db->terms[match_term].as.match.case_count == 0) {
		return 0;
	}
	const struct prototype_term* term = &db->terms[match_term];
	uint32_t first_case_id = term->as.match.first_case;
	if (first_case_id >= db->case_count) {
		return -1;
	}
	const struct prototype_match_case* first_case = &db->cases[first_case_id];
	uint32_t first_body;
	if (evaluate_steps(
			db,
			type_declarations,
			definitions,
			options,
			first_case->body,
			&first_body,
			depth
		) != 0) {
		return -1;
	}
	for (uint32_t j = 0; j < first_case->binder_count; ++j) {
		const struct prototype_case_binder* binder =
			&db->case_binders[first_case->first_binder + j];
		if (prototype_term_contains_free_binder(db, first_body, binder->binder_id)) {
			return 0;
		}
	}
	for (uint32_t i = 1; i < term->as.match.case_count; ++i) {
		uint32_t case_id = term->as.match.first_case + i;
		if (case_id >= db->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &db->cases[case_id];
		uint32_t normalized_body;
		if (evaluate_steps(
				db,
				type_declarations,
				definitions,
				options,
				match_case->body,
				&normalized_body,
				depth
			) != 0) {
			return -1;
		}
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
			const struct prototype_case_binder* binder =
				&db->case_binders[match_case->first_binder + j];
			if (prototype_term_contains_free_binder(db, normalized_body, binder->binder_id)) {
				return 0;
			}
		}
		int same_body = 0;
		if (normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				first_body,
				normalized_body,
				&same_body,
				0
			) != 0) {
			return -1;
		}
		if (!same_body) {
			return 0;
		}
	}
	*p_body = first_body;
	return 1;
}

static int evaluate_steps(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	if (depth == 0) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF: {
			uint32_t definition_term;
			if ((options.flags & PROTOTYPE_TERM_REDUCE_DEFINITIONS) &&
				lookup_transparent_definition(
					definitions,
					term->as.external_ref.name,
					&definition_term
				)) {
				if (definition_term >= db->term_count) {
					return -1;
				}
					return evaluate_steps(
						db,
						type_declarations,
						definitions,
						options,
						definition_term,
						p_ret,
						depth - 1
					);
			}
			*p_ret = term_id;
			return 0;
		}
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			if (evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					term->as.app.function,
					&function,
					depth - 1
				) != 0) {
				return -1;
			}

			const struct prototype_term* reduced_function = &db->terms[function];
			if ((options.flags & PROTOTYPE_TERM_REDUCE_BETA) &&
				reduced_function->tag == PROTOTYPE_TERM_LAMBDA) {
				uint32_t substituted;
				if (substitute_term(
					db,
					type_declarations,
					reduced_function->as.lambda.body,
					reduced_function->as.lambda.binder_id,
					term->as.app.argument,
					&substituted
				) != 0) {
					return -1;
				}
				return evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					substituted,
					p_ret,
					depth - 1
				);
			}

			uint32_t candidate = term_id;
			if (function != term->as.app.function &&
				prototype_term_app(db, function, term->as.app.argument, &candidate) != 0) {
				return -1;
			}
			uint32_t stepped;
			int step_status = perform_host_step(
				db,
				type_declarations,
				definitions,
				options,
				candidate,
				&stepped,
				depth - 1
			);
			if (step_status < 0) {
				return -1;
			}
			if (step_status > 0) {
				return evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					stepped,
					p_ret,
					depth - 1
				);
			}
			*p_ret = candidate;
			return 0;
		}
			case PROTOTYPE_TERM_MATCH: {
				if (!(options.flags & PROTOTYPE_TERM_REDUCE_MATCH)) {
					*p_ret = term_id;
					return 0;
				}
				if (options_allow_uniform_match(options)) {
					uint32_t common_body;
					int uniform_status = uniform_match_body(
						db,
						type_declarations,
						definitions,
						options,
						term_id,
						&common_body,
						depth - 1
					);
					if (uniform_status < 0) {
						return -1;
					}
					if (uniform_status > 0) {
						return evaluate_steps(
							db,
							type_declarations,
							definitions,
							options,
							common_body,
							p_ret,
							depth - 1
						);
					}
				}
				uint32_t scrutinee;
				if (evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					term->as.match.scrutinee,
					&scrutinee,
					depth - 1
				) != 0) {
				return -1;
			}

			uint32_t head;
			uint32_t args[64];
			uint32_t arg_count;
			decompose_app(db, scrutinee, &head, args, &arg_count);
			if (head >= db->term_count) {
				return -1;
			}

			if (term_is_constructor_like(&db->terms[head]) != 0) {
				if (scrutinee == term->as.match.scrutinee) {
					*p_ret = term_id;
					return 0;
				}
				struct prototype_match_case_input case_inputs[64];
				struct prototype_case_binder binder_storage[256];
				uint32_t binder_cursor = 0;
				if (term->as.match.case_count > 64) {
					return -1;
				}
				for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
					const struct prototype_match_case* old_case =
						&db->cases[term->as.match.first_case + i];
					if (binder_cursor + old_case->binder_count > 256) {
						return -1;
					}
					for (uint32_t j = 0; j < old_case->binder_count; ++j) {
						binder_storage[binder_cursor + j] =
							db->case_binders[old_case->first_binder + j];
					}
					case_inputs[i].case_label_symbol_id =
						db->case_label_symbols[term->as.match.first_case + i];
					case_inputs[i].constructor_owner = old_case->constructor_owner;
					case_inputs[i].constructor_id = old_case->constructor_id;
					case_inputs[i].binders = &binder_storage[binder_cursor];
					case_inputs[i].binder_count = old_case->binder_count;
					case_inputs[i].body = old_case->body;
					binder_cursor += old_case->binder_count;
				}
				return prototype_term_match_with_frame(
					db,
					scrutinee,
					case_inputs,
					term->as.match.case_count,
					term->as.match.frame_id,
					p_ret
				);
			}

			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				if (!constructor_member_matches(
						db,
						type_declarations,
						definitions,
						head,
						match_case
					)) {
					continue;
				}
				if (match_case->binder_count != arg_count) {
					return -1;
				}

				uint32_t body = match_case->body;
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					struct prototype_case_binder binder = db->case_binders[match_case->first_binder + j];
					if (substitute_term(
							db,
							type_declarations,
							body,
							binder.binder_id,
							args[j],
							&body
						) != 0) {
						return -1;
					}
				}
				return evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					body,
					p_ret,
					depth - 1
				);
			}

			*p_ret = term_id;
			return 0;
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			if (!(options.flags & PROTOTYPE_TERM_REDUCE_INDUCTION)) {
				*p_ret = term_id;
				return 0;
			}
			uint32_t argument;
			if (evaluate_steps(
				db,
				type_declarations,
				definitions,
				options,
				term->as.induction_hypothesis.argument,
				&argument,
				depth - 1
			) != 0) {
				return -1;
			}

			if (term->as.induction_hypothesis.frame_id >= db->match_frame_count) {
				return -1;
			}
			uint32_t match_term = db->match_frames[term->as.induction_hypothesis.frame_id].match_term;
			if (match_term >= db->term_count || db->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
				*p_ret = term_id;
				return 0;
			}

			const struct prototype_term* frame_match = &db->terms[match_term];
			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			if (frame_match->as.match.case_count > 64) {
				return -1;
			}
			for (uint32_t i = 0; i < frame_match->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[frame_match->as.match.first_case + i];
				if (binder_cursor + old_case->binder_count > 256) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binder_storage[binder_cursor + j] =
						db->case_binders[old_case->first_binder + j];
				}
				case_inputs[i].case_label_symbol_id =
					db->case_label_symbols[frame_match->as.match.first_case + i];
				case_inputs[i].constructor_owner = old_case->constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = old_case->body;
				binder_cursor += old_case->binder_count;
			}

			uint32_t recursive_match;
			if (prototype_term_match_with_frame(
				db,
				argument,
				case_inputs,
				frame_match->as.match.case_count,
				term->as.induction_hypothesis.frame_id,
				&recursive_match
			) != 0) {
				return -1;
			}
			return evaluate_steps(
				db,
				type_declarations,
				definitions,
				options,
				recursive_match,
				p_ret,
				depth - 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_VAR:
			case PROTOTYPE_TERM_CONSTRUCTOR:
			case PROTOTYPE_TERM_TYPE_FORMER:
			case PROTOTYPE_TERM_TYPE_DECLARATION:
			case PROTOTYPE_TERM_TYPE_VIEW:
				case PROTOTYPE_TERM_UNIVERSE_VAR:
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				case PROTOTYPE_TERM_INT_LITERAL:
				case PROTOTYPE_TERM_INTRINSIC:
				case PROTOTYPE_TERM_EFFECT_LABEL:
				case PROTOTYPE_TERM_EFFECT_TYPE:
				default:
					*p_ret = term_id;
					return 0;
	}
}

int prototype_term_evaluate(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	int term_id,
	uint32_t* p_ret
) {
	if (term_id < 0) {
		return -1;
	}
	return evaluate_steps(
		db,
		type_declarations,
		NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
			},
		(uint32_t)term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
}

int prototype_term_whnf(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return evaluate_steps(
		db,
		type_declarations,
		NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
			},
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
}

int prototype_term_whnf_with_definitions(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return evaluate_steps(
		db,
		type_declarations,
		definitions,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_REDUCE_DEFINITIONS
			},
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
}

int prototype_term_whnf_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return prototype_term_perform_with_options(
		db,
		type_declarations,
		definitions,
		options,
		term_id,
		p_ret
	);
}

int prototype_term_whnf_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint32_t* p_ret
) {
	struct prototype_term_reduction_options options;
	if (normalization_profile_options(profile, definitions, &options) != 0) {
		return -1;
	}
	return prototype_term_whnf_with_options(
		db,
		type_declarations,
		definitions,
		options,
		term_id,
		p_ret
	);
}

int prototype_term_perform_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
) {
	options = normalize_reduction_options(options);
	int profile = normalization_profile_from_options(definitions, options);
	if (profile == 0 || !db || !p_ret) {
		return evaluate_steps(
			db,
			type_declarations,
			definitions,
			options,
			term_id,
			p_ret,
			PROTOTYPE_EVALUATION_DEPTH_LIMIT
		);
	}

	uint32_t cached;
	if (normalization_cache_lookup(db, profile, term_id, &cached)) {
		db->normalization_cache_stats.hit_count++;
		*p_ret = cached;
		return 0;
	}
	db->normalization_cache_stats.miss_count++;
	struct prototype_term_normalization_cache_entry* entry =
		normalization_cache_reserve(db, profile, term_id);
	uint64_t entry_revision = db ? db->normalization_graph_revision : 0;
	int status = evaluate_steps(
		db,
		type_declarations,
		definitions,
		options,
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
	if (!entry) {
		return status;
	}
	if (status == 0 && db && entry_revision == db->normalization_graph_revision) {
		entry->result_term_id = *p_ret;
		entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_COMPLETE;
	} else {
		entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY;
	}
	return status;
}

static int match_frame_keys_equal(
	const struct prototype_term_db* db,
	uint32_t left_frame,
	uint32_t right_frame
) {
	if (left_frame == right_frame) {
		return 1;
	}
	if (!db ||
		left_frame >= db->match_frame_count ||
		right_frame >= db->match_frame_count ||
		!db->match_frames[left_frame].key.is_linkable ||
		!db->match_frames[right_frame].key.is_linkable) {
		return 0;
	}
	const struct prototype_match_frame_key* left = &db->match_frames[left_frame].key;
	const struct prototype_match_frame_key* right = &db->match_frames[right_frame].key;
	return left->case_count == right->case_count &&
		left->match_key.hash == right->match_key.hash &&
		left->match_key.node_count == right->match_key.node_count &&
		left->match_key.bound_binder_count == right->match_key.bound_binder_count &&
		left->match_key.free_binder_count == right->match_key.free_binder_count &&
		left->match_key.has_frame_local_reference == right->match_key.has_frame_local_reference;
}

static int normalization_equal_at_depth(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	int* p_equal,
	uint32_t depth
) {
	if (!db || !p_equal || left >= db->term_count || right >= db->term_count || depth > 64) {
		return -1;
	}
	*p_equal = 0;

	uint32_t left_whnf;
	uint32_t right_whnf;
	if (prototype_term_whnf_with_options(db, type_declarations, definitions, options, left, &left_whnf) != 0 ||
		prototype_term_whnf_with_options(db, type_declarations, definitions, options, right, &right_whnf) != 0 ||
		left_whnf >= db->term_count ||
		right_whnf >= db->term_count) {
		return -1;
	}

	int shape_equal = 0;
	if (prototype_term_view_shape_equal(
			db,
			left_whnf,
			right_whnf,
			&shape_equal
		) == 0 && shape_equal) {
		*p_equal = 1;
		return 0;
	}

	const struct prototype_term* left_term = &db->terms[left_whnf];
	const struct prototype_term* right_term = &db->terms[right_whnf];
	if (left_term->tag != right_term->tag) {
		return 0;
	}

	switch (left_term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t left_representation_id;
			uint32_t right_representation_id;
			if (left_term->as.constructor.constructor_id != right_term->as.constructor.constructor_id) {
				return 0;
			}
			if (constructor_owner_representation_id(
					db,
					left_term->as.constructor.owner,
					&left_representation_id
				) != 0 ||
				constructor_owner_representation_id(
					db,
					right_term->as.constructor.owner,
					&right_representation_id
				) != 0) {
				return -1;
			}
			*p_equal = left_representation_id == right_representation_id;
			return 0;
		}
		case PROTOTYPE_TERM_APP: {
			int equal = 0;
			if (normalization_equal_at_depth(
					db,
					type_declarations,
					definitions,
					options,
					left_term->as.app.function,
					right_term->as.app.function,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.app.argument,
				right_term->as.app.argument,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t left_var;
			uint32_t right_body;
			if (prototype_term_var(db, left_term->as.lambda.binder_id, &left_var) != 0 ||
				substitute_term(
					db,
					type_declarations,
					right_term->as.lambda.body,
					right_term->as.lambda.binder_id,
					left_var,
					&right_body
				) != 0) {
				return -1;
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.lambda.body,
				right_body,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_PI: {
			int equal = 0;
			if (normalization_equal_at_depth(
					db,
					type_declarations,
					definitions,
					options,
					left_term->as.pi.domain,
					right_term->as.pi.domain,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.pi.codomain_family,
				right_term->as.pi.codomain_family,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH:
			if (left_term->as.match.case_count != right_term->as.match.case_count) {
				return 0;
			}
			for (uint32_t i = 0; i < left_term->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&db->cases[left_term->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&db->cases[right_term->as.match.first_case + i];
				if (left_case->constructor_id != right_case->constructor_id ||
					left_case->binder_count != right_case->binder_count) {
					return 0;
				}
				int equal = 0;
				if (left_case->constructor_owner != PROTOTYPE_INVALID_ID ||
					right_case->constructor_owner != PROTOTYPE_INVALID_ID) {
					if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
						right_case->constructor_owner == PROTOTYPE_INVALID_ID ||
						normalization_equal_at_depth(
							db,
							type_declarations,
							definitions,
							options,
							left_case->constructor_owner,
							right_case->constructor_owner,
							&equal,
							depth + 1
						) != 0 || !equal) {
						return 0;
					}
				}
				if (normalization_equal_at_depth(
						db,
						type_declarations,
						definitions,
						options,
						left_case->body,
						right_case->body,
						&equal,
						depth + 1
					) != 0 || !equal) {
					return 0;
				}
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.match.scrutinee,
				right_term->as.match.scrutinee,
				p_equal,
				depth + 1
			);
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (!match_frame_keys_equal(
					db,
					left_term->as.induction_hypothesis.frame_id,
					right_term->as.induction_hypothesis.frame_id
				)) {
				return 0;
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.induction_hypothesis.argument,
				right_term->as.induction_hypothesis.argument,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_FORMER:
			*p_equal = left_term->as.type_former.representation_id ==
				right_term->as.type_former.representation_id;
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION: {
			if (!type_declarations ||
				left_term->as.type_declaration.type_id >= type_declarations->type_count ||
				right_term->as.type_declaration.type_id >= type_declarations->type_count) {
				return -1;
			}
			*p_equal = left_term->as.type_declaration.type_id ==
				right_term->as.type_declaration.type_id;
			return 0;
		}
			case PROTOTYPE_TERM_TYPE_VIEW:
				if (left_term->as.type_view.view_type_id !=
					right_term->as.type_view.view_type_id) {
				*p_equal = 0;
				return 0;
			}
			if (normalization_equal_at_depth(
					db,
					type_declarations,
					definitions,
					options,
					left_term->as.type_view.core,
					right_term->as.type_view.core,
					p_equal,
					depth + 1
				) != 0 ||
				!*p_equal) {
				return 0;
			}
			return normalization_equal_at_depth(
				db,
				type_declarations,
				definitions,
				options,
				left_term->as.type_view.source,
				right_term->as.type_view.source,
					p_equal,
					depth + 1
				);
			case PROTOTYPE_TERM_EFFECT_LABEL:
				*p_equal = left_term->as.effect_label.effects ==
					right_term->as.effect_label.effects;
				return 0;
			case PROTOTYPE_TERM_EFFECT_TYPE: {
				int equal = 0;
				if (normalization_equal_at_depth(
						db,
						type_declarations,
						definitions,
						options,
						left_term->as.effect_type.label,
						right_term->as.effect_type.label,
						&equal,
						depth + 1
					) != 0 || !equal) {
					return 0;
				}
				return normalization_equal_at_depth(
					db,
					type_declarations,
					definitions,
					options,
					left_term->as.effect_type.result,
					right_term->as.effect_type.result,
					p_equal,
					depth + 1
				);
			}
			default:
				*p_equal = left_whnf == right_whnf;
				return 0;
	}
}

int prototype_term_normalization_equal(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return normalization_equal_at_depth(
		db,
		type_declarations,
		NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
			},
		left,
		right,
		p_equal,
		0
	);
}

int prototype_term_normalization_equal_with_definitions(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return normalization_equal_at_depth(
		db,
		type_declarations,
		definitions,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_REDUCE_DEFINITIONS
			},
		left,
		right,
		p_equal,
		0
	);
}

int prototype_term_normalization_equal_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return normalization_equal_at_depth(
		db,
		type_declarations,
		definitions,
		options,
		left,
		right,
		p_equal,
		0
		);
}

static int int_literal_fits_int32(int64_t value) {
	return value >= INT32_MIN && value <= INT32_MAX;
}

static int64_t int32_from_bits(uint32_t bits) {
	if (bits <= (uint32_t)INT32_MAX) {
		return (int64_t)bits;
	}
	return -1 - (int64_t)(UINT32_MAX - bits);
}

static int64_t int64_from_bits(uint64_t bits) {
	if (bits <= (uint64_t)INT64_MAX) {
		return (int64_t)bits;
	}
	return -1 - (int64_t)(UINT64_MAX - bits);
}

static int host_oracle_is_integer_arithmetic(int oracle_kind) {
	return oracle_kind == PROTOTYPE_HOST_ORACLE_INT_ADD ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_SUB ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_MUL ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_NEG;
}

static int int_intrinsic_arity(int intrinsic_id) {
	const struct prototype_host_intrinsic_signature* signature =
		prototype_term_host_intrinsic_signature(intrinsic_id);
	if (!signature || !host_oracle_is_integer_arithmetic(signature->oracle_kind)) {
		return 0;
	}
	return (int)signature->arity;
}

static int execute_int_intrinsic(
	struct prototype_term_db* db,
	int intrinsic_id,
	const int64_t* arguments,
	uint32_t argument_count,
	uint32_t* p_ret
) {
	if (!db || !arguments || !p_ret ||
		argument_count != (uint32_t)int_intrinsic_arity(intrinsic_id)) {
		return -1;
	}
	const struct prototype_host_intrinsic_signature* signature =
		prototype_term_host_intrinsic_signature(intrinsic_id);
	if (!signature || !host_oracle_is_integer_arithmetic(signature->oracle_kind)) {
		return -1;
	}

	int result_width = prototype_term_host_type_bit_width(signature->result_type);
	if (result_width == 32) {
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (!int_literal_fits_int32(arguments[i])) {
				return -1;
			}
		}
		uint32_t result_bits;
		switch (signature->oracle_kind) {
			case PROTOTYPE_HOST_ORACLE_INT_ADD:
				result_bits = (uint32_t)arguments[0] + (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_SUB:
				result_bits = (uint32_t)arguments[0] - (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_MUL:
				result_bits = (uint32_t)arguments[0] * (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_NEG:
				result_bits = 0u - (uint32_t)arguments[0];
				break;
			default:
				return -1;
		}
		return prototype_term_int_literal(db, int32_from_bits(result_bits), p_ret);
	}
	if (result_width != 64) {
		return -1;
	}

	uint64_t result_bits;
	switch (signature->oracle_kind) {
		case PROTOTYPE_HOST_ORACLE_INT_ADD:
			result_bits = (uint64_t)arguments[0] + (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_SUB:
			result_bits = (uint64_t)arguments[0] - (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_MUL:
			result_bits = (uint64_t)arguments[0] * (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_NEG:
			result_bits = 0ull - (uint64_t)arguments[0];
			break;
		default:
			return -1;
	}
	return prototype_term_int_literal(db, int64_from_bits(result_bits), p_ret);
}

static int rebuild_app_spine(
	struct prototype_term_db* db,
	uint32_t head,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_ret
) {
	if (!db || !p_ret || (argument_count > 0 && !arguments)) {
		return -1;
	}
	uint32_t current = head;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_term_app(db, current, arguments[i], &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

static int host_effect_allowed(
	struct prototype_term_reduction_options options,
	unsigned effects
) {
	if (effects == PROTOTYPE_HOST_EFFECT_NONE) {
		return (options.flags & PROTOTYPE_TERM_REDUCE_HOST_PURE) != 0;
	}
	return (options.flags & PROTOTYPE_TERM_PERFORM_HOST_EFFECT) != 0 &&
		(options.effect_capabilities & effects) == effects;
}

static int perform_host_step_rebuild_if_changed(
	struct prototype_term_db* db,
	uint32_t head,
	const uint32_t* args,
	uint32_t arg_count,
	int changed,
	uint32_t original,
	uint32_t* p_ret
) {
	if (!changed) {
		*p_ret = original;
		return 0;
	}
	return rebuild_app_spine(db, head, args, arg_count, p_ret) == 0 ? 1 : -1;
}

static int lookup_nat_runtime_shape(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	struct symbol_table* symbols,
	uint32_t* p_nat_owner,
	const struct prototype_type_constructor_declaration** p_zero_constructor,
	const struct prototype_type_constructor_declaration** p_succ_constructor
) {
	if (!db || !type_declarations || !symbols || !p_nat_owner ||
		!p_zero_constructor || !p_succ_constructor) {
		return -1;
	}
	int nat_symbol = symbol_intern(symbols, "#.Nat", 5);
	int zero_symbol = symbol_intern(symbols, "zero", 4);
	int succ_symbol = symbol_intern(symbols, "succ", 4);
	if (nat_symbol < 0 || zero_symbol < 0 || succ_symbol < 0) {
		return -1;
	}
	const struct prototype_type_declaration* nat_type =
		prototype_type_declaration_lookup(type_declarations, nat_symbol);
	if (!nat_type) {
		return 1;
	}
	*p_zero_constructor =
		prototype_type_declaration_lookup_constructor(type_declarations, nat_type->type_index, zero_symbol);
	*p_succ_constructor =
		prototype_type_declaration_lookup_constructor(type_declarations, nat_type->type_index, succ_symbol);
	if (!*p_zero_constructor || !*p_succ_constructor) {
		return -1;
	}
	if (prototype_term_type_instance_make(
			db,
			type_declarations,
			nat_type->type_index,
			NULL,
			0,
			p_nat_owner
		) != 0) {
		return -1;
	}
	return 0;
}

static int perform_host_step(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !type_declarations || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	if (depth == 0 || db->terms[term_id].tag != PROTOTYPE_TERM_APP) {
		*p_ret = term_id;
		return 0;
	}

	uint32_t head;
	uint32_t args[64];
	uint32_t arg_count;
	decompose_app(db, term_id, &head, args, &arg_count);
	if (head >= db->term_count ||
		db->terms[head].tag != PROTOTYPE_TERM_INTRINSIC) {
		*p_ret = term_id;
		return 0;
	}

	int intrinsic_id = db->terms[head].as.intrinsic.intrinsic_id;
	const struct prototype_host_intrinsic_signature* signature =
		prototype_term_host_intrinsic_signature(intrinsic_id);
	if (!signature || arg_count != signature->arity ||
		!host_effect_allowed(options, signature->effects)) {
		*p_ret = term_id;
		return 0;
	}
	if (signature->effects != PROTOTYPE_HOST_EFFECT_NONE &&
		(!options.effect_output || !options.symbols)) {
		return -1;
	}

	uint32_t reduced_args[64];
	int changed = 0;
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (evaluate_steps(
				db,
				type_declarations,
				definitions,
				options,
				args[i],
				&reduced_args[i],
				depth - 1
			) != 0) {
			return -1;
		}
		if (reduced_args[i] != args[i]) {
			changed = 1;
		}
	}

	if (host_oracle_is_integer_arithmetic(signature->oracle_kind)) {
		int64_t int_args[PROTOTYPE_HOST_INTRINSIC_MAX_ARITY];
		for (uint32_t i = 0; i < arg_count; ++i) {
			if (reduced_args[i] >= db->term_count ||
				db->terms[reduced_args[i]].tag != PROTOTYPE_TERM_INT_LITERAL) {
				return perform_host_step_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			int_args[i] = db->terms[reduced_args[i]].as.int_literal.value;
		}
		return execute_int_intrinsic(
			db,
			intrinsic_id,
			int_args,
			arg_count,
			p_ret
		) == 0 ? 1 : -1;
	}

	if (signature->oracle_kind == PROTOTYPE_HOST_ORACLE_PRINT) {
		if (arg_count != 1 ||
			reduced_args[0] >= db->term_count ||
			db->terms[reduced_args[0]].tag != PROTOTYPE_TERM_TEXT_LITERAL) {
			return perform_host_step_rebuild_if_changed(
				db,
				head,
				reduced_args,
				arg_count,
				changed,
				term_id,
				p_ret
			);
		}
		const char* text = symbol_to_string(
			options.symbols,
			db->terms[reduced_args[0]].as.text_literal.text_symbol_id
		);
		if (text) {
			fputs(text, options.effect_output);
		}
		if (options.p_effect_performed) {
			*options.p_effect_performed = 1;
		}
		*p_ret = reduced_args[0];
		return 1;
	}

	if (!options.symbols) {
		return perform_host_step_rebuild_if_changed(
			db,
			head,
			reduced_args,
			arg_count,
			changed,
			term_id,
			p_ret
		);
	}

	uint32_t nat_owner;
	const struct prototype_type_constructor_declaration* zero_constructor;
	const struct prototype_type_constructor_declaration* succ_constructor;
	int nat_status = lookup_nat_runtime_shape(
		db,
		type_declarations,
		options.symbols,
		&nat_owner,
		&zero_constructor,
		&succ_constructor
	);
	if (nat_status < 0) {
		return -1;
	}
	if (nat_status > 0) {
		return perform_host_step_rebuild_if_changed(
			db,
			head,
			reduced_args,
			arg_count,
			changed,
			term_id,
			p_ret
		);
	}

	if (signature->oracle_kind == PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT) {
		if (arg_count != 1 ||
			reduced_args[0] >= db->term_count ||
			db->terms[reduced_args[0]].tag != PROTOTYPE_TERM_TEXT_LITERAL) {
			return perform_host_step_rebuild_if_changed(
				db,
				head,
				reduced_args,
				arg_count,
				changed,
				term_id,
				p_ret
			);
		}
		const char* text = symbol_to_string(
			options.symbols,
			db->terms[reduced_args[0]].as.text_literal.text_symbol_id
		);
		if (!text || *text == '\0') {
			return -1;
		}
		unsigned long value = 0;
		for (const char* p = text; *p; ++p) {
			if (*p < '0' || *p > '9') {
				return -1;
			}
			value = value * 10u + (unsigned long)(*p - '0');
			if (value > 100000u) {
				return -1;
			}
		}
		uint32_t current;
		if (prototype_term_constructor(
				db,
				nat_owner,
				zero_constructor->constructor_index,
				&current
			) != 0) {
			return -1;
		}
		for (unsigned long i = 0; i < value; ++i) {
			uint32_t succ;
			if (prototype_term_constructor(
					db,
					nat_owner,
					succ_constructor->constructor_index,
					&succ
				) != 0 ||
				prototype_term_app(db, succ, current, &current) != 0) {
				return -1;
			}
		}
		*p_ret = current;
		return 1;
	}

	if (signature->oracle_kind == PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT) {
		if (arg_count != 1) {
			return perform_host_step_rebuild_if_changed(
				db,
				head,
				reduced_args,
				arg_count,
				changed,
				term_id,
				p_ret
			);
		}
		uint32_t current = reduced_args[0];
		unsigned long value = 0;
		for (;;) {
			uint32_t evaluated_current;
			if (evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					current,
					&evaluated_current,
					depth - 1
				) != 0) {
				return -1;
			}
			current = evaluated_current;

			uint32_t nat_head;
			uint32_t nat_args[2];
			uint32_t nat_arg_count;
			decompose_app(db, current, &nat_head, nat_args, &nat_arg_count);
			if (nat_head >= db->term_count ||
				db->terms[nat_head].tag != PROTOTYPE_TERM_CONSTRUCTOR) {
				return perform_host_step_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			int same_owner = 0;
			if (prototype_term_core_shape_equal(
					db,
					db->terms[nat_head].as.constructor.owner,
					nat_owner,
					&same_owner
				) != 0) {
				return -1;
			}
			if (!same_owner) {
				return perform_host_step_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			if (db->terms[nat_head].as.constructor.constructor_id ==
					zero_constructor->constructor_index &&
				nat_arg_count == 0) {
				break;
			}
			if (db->terms[nat_head].as.constructor.constructor_id !=
					succ_constructor->constructor_index ||
				nat_arg_count != 1) {
				return -1;
			}
			value++;
			if (value > 100000u) {
				return -1;
			}
			current = nat_args[0];
		}
		char buffer[32];
		int len = snprintf(buffer, sizeof(buffer), "%lu", value);
		if (len < 0 || (size_t)len >= sizeof(buffer)) {
			return -1;
		}
		int text_symbol = symbol_intern(options.symbols, buffer, (size_t)len);
		if (text_symbol < 0 ||
			prototype_term_text_literal(db, text_symbol, p_ret) != 0) {
			return -1;
		}
		return 1;
	}

	return perform_host_step_rebuild_if_changed(
		db,
		head,
		reduced_args,
		arg_count,
		changed,
		term_id,
		p_ret
	);
}

int prototype_term_normalization_equal_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	struct prototype_term_reduction_options options;
	if (normalization_profile_options(profile, definitions, &options) != 0) {
		return -1;
	}
	return prototype_term_normalization_equal_with_options(
		db,
		type_declarations,
		definitions,
		options,
		left,
		right,
		p_equal
	);
}

int prototype_term_execute_host_intrinsic(
	FILE* output,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_ret
) {
	if (!output || !symbols || !type_declarations || !db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	uint32_t result;
	if (prototype_term_perform_with_options(
			db,
			type_declarations,
			NULL,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_PERFORM_HOST_EFFECT,
				.effect_output = output,
				.symbols = symbols,
				.effect_capabilities = PROTOTYPE_HOST_EFFECT_TERMINAL
			},
			term_id,
			&result
		) != 0) {
		return -1;
	}
	*p_ret = result;
	return result != term_id ? 1 : 0;
}

static const char* safe_symbol_name(const struct symbol_table* symbols, int symbol_id) {
	if (symbol_id == PROTOTYPE_BASE_NAMESPACE_ID) {
		return "_";
	}
	const char* name = symbol_to_string(symbols, symbol_id);
	return name ? name : "<unknown>";
}

static void print_escaped_text(FILE* output, const char* text) {
	if (!output || !text) {
		return;
	}
	for (const char* p = text; *p != '\0'; ++p) {
		switch (*p) {
			case '\n':
				fprintf(output, "\\n");
				break;
			case '\r':
				fprintf(output, "\\r");
				break;
			case '\t':
				fprintf(output, "\\t");
				break;
			case '"':
				fprintf(output, "\\\"");
				break;
			case '\\':
				fprintf(output, "\\\\");
				break;
			default:
				fputc(*p, output);
				break;
		}
	}
}

static void print_type_head(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id
) {
	if (type_id >= type_declarations->type_count) {
		fprintf(output, "<bad-type:%u>", type_id);
		return;
	}
	fprintf(output, "%s", safe_symbol_name(symbols, type_declarations->type_declarations[type_id].name_symbol_id));
}

static void print_term_depth(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	unsigned depth
) {
	if (!output || !symbols || !type_declarations || !terms) {
		return;
	}
	if (term_id >= terms->term_count) {
		fprintf(output, "<bad-term:%u>", term_id);
		return;
	}
	if (depth == 0) {
		fprintf(output, "...");
		return;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(output, "_");
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			fprintf(output, "<external ");
			if (term->as.external_ref.name.namespace_symbol_id >= 0) {
				fprintf(output, "%s.", safe_symbol_name(symbols, term->as.external_ref.name.namespace_symbol_id));
			}
			fprintf(output, "%s>", safe_symbol_name(symbols, term->as.external_ref.name.name_symbol_id));
			break;
		case PROTOTYPE_TERM_INTRINSIC:
			fprintf(output, "#.%s", safe_symbol_name(symbols, term->as.intrinsic.symbol_id));
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t type_id;
			if (type_instance_type_id(terms, term->as.constructor.owner, &type_id) != 0 ||
				type_id >= type_declarations->type_count) {
				uint32_t representation_id;
				if (constructor_owner_representation_id(
						terms,
						term->as.constructor.owner,
						&representation_id
					) != 0) {
					fprintf(output, "<bad-constructor>");
				} else {
					fprintf(output, "rep#%u.ordinal#%u", representation_id,
						term->as.constructor.constructor_id);
				}
				break;
			}
			const struct prototype_type_declaration* type = &type_declarations->type_declarations[type_id];
			if (term->as.constructor.constructor_id >= type->constructor_count) {
				fprintf(output, "<bad-constructor>");
				break;
			}
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[type->first_constructor + term->as.constructor.constructor_id];
			print_term_depth(output, symbols, type_declarations, terms, term->as.constructor.owner, depth - 1);
			fprintf(output, ".%s", safe_symbol_name(symbols, constructor->name_symbol_id));
			break;
		}
		case PROTOTYPE_TERM_APP:
			fprintf(output, "(");
			print_term_depth(output, symbols, type_declarations, terms, term->as.app.function, depth - 1);
			fprintf(output, " ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.app.argument, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(output, "(\\_ => ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.lambda.body, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(output, "PI(");
			print_term_depth(output, symbols, type_declarations, terms, term->as.pi.domain, depth - 1);
			fprintf(output, ", ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.pi.codomain_family, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(output, "match ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.match.scrutinee, depth - 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				fprintf(
					output,
					" @%s => ",
					safe_symbol_name(symbols, terms->case_label_symbols[term->as.match.first_case + i])
				);
				print_term_depth(output, symbols, type_declarations, terms, match_case->body, depth - 1);
			}
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			fprintf(output, "TYPE_FORMER(rep#%u)", term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(output, "TYPE_DECLARATION(");
			print_type_head(output, symbols, type_declarations, term->as.type_declaration.type_id);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(output, "TYPE_VIEW(");
			print_type_head(output, symbols, type_declarations, term->as.type_view.view_type_id);
			fprintf(output, ", core=");
			print_term_depth(output, symbols, type_declarations, terms, term->as.type_view.core, depth - 1);
			fprintf(output, ", source=");
			print_term_depth(output, symbols, type_declarations, terms, term->as.type_view.source, depth - 1);
			fprintf(output, ")");
			break;
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				fprintf(output, "@?u%u", term->as.universe_var.level_var);
				break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				fprintf(output, "#.Text");
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(output, "#\"");
				print_escaped_text(output, safe_symbol_name(symbols, term->as.text_literal.text_symbol_id));
				fprintf(output, "\"");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
				fprintf(output, "#.Int");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				fprintf(output, "#.Int64");
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(output, "#%" PRId64, term->as.int_literal.value);
				break;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
					fprintf(output, "*");
					print_term_depth(output, symbols, type_declarations, terms, term->as.induction_hypothesis.argument, depth - 1);
					break;
				case PROTOTYPE_TERM_EFFECT_LABEL:
					fprintf(output, "Terminal");
					break;
				case PROTOTYPE_TERM_EFFECT_TYPE:
					fprintf(output, "Effect(");
					print_term_depth(output, symbols, type_declarations, terms, term->as.effect_type.label, depth - 1);
					fprintf(output, ", ");
					print_term_depth(output, symbols, type_declarations, terms, term->as.effect_type.result, depth - 1);
					fprintf(output, ")");
					break;
			default:
				fprintf(output, "<unknown-term>");
				break;
	}
}

void prototype_term_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	print_term_depth(output, symbols, type_declarations, terms, term_id, 64);
}

static void print_term_debug_depth(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	unsigned depth
);

static void print_constructor_name(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_term* term
) {
	uint32_t type_id;
	if (type_instance_type_id(terms, term->as.constructor.owner, &type_id) != 0 ||
		type_id >= type_declarations->type_count) {
		uint32_t representation_id;
		if (constructor_owner_representation_id(
				terms,
				term->as.constructor.owner,
				&representation_id
			) != 0) {
			fprintf(output, "<bad-constructor>");
		} else {
			fprintf(output, "rep#%u.ordinal#%u", representation_id,
				term->as.constructor.constructor_id);
		}
		return;
	}

	const struct prototype_type_declaration* type = &type_declarations->type_declarations[type_id];
	if (term->as.constructor.constructor_id >= type->constructor_count) {
		fprintf(output, "<bad-constructor>");
		return;
	}

	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[type->first_constructor + term->as.constructor.constructor_id];
	print_term_debug_depth(output, symbols, type_declarations, terms, term->as.constructor.owner, 32);
	fprintf(output, ".%s", safe_symbol_name(symbols, constructor->name_symbol_id));
}

static void print_term_debug_depth(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	unsigned depth
) {
	if (!output || !symbols || !type_declarations || !terms) {
		return;
	}
	if (term_id >= terms->term_count) {
		fprintf(output, "BAD_TERM(%u)", term_id);
		return;
	}
	if (depth == 0) {
		fprintf(output, "...");
		return;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(output, "VAR(_#%u)", term->as.var.binder_id);
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			fprintf(output, "EXTERNAL_REF(");
			if (term->as.external_ref.name.namespace_symbol_id >= 0) {
				fprintf(output, "%s.", safe_symbol_name(symbols, term->as.external_ref.name.namespace_symbol_id));
			}
			fprintf(output, "%s)", safe_symbol_name(symbols, term->as.external_ref.name.name_symbol_id));
			break;
		case PROTOTYPE_TERM_INTRINSIC:
			fprintf(output, "INTRINSIC(%s", safe_symbol_name(symbols, term->as.intrinsic.symbol_id));
			if (term->as.intrinsic.type_symbol_id >= 0) {
				fprintf(output, ":%s", safe_symbol_name(symbols, term->as.intrinsic.type_symbol_id));
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			fprintf(output, "CONSTRUCTOR(");
			print_constructor_name(output, symbols, type_declarations, terms, term);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_APP:
			fprintf(output, "APP(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.function, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.argument, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(output,
				"LAMBDA(_#%u, ",
				term->as.lambda.binder_id);
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.lambda.body, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(output, "PI(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.domain, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.codomain_family, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(output, "MATCH(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.match.scrutinee, depth - 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				fprintf(
					output,
					", CASE(%s",
					safe_symbol_name(symbols, terms->case_label_symbols[term->as.match.first_case + i])
				);
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					struct prototype_case_binder binder = terms->case_binders[match_case->first_binder + j];
					fprintf(output, " _#%u", binder.binder_id);
				}
				fprintf(output, " -> ");
				print_term_debug_depth(output, symbols, type_declarations, terms, match_case->body, depth - 1);
				fprintf(output, ")");
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			fprintf(output, "TYPE_FORMER(rep#%u)", term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(output, "TYPE_DECLARATION(");
			print_type_head(output, symbols, type_declarations, term->as.type_declaration.type_id);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(output, "TYPE_VIEW(");
			print_type_head(output, symbols, type_declarations, term->as.type_view.view_type_id);
			fprintf(output, ", core=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.core, depth - 1);
			fprintf(output, ", source=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.source, depth - 1);
			fprintf(output, ")");
			break;
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				fprintf(output, "UNIVERSE(?u%u)", term->as.universe_var.level_var);
				break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				fprintf(output, "PRIMITIVE(Text)");
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(output, "TEXT_LITERAL(\"");
				print_escaped_text(output, safe_symbol_name(symbols, term->as.text_literal.text_symbol_id));
				fprintf(output, "\")");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
				fprintf(output, "PRIMITIVE(Int)");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				fprintf(output, "PRIMITIVE(Int64)");
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(output, "INT_LITERAL(%" PRId64 ")", term->as.int_literal.value);
				break;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
					fprintf(output, "INDUCTION_HYPOTHESIS(frame#%u, ",
						term->as.induction_hypothesis.frame_id);
			print_term_debug_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.induction_hypothesis.argument,
				depth - 1
			);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				fprintf(output, "EFFECT_LABEL(%u)", term->as.effect_label.effects);
				break;
			case PROTOTYPE_TERM_EFFECT_TYPE:
				fprintf(output, "EFFECT_TYPE(");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.effect_type.label, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.effect_type.result, depth - 1);
				fprintf(output, ")");
				break;
			default:
				fprintf(output, "UNKNOWN_TERM");
				break;
	}
}

void prototype_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	print_term_debug_depth(output, symbols, type_declarations, terms, term_id, 64);
}
