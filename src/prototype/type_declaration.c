#include "type_declaration.h"
#include "context.h"
#include "term.h"

#include <stdlib.h>
#include <string.h>

#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY 512
#define PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY 512
#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_OFFSET 1469598103934665603ULL
#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_PRIME 1099511628211ULL

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

static int type_declaration_present(const struct prototype_type_declaration* type) {
	return type && type->type_index != PROTOTYPE_INVALID_ID;
}

static int constructor_declaration_present(
	const struct prototype_type_constructor_declaration* constructor
) {
	return constructor && constructor->owner_type != PROTOTYPE_INVALID_ID;
}

struct type_code_shape_key_binder_env {
	const struct prototype_context_db* contexts;
	uint32_t binder_id[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t slot[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
	uint32_t level_var[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t level_slot[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t level_count;
	uint32_t next_level_slot;
};

struct representation_compare_env {
	const struct prototype_context_db* contexts;
	uint32_t left_binders[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t right_binders[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t binder_count;
	uint32_t left_types[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t right_types[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t type_count;
};

static int representation_types_equal_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t left_type_id,
	uint32_t right_type_id,
	struct representation_compare_env* env,
	uint32_t depth
);

static int representation_terms_equal_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t left_term_id,
	uint32_t right_term_id,
	struct representation_compare_env* env,
	uint32_t depth
);

static void type_code_shape_key_hash_mix_u32(uint64_t* p_hash, uint32_t value) {
	*p_hash ^= (uint64_t)value;
	*p_hash *= PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_PRIME;
}

static void type_code_shape_key_hash_mix_tag(uint64_t* p_hash, uint32_t tag) {
	type_code_shape_key_hash_mix_u32(p_hash, 0x9e3779b9U);
	type_code_shape_key_hash_mix_u32(p_hash, tag);
}

static void type_code_shape_key_hash_mix_key(
	uint64_t* p_hash,
	const struct prototype_type_code_shape_key* key
) {
	type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)key->hash);
	type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)(key->hash >> 32));
	type_code_shape_key_hash_mix_u32(p_hash, key->node_count);
	type_code_shape_key_hash_mix_u32(p_hash, key->parameter_count);
	type_code_shape_key_hash_mix_u32(p_hash, key->constructor_count);
	type_code_shape_key_hash_mix_u32(p_hash, key->bound_binder_count);
	type_code_shape_key_hash_mix_u32(p_hash, key->free_binder_count);
	type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)key->has_local_universe_reference);
	type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)key->has_name_reference);
}

static int type_code_shape_key_env_lookup(
	const struct type_code_shape_key_binder_env* env,
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

static int type_code_shape_key_env_push(
	struct type_code_shape_key_binder_env* env,
	uint32_t binder_id
) {
	if (!env || env->count >= PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY) {
		return -1;
	}
	env->binder_id[env->count] = binder_id;
	env->slot[env->count] = env->next_slot++;
	env->count++;
	return 0;
}

static int type_code_shape_key_level_env_lookup(
	const struct type_code_shape_key_binder_env* env,
	uint32_t level_var,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return 0;
	}
	for (uint32_t i = env->level_count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->level_var[index] == level_var) {
			*p_slot = env->level_slot[index];
			return 1;
		}
	}
	return 0;
}

static int type_code_shape_key_level_env_slot(
	struct type_code_shape_key_binder_env* env,
	uint32_t level_var,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return -1;
	}
	if (type_code_shape_key_level_env_lookup(env, level_var, p_slot)) {
		return 0;
	}
	if (env->level_count >= PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY) {
		return -1;
	}
	env->level_var[env->level_count] = level_var;
	env->level_slot[env->level_count] = env->next_level_slot++;
	*p_slot = env->level_slot[env->level_count];
	env->level_count++;
	return 0;
}

void prototype_type_declaration_db_init(
	struct prototype_type_declaration_db* db,
	struct prototype_type_declaration* type_declarations,
	size_t type_capacity,
	struct prototype_type_constructor_declaration* constructor_declarations,
	size_t constructor_capacity,
	struct prototype_type_parameter_declaration* parameter_declarations,
	size_t parameter_capacity,
	uint32_t* readback_field_types,
	size_t readback_field_type_capacity,
	struct prototype_type_expr* exprs,
	size_t expr_capacity
) {
	memset(db, 0, sizeof(*db));
	db->type_declarations = type_declarations;
	db->type_capacity = type_capacity;
	db->constructor_declarations = constructor_declarations;
	db->constructor_capacity = constructor_capacity;
	db->parameter_declarations = parameter_declarations;
	db->parameter_capacity = parameter_capacity;
	db->readback_field_types = readback_field_types;
	db->readback_field_type_capacity = readback_field_type_capacity;
	db->exprs = exprs;
	db->expr_capacity = expr_capacity;
	db->representations = calloc(type_capacity, sizeof(*db->representations));
	if (db->representations) {
		db->representation_capacity = type_capacity;
	}
	db->representations_dirty = 1;
}

static int add_expr(struct prototype_type_declaration_db* db, struct prototype_type_expr expr, uint32_t* p_ret) {
	if (!db || !p_ret || reserve_slot(db->expr_count, db->expr_capacity) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)db->expr_count; ++i) {
		if (memcmp(&db->exprs[i], &expr, sizeof(expr)) == 0) {
			*p_ret = i;
			return 0;
		}
	}

	uint32_t id = (uint32_t)db->expr_count;
	db->exprs[id] = expr;
	db->expr_count++;
	*p_ret = id;
	return 0;
}

int prototype_type_expr_universe(struct prototype_type_declaration_db* db, uint32_t level, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_UNIVERSE;
	expr.as.universe.level = level;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_fresh_universe(struct prototype_type_declaration_db* db, uint32_t* p_ret) {
	if (!db) {
		return -1;
	}

	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR;
	expr.as.universe_var.level_var = db->next_level_var++;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_self(struct prototype_type_declaration_db* db, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_SELF;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_var(struct prototype_type_declaration_db* db, uint32_t binder_id, int symbol_id, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_VAR;
	expr.as.var.binder_id = binder_id;
	expr.as.var.symbol_id = symbol_id;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_name(struct prototype_type_declaration_db* db, int symbol_id, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_NAME;
	expr.as.name.symbol_id = symbol_id;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_primitive(struct prototype_type_declaration_db* db, int tag, uint32_t* p_ret) {
	int host_type;
	if (prototype_term_host_type_from_type_expr_tag(tag, &host_type) != 0) {
		return -1;
	}
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = tag;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_app(struct prototype_type_declaration_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_APP;
	expr.as.app.function = function;
	expr.as.app.argument = argument;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_arrow(struct prototype_type_declaration_db* db, uint32_t domain, uint32_t codomain, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_ARROW;
	expr.as.arrow.domain = domain;
	expr.as.arrow.codomain = codomain;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_pi(
	struct prototype_type_declaration_db* db,
	uint32_t binder_id,
	int symbol_id,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_PI;
	expr.as.pi.binder_id = binder_id;
	expr.as.pi.symbol_id = symbol_id;
	expr.as.pi.domain = domain;
	expr.as.pi.codomain = codomain;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_imported_type(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	const struct prototype_type_code_shape_key* key,
	uint32_t* p_ret
) {
	if (!key) {
		return -1;
	}

	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE;
	expr.as.imported_type.name = name;
	expr.as.imported_type.code_shape_key = *key;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_external_term(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
) {
	if (name.name_symbol_id < 0) {
		return -1;
	}
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM;
	expr.as.external_term.name = name;
	return add_expr(db, expr, p_ret);
}

int prototype_type_declaration_add(
	struct prototype_type_declaration_db* db,
	int name_symbol_id,
	uint32_t* p_type_id
) {
	if (!db || !p_type_id || reserve_slot(db->type_count, db->type_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_count;
	struct prototype_type_declaration* type = &db->type_declarations[id];
	memset(type, 0, sizeof(*type));
	type->name_symbol_id = name_symbol_id;
	type->namespace_symbol_id = -1;
	type->type_index = id;
	type->representation_id = PROTOTYPE_INVALID_ID;
	type->formation_classifier = PROTOTYPE_INVALID_ID;
	type->parameter_context = PROTOTYPE_INVALID_ID;
	type->first_parameter = (uint32_t)db->parameter_count;
	type->first_constructor = (uint32_t)db->constructor_count;

	db->type_count++;
	db->representations_dirty = 1;
	*p_type_id = id;
	return 0;
}

int prototype_type_declaration_add_parameter(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t binder_id,
	int name_symbol_id,
	uint32_t type_expr
) {
	if (!db || type_id >= db->type_count) {
		return -1;
	}
	if (reserve_slot(db->parameter_count, db->parameter_capacity) != 0) {
		return -1;
	}

	struct prototype_type_declaration* type = &db->type_declarations[type_id];
	if ((uint32_t)db->parameter_count != type->first_parameter + type->parameter_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->parameter_count;
	db->parameter_declarations[id].binder_id = binder_id;
	db->parameter_declarations[id].name_symbol_id = name_symbol_id;
	db->parameter_declarations[id].type_expr = type_expr;
	db->parameter_count++;
	type->parameter_count++;
	db->representations_dirty = 1;
	return 0;
}

int prototype_type_declaration_add_constructor(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id,
	const uint32_t* readback_field_type_exprs,
	uint32_t readback_field_count,
	uint32_t readback_result_type_expr,
	uint32_t parameter_context,
	uint32_t field_context,
	uint32_t result_classifier,
	uint32_t curried_classifier_cache,
	uint32_t* p_constructor_id
) {
	if (!db || !p_constructor_id || type_id >= db->type_count ||
		parameter_context == PROTOTYPE_INVALID_ID ||
		field_context == PROTOTYPE_INVALID_ID ||
		result_classifier == PROTOTYPE_INVALID_ID ||
		curried_classifier_cache == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (readback_field_count > 0 && !readback_field_type_exprs) {
		return -1;
	}
	if (reserve_slot(db->constructor_count, db->constructor_capacity) != 0) {
		return -1;
	}
	if (db->readback_field_type_count + readback_field_count > db->readback_field_type_capacity) {
		return -1;
	}

	struct prototype_type_declaration* type = &db->type_declarations[type_id];
	if ((uint32_t)db->constructor_count != type->first_constructor + type->constructor_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->constructor_count;
	struct prototype_type_constructor_declaration* constructor = &db->constructor_declarations[id];
	memset(constructor, 0, sizeof(*constructor));
	constructor->name_symbol_id = name_symbol_id;
	constructor->owner_type = type_id;
	constructor->constructor_index = type->constructor_count;
	constructor->readback.first_field_type = (uint32_t)db->readback_field_type_count;
	constructor->readback.field_count = readback_field_count;
	constructor->readback.result_type = readback_result_type_expr;
	constructor->parameter_context = parameter_context;
	constructor->field_context = field_context;
	constructor->result_classifier = result_classifier;
	constructor->curried_classifier_cache = curried_classifier_cache;

	for (uint32_t i = 0; i < readback_field_count; ++i) {
		db->readback_field_types[db->readback_field_type_count++] = readback_field_type_exprs[i];
	}

	db->constructor_count++;
	type->constructor_count++;
	db->representations_dirty = 1;
	*p_constructor_id = id;
	return 0;
}

int prototype_type_constructor_derive_curried_classifier(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t parameter_context,
	uint32_t field_context,
	uint32_t result_classifier,
	uint32_t* p_classifier
) {
	uint32_t parameter_path[64];
	uint32_t field_path[64];
	uint32_t parameter_count;
	uint32_t field_count;
	if (!terms || !contexts || !p_classifier ||
		result_classifier >= terms->term_count ||
		prototype_context_extension_path(
			contexts,
			prototype_context_empty(contexts),
			parameter_context,
			parameter_path,
			64,
			&parameter_count
		) != 0 ||
		prototype_context_extension_path(
			contexts,
			parameter_context,
			field_context,
			field_path,
			64,
			&field_count
		) != 0) {
		return -1;
	}

	uint32_t classifier = result_classifier;
	for (uint32_t i = field_count; i > 0; --i) {
		const struct prototype_context* field =
			prototype_context_get(contexts, field_path[i - 1]);
		uint32_t codomain_family;
		uint32_t pi_classifier;
		if (!field || field->classifier == PROTOTYPE_INVALID_ID ||
			prototype_term_pure_family(
				terms,
				field->binder_id,
				classifier,
				&codomain_family
			) != 0 ||
			prototype_term_pi_family(
				terms,
				field->classifier,
				codomain_family,
				&pi_classifier
			) != 0) {
			return -1;
		}
		classifier = pi_classifier;
	}
	for (uint32_t i = parameter_count; i > 0; --i) {
		const struct prototype_context* parameter =
			prototype_context_get(contexts, parameter_path[i - 1]);
		uint32_t lambda;
		if (!parameter ||
			prototype_term_lambda(
				terms, parameter->binder_id, classifier, &lambda
			) != 0) {
			return -1;
		}
		classifier = lambda;
	}
	*p_classifier = classifier;
	return 0;
}

const struct prototype_type_declaration* prototype_type_declaration_lookup(
	const struct prototype_type_declaration_db* db,
	int name_symbol_id
) {
	if (!db) {
		return NULL;
	}

	for (size_t i = 0; i < db->type_count; ++i) {
		if (type_declaration_present(&db->type_declarations[i]) &&
			db->type_declarations[i].name_symbol_id == name_symbol_id) {
			return &db->type_declarations[i];
		}
	}
	return NULL;
}

const struct prototype_type_constructor_declaration* prototype_type_declaration_lookup_constructor(
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id
) {
	if (!db || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return NULL;
	}

	const struct prototype_type_declaration* type = &db->type_declarations[type_id];
	uint32_t first = type->first_constructor;
	uint32_t end = first + type->constructor_count;
	for (uint32_t i = first; i < end; ++i) {
		if (i >= db->constructor_count ||
			!constructor_declaration_present(&db->constructor_declarations[i])) {
			return NULL;
		}
		if (db->constructor_declarations[i].name_symbol_id == name_symbol_id) {
			return &db->constructor_declarations[i];
		}
	}
	return NULL;
}

static int representation_binders_equal(
	const struct representation_compare_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	if (!env) {
		return 0;
	}
	for (uint32_t i = env->binder_count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->left_binders[index] == left_binder) {
			return env->right_binders[index] == right_binder;
		}
		if (env->right_binders[index] == right_binder) {
			return 0;
		}
	}
	return left_binder == right_binder;
}

static int representation_push_binder(
	struct representation_compare_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	if (!env || env->binder_count >= PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY) {
		return -1;
	}
	env->left_binders[env->binder_count] = left_binder;
	env->right_binders[env->binder_count] = right_binder;
	env->binder_count++;
	return 0;
}

static int representation_type_pair_lookup(
	const struct representation_compare_env* env,
	uint32_t left_type_id,
	uint32_t right_type_id
) {
	if (!env) {
		return 0;
	}
	for (uint32_t i = 0; i < env->type_count; ++i) {
		if (env->left_types[i] == left_type_id || env->right_types[i] == right_type_id) {
			return env->left_types[i] == left_type_id && env->right_types[i] == right_type_id ? 1 : -1;
		}
	}
	return 0;
}

static int representation_push_type_pair(
	struct representation_compare_env* env,
	uint32_t left_type_id,
	uint32_t right_type_id
) {
	if (!env || env->type_count >= PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY) {
		return -1;
	}
	env->left_types[env->type_count] = left_type_id;
	env->right_types[env->type_count] = right_type_id;
	env->type_count++;
	return 0;
}

static int representation_match_cases_equal_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_match_case* left,
	const struct prototype_match_case* right,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!terms || !db || !left || !right || !env ||
		left->constructor_id != right->constructor_id ||
		left->binder_count != right->binder_count ||
		left->first_binder + left->binder_count > terms->case_binder_count ||
		right->first_binder + right->binder_count > terms->case_binder_count ||
		!representation_terms_equal_at_depth(
			terms, db, left->constructor_owner, right->constructor_owner, env, depth + 1
		)) {
		return 0;
	}
	uint32_t saved = env->binder_count;
	for (uint32_t i = 0; i < left->binder_count; ++i) {
		if (representation_push_binder(
				env,
				terms->case_binders[left->first_binder + i].binder_id,
				terms->case_binders[right->first_binder + i].binder_id
			) != 0) {
			env->binder_count = saved;
			return 0;
		}
	}
	int equal = representation_terms_equal_at_depth(
		terms, db, left->body, right->body, env, depth + 1
	);
	env->binder_count = saved;
	return equal;
}

static int representation_terms_equal_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t left_term_id,
	uint32_t right_term_id,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!terms || !db || !env || left_term_id >= terms->term_count ||
		right_term_id >= terms->term_count || depth > PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY) {
		return 0;
	}
	const struct prototype_term* left = &terms->terms[left_term_id];
	const struct prototype_term* right = &terms->terms[right_term_id];
	if (left->tag != right->tag) {
		return 0;
	}
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return representation_binders_equal(env, left->as.var.binder_id, right->as.var.binder_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id == right->as.constructor.constructor_id &&
				representation_terms_equal_at_depth(
					terms, db, left->as.constructor.owner, right->as.constructor.owner, env, depth + 1
				);
		case PROTOTYPE_TERM_APP:
			return representation_terms_equal_at_depth(
				terms, db, left->as.app.function, right->as.app.function, env, depth + 1
			) && representation_terms_equal_at_depth(
				terms, db, left->as.app.argument, right->as.app.argument, env, depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_PI: {
			uint32_t left_body = PROTOTYPE_INVALID_ID;
			uint32_t right_body = PROTOTYPE_INVALID_ID;
			uint32_t left_binder = PROTOTYPE_INVALID_ID;
			uint32_t right_binder = PROTOTYPE_INVALID_ID;
			if (left->tag == PROTOTYPE_TERM_LAMBDA) {
				left_binder = left->as.lambda.binder_id;
				right_binder = right->as.lambda.binder_id;
				left_body = left->as.lambda.body;
				right_body = right->as.lambda.body;
			} else {
				if (prototype_term_pure_family_parts(
						terms,
						left->as.pi.codomain_family,
						&left_binder,
						&left_body
					) != 0 || prototype_term_pure_family_parts(
						terms,
						right->as.pi.codomain_family,
						&right_binder,
						&right_body
					) != 0) {
					return 0;
				}
			}
			uint32_t saved = env->binder_count;
			if (left->tag == PROTOTYPE_TERM_PI &&
				!representation_terms_equal_at_depth(
					terms, db, left->as.pi.domain, right->as.pi.domain, env, depth + 1
				)) {
				return 0;
			}
			if (left_binder != PROTOTYPE_INVALID_ID &&
				representation_push_binder(env, left_binder, right_binder) != 0) {
				return 0;
			}
			int equal = representation_terms_equal_at_depth(
				terms,
				db,
				left_body,
				right_body,
				env,
				depth + 1
			);
			env->binder_count = saved;
			return equal;
		}
		case PROTOTYPE_TERM_MATCH:
			if (left->as.match.case_count != right->as.match.case_count ||
				left->as.match.first_case + left->as.match.case_count > terms->case_count ||
				right->as.match.first_case + right->as.match.case_count > terms->case_count ||
				!representation_terms_equal_at_depth(
					terms, db, left->as.match.scrutinee, right->as.match.scrutinee, env, depth + 1
				)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				if (!representation_match_cases_equal_at_depth(
						terms,
						db,
						&terms->cases[left->as.match.first_case + i],
						&terms->cases[right->as.match.first_case + i],
						env,
						depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_VIEW:
			return representation_terms_equal_at_depth(
				terms, db, left->as.type_view.source, right->as.type_view.source, env, depth + 1
			);
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return representation_types_equal_at_depth(
				terms, db, left->as.type_declaration.type_id, right->as.type_declaration.type_id, env, depth + 1
			);
		case PROTOTYPE_TERM_TYPE_FORMER:
			return left->as.type_former.representation_id == right->as.type_former.representation_id;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return 1;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return left->as.external_ref.name.namespace_symbol_id == right->as.external_ref.name.namespace_symbol_id &&
				left->as.external_ref.name.name_symbol_id == right->as.external_ref.name.name_symbol_id;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			return left->as.pure_primitive.primitive_id ==
					right->as.pure_primitive.primitive_id &&
				left->as.pure_primitive.type_symbol_id == right->as.pure_primitive.type_symbol_id;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return left->as.effect_operation.operation_id == right->as.effect_operation.operation_id;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return representation_terms_equal_at_depth(
				terms, db, left->as.induction_hypothesis.argument,
				right->as.induction_hypothesis.argument, env, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_LABEL:
			return left->as.effect_label.effects == right->as.effect_label.effects;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return representation_binders_equal(
				env, left->as.effect_row_var.binder_id, right->as.effect_row_var.binder_id
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return representation_terms_equal_at_depth(
				terms, db, left->as.effect_row_union.left, right->as.effect_row_union.left, env, depth + 1
			) && representation_terms_equal_at_depth(
				terms, db, left->as.effect_row_union.right, right->as.effect_row_union.right, env, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return representation_terms_equal_at_depth(
				terms, db, left->as.computation_type.label, right->as.computation_type.label, env, depth + 1
			) && representation_terms_equal_at_depth(
				terms, db, left->as.computation_type.result, right->as.computation_type.result, env, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return representation_terms_equal_at_depth(
				terms, db, left->as.thunk_type.computation,
				right->as.thunk_type.computation, env, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return representation_terms_equal_at_depth(
				terms, db, left->as.return_term.value, right->as.return_term.value, env, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return representation_terms_equal_at_depth(
				terms, db, left->as.thunk.computation, right->as.thunk.computation, env, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return representation_terms_equal_at_depth(
				terms, db, left->as.force.value, right->as.force.value, env, depth + 1
			);
		case PROTOTYPE_TERM_DEEP_FOLD: {
			if (!representation_terms_equal_at_depth(
					terms, db, left->as.deep_fold.computation,
					right->as.deep_fold.computation, env, depth + 1
				) || !representation_terms_equal_at_depth(
					terms, db, left->as.deep_fold.return_clause,
					right->as.deep_fold.return_clause, env, depth + 1
				) || left->as.deep_fold.clause_count != right->as.deep_fold.clause_count) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.deep_fold.clause_count; ++i) {
				const struct prototype_deep_fold_clause* left_clause =
					&terms->deep_fold_clauses[left->as.deep_fold.first_clause + i];
				const struct prototype_deep_fold_clause* right_clause =
					&terms->deep_fold_clauses[right->as.deep_fold.first_clause + i];
				if (!representation_terms_equal_at_depth(
						terms, db, left_clause->operation, right_clause->operation,
						env, depth + 1
					) || !representation_terms_equal_at_depth(
						terms, db, left_clause->body, right_clause->body,
						env, depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return representation_terms_equal_at_depth(
				terms, db, left->as.operation_request.operation, right->as.operation_request.operation,
				env, depth + 1
			) && representation_terms_equal_at_depth(
				terms, db, left->as.operation_request.argument, right->as.operation_request.argument,
				env, depth + 1
			) && representation_terms_equal_at_depth(
				terms, db, left->as.operation_request.continuation, right->as.operation_request.continuation,
				env, depth + 1
			);
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
			return 1;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
		case PROTOTYPE_TERM_INT_LITERAL:
			return left->as.int_literal.value == right->as.int_literal.value;
		default:
			return 0;
	}
}

static int representation_types_equal_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t left_type_id,
	uint32_t right_type_id,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!db || !env || left_type_id >= db->type_count || right_type_id >= db->type_count ||
		depth > PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY ||
		!type_declaration_present(&db->type_declarations[left_type_id]) ||
		!type_declaration_present(&db->type_declarations[right_type_id])) {
		return 0;
	}
	int pair_status = representation_type_pair_lookup(env, left_type_id, right_type_id);
	if (pair_status > 0) {
		return 1;
	}
	if (pair_status < 0) {
		return 0;
	}
	const struct prototype_type_declaration* left = &db->type_declarations[left_type_id];
	const struct prototype_type_declaration* right = &db->type_declarations[right_type_id];
	if (left->parameter_count != right->parameter_count ||
		left->constructor_count != right->constructor_count ||
		left->parameter_count > 64 ||
		left->first_constructor + left->constructor_count > db->constructor_count ||
		right->first_constructor + right->constructor_count > db->constructor_count ||
		!env->contexts ||
		representation_push_type_pair(env, left_type_id, right_type_id) != 0) {
		return 0;
	}
	uint32_t left_parameter_path[64];
	uint32_t right_parameter_path[64];
	uint32_t left_context = left->parameter_context;
	uint32_t right_context = right->parameter_context;
	for (uint32_t i = left->parameter_count; i > 0; --i) {
		const struct prototype_context* left_entry =
			prototype_context_get(env->contexts, left_context);
		const struct prototype_context* right_entry =
			prototype_context_get(env->contexts, right_context);
		if (!left_entry || !right_entry ||
			left_entry->parent == PROTOTYPE_INVALID_ID ||
			right_entry->parent == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		left_parameter_path[i - 1] = left_context;
		right_parameter_path[i - 1] = right_context;
		left_context = left_entry->parent;
		right_context = right_entry->parent;
	}
	uint32_t saved_binders = env->binder_count;
	for (uint32_t i = 0; i < left->parameter_count; ++i) {
		const struct prototype_context* left_parameter =
			prototype_context_get(env->contexts, left_parameter_path[i]);
		const struct prototype_context* right_parameter =
			prototype_context_get(env->contexts, right_parameter_path[i]);
		if (!left_parameter || !right_parameter ||
			left_parameter->classifier == PROTOTYPE_INVALID_ID ||
			right_parameter->classifier == PROTOTYPE_INVALID_ID ||
			!representation_terms_equal_at_depth(
				terms, db, left_parameter->classifier,
				right_parameter->classifier, env, depth + 1
			) ||
			representation_push_binder(
				env, left_parameter->binder_id, right_parameter->binder_id
			) != 0) {
			env->binder_count = saved_binders;
			return 0;
		}
	}
	uint32_t parameter_binders = env->binder_count;
	for (uint32_t i = 0; i < left->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* left_constructor =
			&db->constructor_declarations[left->first_constructor + i];
		const struct prototype_type_constructor_declaration* right_constructor =
			&db->constructor_declarations[right->first_constructor + i];
		uint32_t left_fields[64];
		uint32_t right_fields[64];
		uint32_t left_field_count;
		uint32_t right_field_count;
		if (!constructor_declaration_present(left_constructor) ||
			!constructor_declaration_present(right_constructor) ||
			left_constructor->constructor_index != right_constructor->constructor_index ||
			left_constructor->parameter_context != left->parameter_context ||
			right_constructor->parameter_context != right->parameter_context ||
			prototype_context_extension_path(
				env->contexts, left_constructor->parameter_context,
				left_constructor->field_context, left_fields, 64,
				&left_field_count
			) != 0 ||
			prototype_context_extension_path(
				env->contexts, right_constructor->parameter_context,
				right_constructor->field_context, right_fields, 64,
				&right_field_count
			) != 0 ||
			left_field_count != right_field_count) {
			env->binder_count = saved_binders;
			return 0;
		}
		env->binder_count = parameter_binders;
		for (uint32_t j = 0; j < left_field_count; ++j) {
			const struct prototype_context* left_field =
				prototype_context_get(env->contexts, left_fields[j]);
			const struct prototype_context* right_field =
				prototype_context_get(env->contexts, right_fields[j]);
			if (!left_field || !right_field ||
				left_field->classifier == PROTOTYPE_INVALID_ID ||
				right_field->classifier == PROTOTYPE_INVALID_ID ||
				!representation_terms_equal_at_depth(
					terms, db, left_field->classifier,
					right_field->classifier, env, depth + 1
				) ||
				representation_push_binder(
					env, left_field->binder_id, right_field->binder_id
				) != 0) {
				env->binder_count = saved_binders;
				return 0;
			}
		}
		if (!representation_terms_equal_at_depth(
				terms, db, left_constructor->result_classifier,
				right_constructor->result_classifier, env, depth + 1
			)) {
			env->binder_count = saved_binders;
			return 0;
		}
	}
	env->binder_count = saved_binders;
	return 1;
}

static void type_code_shape_key_merge_referenced_key(
	struct prototype_type_code_shape_key* key,
	const struct prototype_type_code_shape_key* referenced
) {
	key->node_count += referenced->node_count;
	key->bound_binder_count += referenced->bound_binder_count;
	key->free_binder_count += referenced->free_binder_count;
	if (referenced->has_local_universe_reference) {
		key->has_local_universe_reference = 1;
	}
	if (referenced->has_name_reference) {
		key->has_name_reference = 1;
	}
}

static int type_code_shape_key_term_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_code_shape_key_binder_env* env,
	struct prototype_type_code_shape_key* key,
	uint64_t* p_hash,
	uint32_t depth
);

static int type_code_shape_key_type_instance_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_code_shape_key_binder_env* env,
	struct prototype_type_code_shape_key* key,
	uint64_t* p_hash,
	uint32_t depth,
	int* p_handled
) {
	if (!terms || !db || !env || !key || !p_hash || !p_handled ||
		term_id >= terms->term_count) {
		return -1;
	}
	*p_handled = 0;

	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0) {
		return 0;
	}
	if (type_id >= db->type_count || arg_count > 16) {
		return -1;
	}

	*p_handled = 1;
	key->node_count++;
	type_code_shape_key_hash_mix_tag(p_hash, 0x74797065U);
	if (type_id == self_type_id) {
		type_code_shape_key_hash_mix_tag(p_hash, 0x73656c66U);
	} else {
		struct prototype_type_code_shape_key referenced;
		if (prototype_type_declaration_code_shape_key(
				terms,
				db,
				env->contexts,
				type_id,
				&referenced
			) != 0) {
			return -1;
		}
		type_code_shape_key_hash_mix_tag(p_hash, 0x72656674U);
		type_code_shape_key_hash_mix_key(p_hash, &referenced);
		type_code_shape_key_merge_referenced_key(key, &referenced);
	}
	type_code_shape_key_hash_mix_u32(p_hash, arg_count);
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				args[i],
				env,
				key,
				p_hash,
				depth + 1
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int type_code_shape_key_match_case_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	const struct prototype_match_case* match_case,
	struct type_code_shape_key_binder_env* env,
	struct prototype_type_code_shape_key* key,
	uint64_t* p_hash,
	uint32_t depth
) {
	if (!terms || !db || !match_case || !env || !key || !p_hash ||
		depth > 256 ||
		match_case->first_binder + match_case->binder_count > terms->case_binder_count) {
		return -1;
	}
	type_code_shape_key_hash_mix_u32(p_hash, match_case->constructor_id);
	type_code_shape_key_hash_mix_u32(p_hash, match_case->binder_count);
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		type_code_shape_key_hash_mix_u32(p_hash, PROTOTYPE_INVALID_ID);
	} else if (type_code_shape_key_term_at_depth(
			terms,
			db,
			self_type_id,
			match_case->constructor_owner,
			env,
			key,
			p_hash,
			depth + 1
		) != 0) {
		return -1;
	}

	uint32_t saved_count = env->count;
	uint32_t saved_next_slot = env->next_slot;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + i];
		if (type_code_shape_key_env_push(env, binder->binder_id) != 0) {
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return -1;
		}
		key->bound_binder_count++;
	}
	int status = type_code_shape_key_term_at_depth(
		terms,
		db,
		self_type_id,
		match_case->body,
		env,
		key,
		p_hash,
		depth + 1
	);
	env->count = saved_count;
	env->next_slot = saved_next_slot;
	return status;
}

static int type_code_shape_key_term_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_code_shape_key_binder_env* env,
	struct prototype_type_code_shape_key* key,
	uint64_t* p_hash,
	uint32_t depth
) {
	if (!terms || !db || !env || !key || !p_hash ||
		term_id >= terms->term_count ||
		depth > 256) {
		return -1;
	}

	int handled = 0;
	if (type_code_shape_key_type_instance_at_depth(
			terms,
			db,
			self_type_id,
			term_id,
			env,
			key,
			p_hash,
			depth,
			&handled
		) != 0) {
		return -1;
	}
	if (handled) {
		return 0;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	key->node_count++;
	type_code_shape_key_hash_mix_tag(p_hash, (uint32_t)term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			uint32_t slot;
			if (type_code_shape_key_env_lookup(env, term->as.var.binder_id, &slot)) {
				type_code_shape_key_hash_mix_u32(p_hash, 1);
				type_code_shape_key_hash_mix_u32(p_hash, slot);
			} else {
				type_code_shape_key_hash_mix_u32(p_hash, 0);
				type_code_shape_key_hash_mix_u32(p_hash, term->as.var.binder_id);
				key->free_binder_count++;
			}
			return 0;
		}
		case PROTOTYPE_TERM_CONSTRUCTOR:
			type_code_shape_key_hash_mix_u32(p_hash, term->as.constructor.constructor_id);
			return type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.constructor.owner,
				env,
				key,
				p_hash,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			if (type_code_shape_key_term_at_depth(
					terms,
					db,
					self_type_id,
					term->as.app.function,
					env,
					key,
					p_hash,
					depth + 1
				) != 0) {
				return -1;
			}
			return type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.app.argument,
				env,
				key,
				p_hash,
				depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (type_code_shape_key_env_push(env, term->as.lambda.binder_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.lambda.body,
				env,
				key,
				p_hash,
				depth + 1
			);
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return status;
		}
		case PROTOTYPE_TERM_PI:
		{
			uint32_t binder_id;
			uint32_t body;
			if (type_code_shape_key_term_at_depth(
					terms,
					db,
					self_type_id,
					term->as.pi.domain,
					env,
					key,
					p_hash,
					depth + 1
				) != 0) {
				return -1;
			}
			if (prototype_term_pure_family_parts(
					terms,
					term->as.pi.codomain_family,
					&binder_id,
					&body
				) != 0) {
				return -1;
			}
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (type_code_shape_key_env_push(env, binder_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				body,
				env,
				key,
				p_hash,
				depth + 1
			);
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return status;
		}
		case PROTOTYPE_TERM_EFFECT_LABEL:
			type_code_shape_key_hash_mix_u32(p_hash, term->as.effect_label.effects);
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			{
				uint32_t slot;
				if (type_code_shape_key_env_lookup(
						env, term->as.effect_row_var.binder_id, &slot
					)) {
					type_code_shape_key_hash_mix_u32(p_hash, slot);
				} else {
					type_code_shape_key_hash_mix_u32(p_hash, term->as.effect_row_var.binder_id);
				}
			}
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			if (type_code_shape_key_term_at_depth(
					terms, db, self_type_id, term->as.effect_row_union.left,
					env, key, p_hash, depth + 1
				) != 0) {
				return -1;
			}
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.effect_row_union.right,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			if (type_code_shape_key_term_at_depth(
					terms, db, self_type_id, term->as.computation_type.label,
					env, key, p_hash, depth + 1
				) != 0) {
				return -1;
			}
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.computation_type.result,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.thunk_type.computation,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.return_term.value,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.thunk.computation,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return type_code_shape_key_term_at_depth(
				terms, db, self_type_id, term->as.force.value,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			if (term->as.match.first_case + term->as.match.case_count > terms->case_count) {
				return -1;
			}
			type_code_shape_key_hash_mix_u32(p_hash, term->as.match.case_count);
			if (type_code_shape_key_term_at_depth(
					terms,
					db,
					self_type_id,
					term->as.match.scrutinee,
					env,
					key,
					p_hash,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				if (type_code_shape_key_match_case_at_depth(
						terms,
						db,
						self_type_id,
						match_case,
						env,
						key,
						p_hash,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			type_code_shape_key_hash_mix_u32(p_hash, term->as.induction_hypothesis.frame_id);
			return type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.induction_hypothesis.argument,
				env,
				key,
				p_hash,
				depth + 1
			);
		case PROTOTYPE_TERM_UNIVERSE_VAR: {
			uint32_t slot;
			key->has_local_universe_reference = 1;
			if (type_code_shape_key_level_env_slot(
					env,
					term->as.universe_var.level_var,
					&slot
				) != 0) {
				return -1;
			}
			type_code_shape_key_hash_mix_u32(p_hash, slot);
			return 0;
		}
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.text_literal.text_symbol_id);
				return 0;
				case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.int_literal.value);
				type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)((uint64_t)term->as.int_literal.value >> 32));
				return 0;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				key->has_name_reference = 1;
				type_code_shape_key_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.namespace_symbol_id
				);
				type_code_shape_key_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.name_symbol_id
				);
				return 0;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			type_code_shape_key_hash_mix_u32(
				p_hash, (uint32_t)term->as.pure_primitive.primitive_id
			);
			type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.pure_primitive.type_symbol_id);
			return 0;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			type_code_shape_key_hash_mix_u32(p_hash,
				(uint32_t)term->as.effect_operation.operation_id);
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (term->as.type_declaration.type_id == self_type_id) {
				type_code_shape_key_hash_mix_tag(p_hash, 0x73656c66U);
				return 0;
			}
			if (term->as.type_declaration.type_id >= db->type_count) {
				return -1;
			}
			{
				struct prototype_type_code_shape_key referenced;
				if (prototype_type_declaration_code_shape_key(
						terms,
						db,
						env->contexts,
						term->as.type_declaration.type_id,
						&referenced
					) != 0) {
					return -1;
				}
				type_code_shape_key_hash_mix_key(p_hash, &referenced);
				type_code_shape_key_merge_referenced_key(key, &referenced);
				return 0;
			}
		case PROTOTYPE_TERM_TYPE_FORMER:
		case PROTOTYPE_TERM_TYPE_VIEW:
			return -1;
		default:
			return -1;
	}
}

int prototype_type_declaration_code_shape_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t type_id,
	struct prototype_type_code_shape_key* p_key
) {
	if (!terms || !db || !contexts || !p_key || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}

	const struct prototype_type_declaration* type = &db->type_declarations[type_id];
	if (type->first_parameter + type->parameter_count > db->parameter_count ||
		type->first_constructor + type->constructor_count > db->constructor_count ||
		type->parameter_count > 64 ||
		!prototype_context_get(contexts, type->parameter_context)) {
		return -1;
	}
	struct type_code_shape_key_binder_env env;
	uint64_t hash = PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	env.contexts = contexts;
	memset(p_key, 0, sizeof(*p_key));
	p_key->parameter_count = type->parameter_count;
	p_key->constructor_count = type->constructor_count;

	uint32_t parameter_path[64];
	uint32_t parameter_context = type->parameter_context;
	for (uint32_t i = type->parameter_count; i > 0; --i) {
		const struct prototype_context* entry =
			prototype_context_get(contexts, parameter_context);
		if (!entry || entry->parent == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		parameter_path[i - 1] = parameter_context;
		parameter_context = entry->parent;
	}
	type_code_shape_key_hash_mix_u32(&hash, type->parameter_count);
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		const struct prototype_context* parameter =
			prototype_context_get(contexts, parameter_path[i]);
		if (!parameter || parameter->classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		type_code_shape_key_hash_mix_tag(&hash, 0x7061726dU);
		if (type_code_shape_key_term_at_depth(
				terms,
				db,
				type_id,
				parameter->classifier,
				&env,
				p_key,
				&hash,
				0
			) != 0 ||
			type_code_shape_key_env_push(&env, parameter->binder_id) != 0) {
			return -1;
		}
		p_key->bound_binder_count++;
	}

	uint32_t parameter_binder_count = env.count;
	type_code_shape_key_hash_mix_u32(&hash, type->constructor_count);
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&db->constructor_declarations[type->first_constructor + i];
		uint32_t field_path[64];
		uint32_t field_count;
		if (!constructor_declaration_present(constructor) ||
			constructor->parameter_context != type->parameter_context ||
			prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				field_path,
				64,
				&field_count
			) != 0 ||
			constructor->result_classifier >= terms->term_count) {
			return -1;
		}
		env.count = parameter_binder_count;
		type_code_shape_key_hash_mix_tag(&hash, 0x636f6e73U);
		type_code_shape_key_hash_mix_u32(&hash, constructor->constructor_index);
		type_code_shape_key_hash_mix_u32(&hash, field_count);
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field =
				prototype_context_get(contexts, field_path[j]);
			if (!field || field->classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			type_code_shape_key_hash_mix_tag(&hash, 0x6669656cU);
			if (type_code_shape_key_term_at_depth(
					terms,
					db,
					type_id,
					field->classifier,
					&env,
					p_key,
					&hash,
					0
				) != 0 ||
				type_code_shape_key_env_push(&env, field->binder_id) != 0) {
				return -1;
			}
			p_key->bound_binder_count++;
		}
		type_code_shape_key_hash_mix_tag(&hash, 0x72657375U);
		if (
			type_code_shape_key_term_at_depth(
				terms,
				db,
				type_id,
				constructor->result_classifier,
				&env,
				p_key,
				&hash,
				0
			) != 0) {
			return -1;
		}
	}

	p_key->hash = hash;
	return 0;
}

int prototype_type_code_shape_keys_equal(
	const struct prototype_type_code_shape_key* left,
	const struct prototype_type_code_shape_key* right
) {
	return left && right &&
		left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->parameter_count == right->parameter_count &&
		left->constructor_count == right->constructor_count &&
		left->bound_binder_count == right->bound_binder_count &&
		left->free_binder_count == right->free_binder_count &&
		left->has_local_universe_reference == right->has_local_universe_reference &&
		left->has_name_reference == right->has_name_reference;
}

int prototype_type_declaration_representation_anchor_type_id(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t* p_anchor_type_id
) {
	if (!terms || !db || !p_anchor_type_id || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}
	uint32_t representation_id = db->type_declarations[type_id].representation_id;
	if (representation_id == PROTOTYPE_INVALID_ID ||
		representation_id >= db->representation_count) {
		return -1;
	}
	*p_anchor_type_id = db->representations[representation_id].representative_type_id;
	return *p_anchor_type_id < db->type_count ? 0 : -1;
}

int prototype_type_declaration_intern_representation(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t* p_representation_id
) {
	if (!terms || !db || !p_representation_id || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id]) ||
		!db->representations) {
		return -1;
	}
	if (db->representations_dirty) {
		/* Type declarations are still being assembled. Use a declaration anchor
		 * until the graph-finalization pass interns exact representations. */
		*p_representation_id = type_id;
		return 0;
	}
	uint32_t representation_id = db->type_declarations[type_id].representation_id;
	if (representation_id == PROTOTYPE_INVALID_ID || representation_id >= db->representation_count) {
		return -1;
	}
	*p_representation_id = representation_id;
	return 0;
}

int prototype_type_declaration_representation_type_id(
	const struct prototype_type_declaration_db* db,
	uint32_t representation_id,
	uint32_t* p_type_id
) {
	if (!db || !p_type_id || representation_id >= db->representation_count ||
		db->representations[representation_id].representative_type_id >= db->type_count) {
		return -1;
	}
	*p_type_id = db->representations[representation_id].representative_type_id;
	return 0;
}

int prototype_type_declaration_rebuild_representations(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts
) {
	if (!terms || !db || !contexts || !db->representations ||
		db->representation_capacity < db->type_count) {
		return -1;
	}
	db->representation_count = 0;
	for (uint32_t type_id = 0; type_id < db->type_count; ++type_id) {
		if (!type_declaration_present(&db->type_declarations[type_id])) {
			continue;
		}
		struct prototype_type_code_shape_key fingerprint;
		if (prototype_type_declaration_code_shape_key(
				terms, db, contexts, type_id, &fingerprint
			) != 0) {
			return -1;
		}
		uint32_t representation_id = PROTOTYPE_INVALID_ID;
		for (uint32_t candidate_id = 0; candidate_id < db->representation_count; ++candidate_id) {
			const struct prototype_type_representation* candidate =
				&db->representations[candidate_id];
			if (!prototype_type_code_shape_keys_equal(&fingerprint, &candidate->fingerprint)) {
				continue;
			}
			struct representation_compare_env env;
			memset(&env, 0, sizeof(env));
			env.contexts = contexts;
			if (representation_types_equal_at_depth(
					terms,
					db,
					type_id,
					candidate->representative_type_id,
					&env,
					0
				)) {
				representation_id = candidate_id;
				break;
			}
		}
		if (representation_id == PROTOTYPE_INVALID_ID) {
			if (db->representation_count >= db->representation_capacity) {
				return -1;
			}
			representation_id = (uint32_t)db->representation_count;
			db->representations[representation_id].representative_type_id = type_id;
			db->representations[representation_id].fingerprint = fingerprint;
			db->representation_count++;
		}
		db->type_declarations[type_id].representation_id = representation_id;
	}
	db->representations_dirty = 0;
	return 0;
}
