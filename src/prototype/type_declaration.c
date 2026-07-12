#include "type_declaration.h"
#include "term.h"

#include <string.h>

#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY 512
#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_OFFSET 1469598103934665603ULL
#define PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_PRIME 1099511628211ULL

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

static int type_declaration_present(const struct prototype_type_declaration* type) {
	return type && type->type_index != PROTOTYPE_INVALID_ID;
}

static int parameter_declaration_present(
	const struct prototype_type_parameter_declaration* parameter
) {
	return parameter && parameter->binder_id != PROTOTYPE_INVALID_ID;
}

static int constructor_declaration_present(
	const struct prototype_type_constructor_declaration* constructor
) {
	return constructor && constructor->owner_type != PROTOTYPE_INVALID_ID;
}

static int type_expr_present(const struct prototype_type_expr* expr) {
	return expr && expr->tag != 0;
}

struct type_code_shape_key_binder_env {
	uint32_t binder_id[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t slot[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
	uint32_t level_var[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t level_slot[PROTOTYPE_TYPE_CODE_SHAPE_KEY_BINDER_CAPACITY];
	uint32_t level_count;
	uint32_t next_level_slot;
};

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
	type->type_index = id;
	type->first_parameter = (uint32_t)db->parameter_count;
	type->first_constructor = (uint32_t)db->constructor_count;

	db->type_count++;
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
	return 0;
}

int prototype_type_declaration_add_constructor(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id,
	const uint32_t* readback_field_type_exprs,
	uint32_t readback_field_count,
	uint32_t readback_result_type_expr,
	uint32_t classifier_family,
	uint32_t* p_constructor_id
) {
	if (!db || !p_constructor_id || type_id >= db->type_count ||
		classifier_family == PROTOTYPE_INVALID_ID) {
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
	constructor->classifier_family = classifier_family;

	for (uint32_t i = 0; i < readback_field_count; ++i) {
		db->readback_field_types[db->readback_field_type_count++] = readback_field_type_exprs[i];
	}

	db->constructor_count++;
	type->constructor_count++;
	*p_constructor_id = id;
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

static int type_expr_code_shape_key_at_depth(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id,
	struct type_code_shape_key_binder_env* env,
	struct prototype_type_code_shape_key* key,
	uint64_t* p_hash,
	uint32_t depth
) {
	if (!db || !env || !key || !p_hash ||
		expr_id >= db->expr_count ||
		depth > 256) {
		return -1;
	}

	const struct prototype_type_expr* expr = &db->exprs[expr_id];
	if (!type_expr_present(expr)) {
		return -1;
	}
	key->node_count++;
	type_code_shape_key_hash_mix_tag(p_hash, (uint32_t)expr->tag);
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			type_code_shape_key_hash_mix_u32(p_hash, expr->as.universe.level);
			return 0;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
		{
			uint32_t slot;
			key->has_local_universe_reference = 1;
			if (type_code_shape_key_level_env_slot(env, expr->as.universe_var.level_var, &slot) != 0) {
				return -1;
			}
			type_code_shape_key_hash_mix_u32(p_hash, slot);
			return 0;
		}
		case PROTOTYPE_TYPE_EXPR_SELF:
			return 0;
		case PROTOTYPE_TYPE_EXPR_VAR: {
			uint32_t slot;
			if (type_code_shape_key_env_lookup(env, expr->as.var.binder_id, &slot)) {
				type_code_shape_key_hash_mix_u32(p_hash, 1);
				type_code_shape_key_hash_mix_u32(p_hash, slot);
			} else {
				type_code_shape_key_hash_mix_u32(p_hash, 0);
				type_code_shape_key_hash_mix_u32(p_hash, expr->as.var.binder_id);
				key->free_binder_count++;
			}
			return 0;
		}
			case PROTOTYPE_TYPE_EXPR_NAME:
				key->has_name_reference = 1;
				type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)expr->as.name.symbol_id);
				return 0;
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
				type_code_shape_key_hash_mix_key(p_hash, &expr->as.imported_type.code_shape_key);
			key->node_count += expr->as.imported_type.code_shape_key.node_count;
			key->bound_binder_count += expr->as.imported_type.code_shape_key.bound_binder_count;
			key->free_binder_count += expr->as.imported_type.code_shape_key.free_binder_count;
			if (expr->as.imported_type.code_shape_key.has_local_universe_reference) {
				key->has_local_universe_reference = 1;
			}
				if (expr->as.imported_type.code_shape_key.has_name_reference) {
					key->has_name_reference = 1;
				}
				return 0;
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				key->has_name_reference = 1;
				type_code_shape_key_hash_mix_u32(
					p_hash,
					(uint32_t)expr->as.external_term.name.namespace_symbol_id
				);
				type_code_shape_key_hash_mix_u32(
					p_hash,
					(uint32_t)expr->as.external_term.name.name_symbol_id
				);
				return 0;
			case PROTOTYPE_TYPE_EXPR_APP:
			if (type_expr_code_shape_key_at_depth(
					db,
					expr->as.app.function,
					env,
					key,
					p_hash,
					depth + 1
				) != 0) {
				return -1;
			}
			return type_expr_code_shape_key_at_depth(
				db,
				expr->as.app.argument,
				env,
				key,
				p_hash,
				depth + 1
			);
		case PROTOTYPE_TYPE_EXPR_ARROW:
			if (type_expr_code_shape_key_at_depth(
					db,
					expr->as.arrow.domain,
					env,
					key,
					p_hash,
					depth + 1
				) != 0) {
				return -1;
			}
			return type_expr_code_shape_key_at_depth(
				db,
				expr->as.arrow.codomain,
				env,
				key,
				p_hash,
				depth + 1
			);
		default:
			return -1;
	}
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
			return type_code_shape_key_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.pi.codomain_family,
				env,
				key,
				p_hash,
				depth + 1
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
		case PROTOTYPE_TERM_INTRINSIC:
			type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.intrinsic_id);
			type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.symbol_id);
			type_code_shape_key_hash_mix_u32(p_hash, (uint32_t)term->as.intrinsic.type_symbol_id);
			return 0;
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
	uint32_t type_id,
	struct prototype_type_code_shape_key* p_key
) {
	if (!terms || !db || !p_key || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}

	const struct prototype_type_declaration* type = &db->type_declarations[type_id];
	if (type->first_parameter + type->parameter_count > db->parameter_count ||
		type->first_constructor + type->constructor_count > db->constructor_count) {
		return -1;
	}
	struct type_code_shape_key_binder_env env;
	uint64_t hash = PROTOTYPE_TYPE_CODE_SHAPE_KEY_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	memset(p_key, 0, sizeof(*p_key));
	p_key->parameter_count = type->parameter_count;
	p_key->constructor_count = type->constructor_count;

	type_code_shape_key_hash_mix_u32(&hash, type->parameter_count);
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&db->parameter_declarations[type->first_parameter + i];
		if (!parameter_declaration_present(parameter)) {
			return -1;
		}
		type_code_shape_key_hash_mix_tag(&hash, 0x7061726dU);
		if (type_code_shape_key_env_push(&env, parameter->binder_id) != 0) {
			return -1;
		}
		p_key->bound_binder_count++;
		if (type_expr_code_shape_key_at_depth(
				db,
				parameter->type_expr,
				&env,
				p_key,
				&hash,
				0
			) != 0) {
			return -1;
		}
	}

	type_code_shape_key_hash_mix_u32(&hash, type->constructor_count);
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&db->constructor_declarations[type->first_constructor + i];
		if (!constructor_declaration_present(constructor)) {
			return -1;
		}
		type_code_shape_key_hash_mix_tag(&hash, 0x636f6e73U);
		type_code_shape_key_hash_mix_u32(&hash, constructor->constructor_index);
			if (constructor->classifier_family == PROTOTYPE_INVALID_ID ||
			constructor->classifier_family >= terms->term_count ||
			type_code_shape_key_term_at_depth(
				terms,
				db,
				type_id,
				constructor->classifier_family,
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

int prototype_type_declaration_core_shape_representative(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t* p_core_type_id
) {
	if (!terms || !db || !p_core_type_id || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}

	struct prototype_type_code_shape_key key;
	if (prototype_type_declaration_code_shape_key(terms, db, type_id, &key) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i <= type_id; ++i) {
		struct prototype_type_code_shape_key candidate;
		if (!type_declaration_present(&db->type_declarations[i])) {
			continue;
		}
		if (prototype_type_declaration_code_shape_key(terms, db, i, &candidate) != 0) {
			return -1;
		}
		if (prototype_type_code_shape_keys_equal(&candidate, &key)) {
			*p_core_type_id = i;
			return 0;
		}
	}
	return -1;
}

int prototype_type_declaration_find_by_code_shape_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_type_code_shape_key* key,
	uint32_t* p_type_id
) {
	if (!terms || !db || !key || !p_type_id) {
		return -1;
	}
	for (uint32_t i = 0; i < db->type_count; ++i) {
		struct prototype_type_code_shape_key candidate;
		if (!type_declaration_present(&db->type_declarations[i])) {
			continue;
		}
		if (prototype_type_declaration_code_shape_key(terms, db, i, &candidate) != 0) {
			return -1;
		}
		if (prototype_type_code_shape_keys_equal(&candidate, key)) {
			*p_type_id = i;
			return 0;
		}
	}
	return 1;
}
