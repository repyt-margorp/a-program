#include "a_program/kernel/type_declaration.h"
#include "a_program/kernel/context.h"
#include "a_program/core/term.h"

#include <string.h>

#define PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY 512
#define PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY 512
#define PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_HASH_OFFSET 1469598103934665603ULL
#define PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_HASH_PRIME 1099511628211ULL

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

int prototype_constructor_telescopes_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
) {
	if (!type_declarations || !contexts || !terms) {
		return -1;
	}
	if (prototype_type_declaration_origins_validate(
			type_declarations, terms
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (type->type_index != PROTOTYPE_INVALID_ID) {
			uint32_t parameter_path[64];
			uint32_t index_path[64];
			uint32_t parameter_count;
			uint32_t index_count;
			if (type->parameter_count > 64 || type->index_count > 64 ||
				prototype_context_extension_path(
					contexts,
					prototype_context_empty(contexts),
					type->parameter_context,
					parameter_path,
					64,
					&parameter_count
				) != 0 || parameter_count != type->parameter_count ||
				prototype_context_extension_path(
					contexts,
					type->parameter_context,
					type->index_context,
					index_path,
					64,
					&index_count
				) != 0 || index_count != type->index_count) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_context* parameter_context =
			prototype_context_get(contexts, constructor->parameter_context);
		const struct prototype_context* field_context =
			prototype_context_get(contexts, constructor->field_context);
		if (!parameter_context || !field_context ||
			constructor->owner_type >= type_declarations->type_count ||
			type_declarations->type_declarations[
				constructor->owner_type
			].parameter_context != constructor->parameter_context ||
			constructor->result_classifier >= terms->term_count ||
			field_context->depth < parameter_context->depth) {
			return -1;
		}
		uint32_t cursor = constructor->field_context;
		while (cursor != constructor->parameter_context) {
			const struct prototype_context* context =
				prototype_context_get(contexts, cursor);
			if (!context || cursor == prototype_context_empty(contexts)) {
				return -1;
			}
			cursor = context->parent;
		}
	}
	return 0;
}

int prototype_constructor_curried_caches_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
) {
	if (!type_declarations || !contexts || !terms ||
		prototype_constructor_telescopes_validate(
			type_declarations, contexts, terms
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t derived_classifier;
		if (prototype_type_constructor_derive_curried_classifier(
				terms,
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				constructor->result_classifier,
				&derived_classifier
			) != 0 ||
			derived_classifier != constructor->curried_classifier_cache) {
			return -1;
		}
	}
	return 0;
}

int prototype_constructor_curried_caches_rebuild(
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
) {
	if (!type_declarations || !contexts || !terms ||
		prototype_constructor_telescopes_validate(
			type_declarations, contexts, terms
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_type_constructor_derive_curried_classifier(
				terms,
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				constructor->result_classifier,
				&constructor->curried_classifier_cache
			) != 0) {
			return -1;
		}
	}
	return 0;
}

struct type_representation_fingerprint_binder_env {
	const struct prototype_context_db* contexts;
	uint32_t binding_id[PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY];
	uint32_t slot[PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
	uint32_t level_var[PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY];
	uint32_t level_slot[PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY];
	uint32_t level_count;
	uint32_t next_level_slot;
};

struct representation_compare_env {
	const struct prototype_context_db* left_contexts;
	const struct prototype_context_db* right_contexts;
	uint32_t left_binders[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t right_binders[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t binder_count;
	uint32_t left_types[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t right_types[PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY];
	uint32_t type_count;
};

static int representation_types_equal_at_depth(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	uint32_t left_type_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	uint32_t right_type_id,
	struct representation_compare_env* env,
	uint32_t depth
);

static int representation_terms_equal_at_depth(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	uint32_t left_term_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	uint32_t right_term_id,
	struct representation_compare_env* env,
	uint32_t depth
);

static void type_representation_fingerprint_hash_mix_u32(uint64_t* p_hash, uint32_t value) {
	*p_hash ^= (uint64_t)value;
	*p_hash *= PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_HASH_PRIME;
}

static void type_representation_fingerprint_hash_mix_tag(uint64_t* p_hash, uint32_t tag) {
	type_representation_fingerprint_hash_mix_u32(p_hash, 0x9e3779b9U);
	type_representation_fingerprint_hash_mix_u32(p_hash, tag);
}

static void type_representation_fingerprint_hash_mix_key(
	uint64_t* p_hash,
	const struct prototype_type_representation_fingerprint* key
) {
	type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)key->hash);
	type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)(key->hash >> 32));
	type_representation_fingerprint_hash_mix_u32(p_hash, key->node_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, key->parameter_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, key->index_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, key->constructor_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, key->bound_binder_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, key->free_binder_count);
	type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)key->has_local_universe_reference);
	type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)key->has_name_reference);
}

static int type_representation_fingerprint_env_lookup(
	const struct type_representation_fingerprint_binder_env* env,
	uint32_t binding_id,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->binding_id[index] == binding_id) {
			*p_slot = env->slot[index];
			return 1;
		}
	}
	return 0;
}

static int type_representation_fingerprint_env_push(
	struct type_representation_fingerprint_binder_env* env,
	uint32_t binding_id
) {
	if (!env || env->count >= PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY) {
		return -1;
	}
	env->binding_id[env->count] = binding_id;
	env->slot[env->count] = env->next_slot++;
	env->count++;
	return 0;
}

static int type_representation_fingerprint_level_env_lookup(
	const struct type_representation_fingerprint_binder_env* env,
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

static int type_representation_fingerprint_level_env_slot(
	struct type_representation_fingerprint_binder_env* env,
	uint32_t level_var,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return -1;
	}
	if (type_representation_fingerprint_level_env_lookup(env, level_var, p_slot)) {
		return 0;
	}
	if (env->level_count >= PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_BINDER_CAPACITY) {
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
	size_t expr_capacity,
	struct prototype_type_representation* representations,
	size_t representation_capacity
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
	db->representations = representations;
	db->representation_capacity = representation_capacity;
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

int prototype_type_expr_var(struct prototype_type_declaration_db* db, uint32_t binding_id, int symbol_id, uint32_t* p_ret) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_VAR;
	expr.as.var.binding_id = binding_id;
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
	uint32_t binding_id,
	int symbol_id,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
) {
	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_PI;
	expr.as.pi.binding_id = binding_id;
	expr.as.pi.symbol_id = symbol_id;
	expr.as.pi.domain = domain;
	expr.as.pi.codomain = codomain;
	return add_expr(db, expr, p_ret);
}

int prototype_type_expr_imported_type(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	const struct prototype_type_representation_fingerprint* key,
	uint32_t* p_ret
) {
	if (!key) {
		return -1;
	}

	struct prototype_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE;
	expr.as.imported_type.name = name;
	expr.as.imported_type.representation_fingerprint = *key;
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
	type->index_context = PROTOTYPE_INVALID_ID;
	type->first_constructor = (uint32_t)db->constructor_count;
	type->origin_kind = PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE;
	type->origin_source_carrier_term_id = PROTOTYPE_INVALID_ID;

	db->type_count++;
	db->representations_dirty = 1;
	*p_type_id = id;
	return 0;
}

int prototype_type_declaration_add_generated_identity(
	struct prototype_type_declaration_db* db,
	uint32_t source_carrier_term_id,
	uint32_t parameter_context_id,
	uint32_t* p_type_id
) {
	if (!db || !p_type_id || source_carrier_term_id == PROTOTYPE_INVALID_ID ||
		parameter_context_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < db->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&db->type_declarations[i];
		if (type_declaration_present(type) && type->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			type->origin_source_carrier_term_id == source_carrier_term_id &&
			type->parameter_context == parameter_context_id) {
			*p_type_id = i;
			return 0;
		}
	}
	uint32_t type_id;
	if (prototype_type_declaration_add(db, -1, &type_id) != 0) {
		return -1;
	}
	db->type_declarations[type_id].origin_kind =
		PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY;
	db->type_declarations[type_id].origin_source_carrier_term_id =
		source_carrier_term_id;
	db->type_declarations[type_id].parameter_context = parameter_context_id;
	*p_type_id = type_id;
	return 0;
}

int prototype_type_declaration_origins_validate(
	const struct prototype_type_declaration_db* db,
	const struct prototype_term_db* terms
) {
	if (!db || !terms) {
		return -1;
	}
	for (uint32_t i = 0; i < db->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&db->type_declarations[i];
		if (!type_declaration_present(type)) {
			continue;
		}
		switch (type->origin_kind) {
		case PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE:
			if (type->name_symbol_id < 0) {
				return -1;
			}
			break;
		case PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY:
			if (type->name_symbol_id != -1 ||
				type->namespace_symbol_id != -1 ||
				type->origin_source_carrier_term_id >= terms->term_count) {
				return -1;
			}
			break;
		default:
			return -1;
		}
	}
	return 0;
}

int prototype_type_declaration_find_generated_identity(
	const struct prototype_type_declaration_db* db,
	uint32_t source_carrier_term_id,
	uint32_t parameter_context_id,
	uint32_t* p_type_id
) {
	if (!db || !p_type_id || source_carrier_term_id == PROTOTYPE_INVALID_ID ||
		parameter_context_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < db->type_count; ++i) {
		const struct prototype_type_declaration* type = &db->type_declarations[i];
		if (type_declaration_present(type) && type->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			type->origin_source_carrier_term_id == source_carrier_term_id &&
			type->parameter_context == parameter_context_id) {
			*p_type_id = i;
			return 0;
		}
	}
	return 1;
}

static int term_is_binding_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t binding_id
);

static int term_app_spine_parts(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
) {
	if (!terms || !p_head || !arguments || !p_argument_count ||
		term_id >= terms->term_count) {
		return -1;
	}
	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = term_id;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16 || count >= argument_capacity) {
			return -1;
		}
		reversed[count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_head = current;
	*p_argument_count = count;
	return 0;
}

static int contextual_field_identity_classifier_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t identity_classifier,
	uint32_t left_classifier,
	uint32_t right_classifier,
	uint32_t left_endpoint,
	uint32_t right_endpoint
) {
	uint32_t head;
	uint32_t arguments[5];
	uint32_t argument_count;
	if (term_app_spine_parts(
			terms,
			identity_classifier,
			&head,
			arguments,
			5,
			&argument_count
		) != 0 || argument_count != 5 || head >= terms->term_count ||
		arguments[2] >= terms->term_count ||
		arguments[3] != left_endpoint || arguments[4] != right_endpoint) {
		return 0;
	}
	struct prototype_term_conversion_result left = { 0 };
	struct prototype_term_conversion_result right = { 0 };
	if (!db || prototype_term_compare_for_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			arguments[0],
			left_classifier,
			UINT64_MAX,
			&left
		) != 0 || left.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		prototype_term_compare_for_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			arguments[1],
			right_classifier,
			UINT64_MAX,
			&right
		) != 0 || right.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	/* The third argument is the object action on the dependent field type.
	 * Its formation Claim is validated by the HOTT bridge certificate; this
	 * structural validator checks that the constructor consumes that action
	 * at the matching fibers and endpoints. */
	return 1;
}

static int generated_adt_identity_constructor_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	const struct prototype_type_constructor_declaration* generated_constructor,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id
) {
	if (!terms || !db || !contexts || !source_type || !generated_constructor ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			db->constructor_count || generated_constructor->owner_type !=
			generated_type_id) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&db->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	if (source_constructor->owner_type != source_type->type_index ||
		source_constructor->constructor_index != constructor_ordinal ||
		generated_constructor->constructor_index >=
			db->type_declarations[generated_type_id].constructor_count) {
		return 0;
	}
	uint32_t source_fields[64];
	uint32_t generated_fields[192];
	uint32_t source_field_count;
	uint32_t generated_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0 || prototype_context_extension_path(
			contexts,
			generated_constructor->parameter_context,
			generated_constructor->field_context,
			generated_fields,
			192,
			&generated_field_count
		) != 0 || generated_field_count != source_field_count * 3) {
		return 0;
	}
	uint32_t result_type_id;
	uint32_t result_arguments[2];
	uint32_t result_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			generated_constructor->result_classifier,
			&result_type_id,
			result_arguments,
			&result_argument_count
		) != 0 || result_type_id != generated_type_id ||
		result_argument_count != 2) {
		return 0;
	}
	uint32_t left_head;
	uint32_t right_head;
	uint32_t left_owner;
	uint32_t right_owner;
	uint32_t left_ordinal;
	uint32_t right_ordinal;
	uint32_t left_arguments[64];
	uint32_t right_arguments[64];
	uint32_t left_argument_count;
	uint32_t right_argument_count;
	if (prototype_term_constructor_spine_info(
			terms, result_arguments[0], &left_head, &left_owner, &left_ordinal,
			left_arguments, 64, &left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms, result_arguments[1], &right_head, &right_owner, &right_ordinal,
			right_arguments, 64, &right_argument_count
		) != 0 || left_owner != source_carrier || right_owner != source_carrier ||
		left_ordinal != constructor_ordinal || right_ordinal != constructor_ordinal ||
		left_argument_count != source_field_count ||
		right_argument_count != source_field_count) {
		return 0;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* source_field =
			prototype_context_get(contexts, source_fields[i]);
		const struct prototype_context* left_field =
			prototype_context_get(contexts, generated_fields[i * 3]);
		const struct prototype_context* right_field =
			prototype_context_get(contexts, generated_fields[i * 3 + 1]);
		const struct prototype_context* identity_field =
			prototype_context_get(contexts, generated_fields[i * 3 + 2]);
		uint32_t field_identity_type_id = generated_type_id;
		uint32_t identity_type_id;
		uint32_t identity_arguments[2];
		uint32_t identity_argument_count;
		if (!source_field || !left_field || !right_field || !identity_field ||
			!term_is_binding_var(
				terms, left_arguments[i], left_field->binding_id
			) || !term_is_binding_var(
				terms, right_arguments[i], right_field->binding_id
			)) {
			return 0;
		}
		uint32_t field_classifier = prototype_context_classifier_term(source_field);
		uint32_t left_classifier = prototype_context_classifier_term(left_field);
		uint32_t right_classifier = prototype_context_classifier_term(right_field);
		uint32_t source_binders[64];
		uint32_t left_binders[64];
		uint32_t right_binders[64];
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_context* previous_source =
				prototype_context_get(contexts, source_fields[j]);
			const struct prototype_context* previous_left =
				prototype_context_get(contexts, generated_fields[j * 3]);
			const struct prototype_context* previous_right =
				prototype_context_get(contexts, generated_fields[j * 3 + 1]);
			if (!previous_source || !previous_left || !previous_right) {
				return 0;
			}
			source_binders[j] = previous_source->binding_id;
			left_binders[j] = previous_left->binding_id;
			right_binders[j] = previous_right->binding_id;
		}
		int left_equal;
		int right_equal;
		if (prototype_term_core_shape_equal_under_binders(
				terms,
				source_binders,
				left_binders,
				i,
				field_classifier,
				left_classifier,
				&left_equal
			) != 0 || prototype_term_core_shape_equal_under_binders(
				terms,
				source_binders,
				right_binders,
				i,
				field_classifier,
				right_classifier,
				&right_equal
			) != 0 || !left_equal || !right_equal) {
			return 0;
		}
		int dependent = 0;
		for (uint32_t j = 0; j < i; ++j) {
			if (prototype_term_contains_free_binding(
					terms, field_classifier, source_binders[j]
				)) {
				dependent = 1;
				break;
			}
		}
		if (!dependent && field_classifier != source_carrier &&
			prototype_type_declaration_find_generated_identity(
				db,
				field_classifier,
				db->type_declarations[generated_type_id].parameter_context,
				&field_identity_type_id
			) != 0) {
			return 0;
		}
		if (dependent) {
			if (!contextual_field_identity_classifier_valid(
					terms,
					db,
					prototype_context_classifier_term(identity_field),
					left_classifier,
					right_classifier,
					left_arguments[i],
					right_arguments[i]
				)) {
				return 0;
			}
		} else if (prototype_term_type_instance_info(
				terms,
				prototype_context_classifier_term(identity_field),
				&identity_type_id,
				identity_arguments,
				&identity_argument_count
			) != 0 || identity_type_id != field_identity_type_id ||
			identity_argument_count != 2 ||
			identity_arguments[0] != left_arguments[i] ||
			identity_arguments[1] != right_arguments[i]) {
			return 0;
		}
	}
	(void)left_head;
	(void)right_head;
	return 1;
}

static int generated_identity_declaration_header_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id
) {
	if (!terms || !db || !contexts || source_carrier >= terms->term_count ||
		generated_type_id >= db->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* generated =
		&db->type_declarations[generated_type_id];
	uint32_t index_path[2];
	uint32_t index_count;
	uint32_t outer_binding;
	uint32_t outer_body;
	uint32_t inner_binding;
	uint32_t inner_body;
	if (!type_declaration_present(generated) || generated->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		generated->origin_source_carrier_term_id != source_carrier ||
		generated->parameter_count != 0 || generated->index_count != 2 ||
		prototype_context_extension_path(
			contexts,
			generated->parameter_context,
			generated->index_context,
			index_path,
			2,
			&index_count
		) != 0 || index_count != 2 ||
		prototype_context_classifier_term(
			prototype_context_get(contexts, index_path[0])
		) != source_carrier || prototype_context_classifier_term(
			prototype_context_get(contexts, index_path[1])
		) != source_carrier || generated->formation_classifier >=
			terms->term_count || terms->terms[
			generated->formation_classifier
		].tag != PROTOTYPE_TERM_PI || terms->terms[
			generated->formation_classifier
		].as.pi.domain != source_carrier || prototype_term_pure_family_parts(
			terms,
			terms->terms[generated->formation_classifier].as.pi.codomain_family,
			&outer_binding,
			&outer_body
		) != 0 || outer_body >= terms->term_count ||
		terms->terms[outer_body].tag != PROTOTYPE_TERM_PI ||
		terms->terms[outer_body].as.pi.domain != source_carrier ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[outer_body].as.pi.codomain_family,
			&inner_binding,
			&inner_body
		) != 0 || inner_body >= terms->term_count ||
		terms->terms[inner_body].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 0;
	}
	(void)outer_binding;
	(void)inner_binding;
	return 1;
}

static int term_is_binding_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t binding_id
) {
	return terms && term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[term_id].as.var.binding_id == binding_id;
}

int prototype_type_declaration_generated_identity_rule_for_source(
	const struct prototype_term_db* terms,
	uint32_t source_carrier
) {
	if (!terms || source_carrier >= terms->term_count) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	if (terms->terms[source_carrier].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT;
	}
	if (terms->terms[source_carrier].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE;
	}
	if (terms->terms[source_carrier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	uint32_t computation = terms->terms[source_carrier].as.thunk_type.computation;
	if (computation >= terms->term_count ||
		terms->terms[computation].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_effect_row_purity(
			terms, terms->terms[computation].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID;
	}
	return PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN;
}

static int generated_identity_constructor_source_ordinal(
	const struct prototype_term_db* terms,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	const struct prototype_type_constructor_declaration* constructor,
	uint32_t* p_source_ordinal
) {
	uint32_t result_type_id;
	uint32_t endpoints[2];
	uint32_t endpoint_count;
	uint32_t left_head;
	uint32_t left_owner;
	uint32_t left_ordinal;
	uint32_t left_arguments[64];
	uint32_t left_argument_count;
	uint32_t right_head;
	uint32_t right_owner;
	uint32_t right_ordinal;
	uint32_t right_arguments[64];
	uint32_t right_argument_count;
	if (!terms || !constructor || !p_source_ordinal ||
		prototype_term_type_instance_info(
			terms,
			constructor->result_classifier,
			&result_type_id,
			endpoints,
			&endpoint_count
		) != 0 || result_type_id != generated_type_id || endpoint_count != 2 ||
		prototype_term_constructor_spine_info(
			terms, endpoints[0], &left_head, &left_owner, &left_ordinal,
			left_arguments, 64, &left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms, endpoints[1], &right_head, &right_owner, &right_ordinal,
			right_arguments, 64, &right_argument_count
		) != 0 || left_owner != source_carrier ||
		right_owner != source_carrier || left_ordinal != right_ordinal ||
		left_argument_count != right_argument_count) {
		return -1;
	}
	*p_source_ordinal = left_ordinal;
	(void)left_head;
	(void)right_head;
	(void)left_arguments;
	(void)right_arguments;
	return 0;
}

static int indexed_nullary_source_constructor_compatible(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t source_ordinal
) {
	if (!terms || !db || !contexts || !source_type ||
		source_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + source_ordinal >= db->constructor_count) {
		return -1;
	}
	if (source_type->index_count == 0) {
		return 1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&db->constructor_declarations[
			source_type->first_constructor + source_ordinal
		];
	uint32_t fields[64];
	uint32_t field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			fields,
			64,
			&field_count
		) != 0) {
		return -1;
	}
	return field_count == 0 && constructor->result_classifier == source_carrier;
}

static int validate_generated_identity_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t remaining_depth
);

static int generated_identity_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi,
	uint32_t* p_domain,
	uint32_t* p_binding,
	uint32_t* p_codomain
) {
	if (!terms || !p_domain || !p_binding || !p_codomain ||
		pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			p_binding,
			p_codomain
		) != 0) {
		return 0;
	}
	*p_domain = terms->terms[pi].as.pi.domain;
	return 1;
}

static int validate_universe_correspondence_identity(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t universe,
	uint32_t generated_type_id
) {
	const struct prototype_type_declaration* generated =
		generated_type_id < db->type_count ?
		&db->type_declarations[generated_type_id] : NULL;
	uint32_t fields[7];
	uint32_t field_count;
	if (!terms || !db || !contexts || !generated ||
		universe >= terms->term_count ||
		terms->terms[universe].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		generated->constructor_count != 1 ||
		generated->first_constructor >= db->constructor_count) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&db->constructor_declarations[generated->first_constructor];
	if (constructor->owner_type != generated_type_id ||
		constructor->constructor_index != 0 ||
		constructor->parameter_context != generated->parameter_context ||
		prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			fields,
			7,
			&field_count
		) != 0 || field_count != 7) {
		return 0;
	}
	const struct prototype_context* field[7];
	for (uint32_t i = 0; i < 7; ++i) {
		field[i] = prototype_context_get(contexts, fields[i]);
		if (!field[i]) {
			return 0;
		}
	}
	if (prototype_context_classifier_term(field[0]) != universe ||
		prototype_context_classifier_term(field[1]) != universe) {
		return 0;
	}

	uint32_t domain;
	uint32_t binding;
	uint32_t codomain;
	uint32_t inner_domain;
	uint32_t inner_binding;
	uint32_t inner_codomain;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[2]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		!generated_identity_pi_parts(
			terms,
			codomain,
			&inner_domain,
			&inner_binding,
			&inner_codomain
		) || !term_is_binding_var(
			terms, inner_domain, field[1]->binding_id
		) || inner_codomain != universe) {
		return 0;
	}
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[3]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		!term_is_binding_var(terms, codomain, field[1]->binding_id) ||
		!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[4]),
			&domain,
			&binding,
			&codomain
		) || !term_is_binding_var(terms, domain, field[1]->binding_id) ||
		!term_is_binding_var(terms, codomain, field[0]->binding_id)) {
		return 0;
	}

	uint32_t right_lift_binding;
	uint32_t right_lift_type;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[5]),
			&domain,
			&right_lift_binding,
			&right_lift_type
		) || !term_is_binding_var(terms, domain, field[0]->binding_id) ||
		right_lift_type >= terms->term_count ||
		terms->terms[right_lift_type].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	uint32_t relation_x = terms->terms[right_lift_type].as.app.function;
	uint32_t trr_x = terms->terms[right_lift_type].as.app.argument;
	if (relation_x >= terms->term_count || trr_x >= terms->term_count ||
		terms->terms[relation_x].tag != PROTOTYPE_TERM_APP ||
		terms->terms[trr_x].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(
			terms, terms->terms[relation_x].as.app.function,
			field[2]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[relation_x].as.app.argument,
			right_lift_binding
		) || !term_is_binding_var(
			terms, terms->terms[trr_x].as.app.function,
			field[3]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[trr_x].as.app.argument,
			right_lift_binding
		)) {
		return 0;
	}

	uint32_t left_lift_binding;
	uint32_t left_lift_type;
	if (!generated_identity_pi_parts(
			terms,
			prototype_context_classifier_term(field[6]),
			&domain,
			&left_lift_binding,
			&left_lift_type
		) || !term_is_binding_var(terms, domain, field[1]->binding_id) ||
		left_lift_type >= terms->term_count ||
		terms->terms[left_lift_type].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	uint32_t relation_left = terms->terms[left_lift_type].as.app.function;
	uint32_t y = terms->terms[left_lift_type].as.app.argument;
	if (relation_left >= terms->term_count || y >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(terms, y, left_lift_binding)) {
		return 0;
	}
	uint32_t relation = terms->terms[relation_left].as.app.function;
	uint32_t trl_y = terms->terms[relation_left].as.app.argument;
	if (relation >= terms->term_count || trl_y >= terms->term_count ||
		terms->terms[relation].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[trl_y].tag != PROTOTYPE_TERM_APP ||
		!term_is_binding_var(terms, relation, field[2]->binding_id) ||
		!term_is_binding_var(
			terms, terms->terms[trl_y].as.app.function, field[4]->binding_id
		) || !term_is_binding_var(
			terms, terms->terms[trl_y].as.app.argument, left_lift_binding
		)) {
		return 0;
	}

	uint32_t result_type_id;
	uint32_t result_arguments[2];
	uint32_t result_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			constructor->result_classifier,
			&result_type_id,
			result_arguments,
			&result_argument_count
		) != 0 || result_type_id != generated_type_id ||
		result_argument_count != 2 || !term_is_binding_var(
			terms, result_arguments[0], field[0]->binding_id
		) || !term_is_binding_var(
			terms, result_arguments[1], field[1]->binding_id
		)) {
		return 0;
	}
	(void)binding;
	(void)inner_binding;
	return 1;
}

static int validate_nondependent_pi_identity_family(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t left_endpoint_binding,
	uint32_t right_endpoint_binding,
	uint32_t identity_family,
	uint32_t remaining_depth
) {
	if (!terms || !db || !contexts || remaining_depth == 0 ||
		source_carrier >= terms->term_count || identity_family >=
			terms->term_count || terms->terms[source_carrier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t source_pi = terms->terms[source_carrier].as.thunk_type.computation;
	uint32_t source_family_binding;
	uint32_t source_codomain;
	if (source_pi >= terms->term_count || terms->terms[source_pi].tag !=
			PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[source_pi].as.pi.codomain_family,
			&source_family_binding,
			&source_codomain
		) != 0 || source_codomain >= terms->term_count ||
		terms->terms[source_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_contains_free_binding(
			terms, source_codomain, source_family_binding
		) || prototype_term_effect_row_purity(
			terms, terms->terms[source_codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t domain = terms->terms[source_pi].as.pi.domain;
	uint32_t codomain_thunk = PROTOTYPE_INVALID_ID;
	uint32_t domain_identity_type_id;
	uint32_t codomain_identity_type_id;
	for (uint32_t i = 0; i < terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_THUNK_TYPE &&
			terms->terms[i].as.thunk_type.computation == source_codomain) {
			codomain_thunk = i;
			break;
		}
	}
	if (codomain_thunk == PROTOTYPE_INVALID_ID ||
		prototype_type_declaration_find_generated_identity(
			db,
			domain,
			prototype_context_empty(contexts),
			&domain_identity_type_id
		) != 0 || prototype_type_declaration_find_generated_identity(
			db,
			codomain_thunk,
			prototype_context_empty(contexts),
			&codomain_identity_type_id
		) != 0 || !validate_generated_identity_depth(
			terms,
			db,
			contexts,
			domain,
			domain_identity_type_id,
			prototype_type_declaration_generated_identity_rule_for_source(
				terms, domain
			),
			remaining_depth - 1
		) || !validate_generated_identity_depth(
			terms,
			db,
			contexts,
			codomain_thunk,
			codomain_identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			remaining_depth - 1
		)) {
		return 0;
	}
	uint32_t current = identity_family;
	uint32_t binders[3];
	uint32_t bodies[3];
	uint32_t pi_terms[3];
	for (uint32_t i = 0; i < 3; ++i) {
		if (current >= terms->term_count || terms->terms[current].tag !=
				PROTOTYPE_TERM_THUNK_TYPE) {
			return 0;
		}
		pi_terms[i] = terms->terms[current].as.thunk_type.computation;
		if (pi_terms[i] >= terms->term_count || terms->terms[pi_terms[i]].tag !=
				PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
				terms,
				terms->terms[pi_terms[i]].as.pi.codomain_family,
				&binders[i],
				&bodies[i]
			) != 0 || bodies[i] >= terms->term_count ||
			terms->terms[bodies[i]].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			prototype_term_effect_row_purity(
				terms, terms->terms[bodies[i]].as.computation_type.label
			) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
			return 0;
		}
		if (i < 2) {
			current = terms->terms[bodies[i]].as.computation_type.result;
		}
	}
	if (terms->terms[pi_terms[0]].as.pi.domain != domain ||
		terms->terms[pi_terms[1]].as.pi.domain != domain) {
		return 0;
	}
	uint32_t domain_identity_type;
	uint32_t domain_identity_arguments[2];
	uint32_t domain_identity_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			terms->terms[pi_terms[2]].as.pi.domain,
			&domain_identity_type,
			domain_identity_arguments,
			&domain_identity_argument_count
		) != 0 || domain_identity_type != domain_identity_type_id ||
		domain_identity_argument_count != 2 || !term_is_binding_var(
			terms, domain_identity_arguments[0], binders[0]
		) || !term_is_binding_var(
			terms, domain_identity_arguments[1], binders[1]
		)) {
		return 0;
	}
	uint32_t result_identity = terms->terms[bodies[2]].as.computation_type.result;
	uint32_t result_identity_type;
	uint32_t result_identity_arguments[2];
	uint32_t result_identity_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			result_identity,
			&result_identity_type,
			result_identity_arguments,
			&result_identity_argument_count
		) != 0 || result_identity_type != codomain_identity_type_id ||
		result_identity_argument_count != 2) {
		return 0;
	}
	uint32_t expected_endpoint_bindings[2] = {
		left_endpoint_binding, right_endpoint_binding
	};
	uint32_t expected_input_bindings[2] = { binders[0], binders[1] };
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t thunk = result_identity_arguments[i];
		uint32_t application = thunk < terms->term_count &&
			terms->terms[thunk].tag == PROTOTYPE_TERM_THUNK ?
			terms->terms[thunk].as.thunk.computation : PROTOTYPE_INVALID_ID;
		uint32_t forced = application < terms->term_count &&
			terms->terms[application].tag == PROTOTYPE_TERM_APP ?
			terms->terms[application].as.app.function : PROTOTYPE_INVALID_ID;
		uint32_t argument = application < terms->term_count &&
			terms->terms[application].tag == PROTOTYPE_TERM_APP ?
			terms->terms[application].as.app.argument : PROTOTYPE_INVALID_ID;
		uint32_t endpoint = forced < terms->term_count &&
			terms->terms[forced].tag == PROTOTYPE_TERM_FORCE ?
			terms->terms[forced].as.force.value : PROTOTYPE_INVALID_ID;
		if (!term_is_binding_var(
				terms, endpoint, expected_endpoint_bindings[i]
			) || !term_is_binding_var(
				terms, argument, expected_input_bindings[i]
			)) {
			return 0;
		}
	}
	return binders[0] != binders[1] && binders[0] != binders[2] &&
		binders[1] != binders[2];
}

static int validate_generated_identity_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t remaining_depth
) {
	if (!terms || !db || !contexts || source_carrier >= terms->term_count ||
		generated_type_id >= db->type_count || remaining_depth == 0 ||
		!generated_identity_declaration_header_valid(
			terms, db, contexts, source_carrier, generated_type_id
		)) {
		return 0;
	}
	const struct prototype_type_declaration* generated =
		&db->type_declarations[generated_type_id];
	if (computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		return validate_universe_correspondence_identity(
			terms, db, contexts, source_carrier, generated_type_id
		);
	}
	if (computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
		uint32_t computation = terms->terms[source_carrier].tag ==
			PROTOTYPE_TERM_THUNK_TYPE ?
			terms->terms[source_carrier].as.thunk_type.computation :
			PROTOTYPE_INVALID_ID;
		uint32_t result_carrier = computation < terms->term_count &&
			terms->terms[computation].tag == PROTOTYPE_TERM_COMPUTATION_TYPE ?
			terms->terms[computation].as.computation_type.result :
			PROTOTYPE_INVALID_ID;
		uint32_t result_identity_type_id = PROTOTYPE_INVALID_ID;
		int result_has_generated_identity = result_carrier != PROTOTYPE_INVALID_ID &&
			prototype_type_declaration_find_generated_identity(
				db,
				result_carrier,
				generated->parameter_context,
				&result_identity_type_id
			) == 0;
		if (result_carrier == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_purity(
				terms, terms->terms[computation].as.computation_type.label
			) != PROTOTYPE_EFFECT_ROW_PURITY_PURE || generated->constructor_count != 1 ||
			(result_has_generated_identity && !validate_generated_identity_depth(
				terms,
				db,
				contexts,
				result_carrier,
				result_identity_type_id,
				prototype_type_declaration_generated_identity_rule_for_source(
					terms, result_carrier
				),
				remaining_depth - 1
			))) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&db->constructor_declarations[generated->first_constructor];
		uint32_t fields[3];
		uint32_t field_count;
		if (generated->first_constructor >= db->constructor_count ||
			constructor->owner_type != generated_type_id ||
			constructor->constructor_index != 0 ||
			constructor->parameter_context != generated->parameter_context ||
			prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				fields,
				3,
				&field_count
			) != 0 || field_count != 3) {
			return 0;
		}
		const struct prototype_context* left = prototype_context_get(
			contexts, fields[0]
		);
		const struct prototype_context* right = prototype_context_get(
			contexts, fields[1]
		);
		const struct prototype_context* identity = prototype_context_get(
			contexts, fields[2]
		);
		uint32_t identity_type_id;
		uint32_t identity_arguments[2];
		uint32_t identity_argument_count;
		uint32_t result_type_id;
		uint32_t result_arguments[2];
		uint32_t result_argument_count;
		if (!left || !right || !identity ||
			prototype_context_classifier_term(left) != result_carrier ||
			prototype_context_classifier_term(right) != result_carrier ||
			(result_has_generated_identity && (prototype_term_type_instance_info(
				terms,
				prototype_context_classifier_term(identity),
				&identity_type_id,
				identity_arguments,
				&identity_argument_count
			) != 0 || identity_type_id != result_identity_type_id ||
			identity_argument_count != 2 || !term_is_binding_var(
				terms, identity_arguments[0], left->binding_id
			) || !term_is_binding_var(
				terms, identity_arguments[1], right->binding_id
			))) || (!result_has_generated_identity &&
			!validate_nondependent_pi_identity_family(
				terms,
				db,
				contexts,
				result_carrier,
				left->binding_id,
				right->binding_id,
				prototype_context_classifier_term(identity),
				remaining_depth - 1
			)) || prototype_term_type_instance_info(
				terms,
				constructor->result_classifier,
				&result_type_id,
				result_arguments,
				&result_argument_count
			) != 0 || result_type_id != generated_type_id ||
			result_argument_count != 2) {
			return 0;
		}
		for (uint32_t i = 0; i < 2; ++i) {
			uint32_t thunk = result_arguments[i];
			uint32_t returned = thunk < terms->term_count &&
				terms->terms[thunk].tag == PROTOTYPE_TERM_THUNK ?
				terms->terms[thunk].as.thunk.computation : PROTOTYPE_INVALID_ID;
			uint32_t value = returned < terms->term_count &&
				terms->terms[returned].tag == PROTOTYPE_TERM_RETURN ?
				terms->terms[returned].as.return_term.value : PROTOTYPE_INVALID_ID;
			uint32_t expected_binding = i == 0 ?
				left->binding_id : right->binding_id;
			if (!term_is_binding_var(terms, value, expected_binding)) {
				return 0;
			}
		}
		return 1;
	}
	if (computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		terms->terms[source_carrier].tag != PROTOTYPE_TERM_TYPE_VIEW) {
		return 0;
	}
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_type;
	if (prototype_type_view_declaration_query(
			db, contexts, terms, source_carrier, &source_type_id, &source_type
		) != 0 || source_type_id >= db->type_count || !source_type ||
		source_type->parameter_count != 0 || source_type->constructor_count == 0) {
		return 0;
	}
	uint32_t source_arguments[16];
	uint32_t source_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			source_carrier,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_argument_count != source_type->index_count) {
		return 0;
	}
	uint32_t expected_constructor_count = 0;
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		int compatible = indexed_nullary_source_constructor_compatible(
			terms, db, contexts, source_carrier, source_type, i
		);
		if (compatible < 0) {
			return 0;
		}
		expected_constructor_count += compatible != 0;
	}
	if (generated->constructor_count != expected_constructor_count) {
		return 0;
	}
	for (uint32_t i = 0; i < generated->constructor_count; ++i) {
		uint32_t constructor_id = generated->first_constructor + i;
		uint32_t source_ordinal;
		if (constructor_id >= db->constructor_count ||
			generated_identity_constructor_source_ordinal(
				terms,
				source_carrier,
				generated_type_id,
				&db->constructor_declarations[constructor_id],
				&source_ordinal
			) != 0 || source_ordinal >= source_type->constructor_count ||
			indexed_nullary_source_constructor_compatible(
				terms,
				db,
				contexts,
				source_carrier,
				source_type,
				source_ordinal
			) != 1 ||
			!generated_adt_identity_constructor_valid(
				terms,
				db,
				contexts,
				source_carrier,
				source_type,
				&db->constructor_declarations[constructor_id],
				source_ordinal,
				generated_type_id
			)) {
			return 0;
		}
		for (uint32_t j = 0; j < i; ++j) {
			uint32_t previous_id = generated->first_constructor + j;
			uint32_t previous_ordinal;
			if (previous_id >= db->constructor_count ||
				generated_identity_constructor_source_ordinal(
					terms,
					source_carrier,
					generated_type_id,
					&db->constructor_declarations[previous_id],
					&previous_ordinal
				) != 0 || previous_ordinal == source_ordinal) {
				return 0;
			}
		}
	}
	(void)source_arguments;
	return 1;
}

int prototype_type_declaration_validate_generated_identity(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	uint32_t generated_type_id,
	int computation_rule
) {
	return validate_generated_identity_depth(
		terms,
		db,
		contexts,
		source_carrier,
		generated_type_id,
		computation_rule,
		terms ? (uint32_t)terms->term_count + 1 : 0
	);
}

int prototype_type_declaration_add_parameter(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t binding_id,
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
	db->parameter_declarations[id].binding_id = binding_id;
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
		uint32_t field_classifier = prototype_context_classifier_term(field);
		uint32_t codomain_family;
		uint32_t pi_classifier;
		if (!field || field_classifier == PROTOTYPE_INVALID_ID ||
			prototype_term_pure_family(
				terms,
				field->binding_id,
				classifier,
				&codomain_family
			) != 0 ||
			prototype_term_pi_family(
				terms,
				field_classifier,
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
				terms, parameter->binding_id, classifier, &lambda
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
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	const struct prototype_match_case* left,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	const struct prototype_match_case* right,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!left_terms || !left_db || !right_terms || !right_db || !left ||
		!right || !env ||
		left->constructor_id != right->constructor_id ||
		left->binder_count != right->binder_count ||
		left->first_binder + left->binder_count > left_terms->case_binder_count ||
		right->first_binder + right->binder_count >
			right_terms->case_binder_count ||
		!representation_terms_equal_at_depth(
			left_terms, left_db, left->constructor_owner,
			right_terms, right_db, right->constructor_owner, env, depth + 1
		)) {
		return 0;
	}
	uint32_t saved = env->binder_count;
	for (uint32_t i = 0; i < left->binder_count; ++i) {
		if (representation_push_binder(
				env,
				left_terms->case_binders[left->first_binder + i].binding_id,
				right_terms->case_binders[right->first_binder + i].binding_id
			) != 0) {
			env->binder_count = saved;
			return 0;
		}
	}
	int equal = representation_terms_equal_at_depth(
		left_terms, left_db, left->body,
		right_terms, right_db, right->body, env, depth + 1
	);
	env->binder_count = saved;
	return equal;
}

static int representation_terms_equal_at_depth(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	uint32_t left_term_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	uint32_t right_term_id,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!left_terms || !left_db || !right_terms || !right_db || !env ||
		left_term_id >= left_terms->term_count ||
		right_term_id >= right_terms->term_count ||
		depth > PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY) {
		return 0;
	}
	const struct prototype_term* left = &left_terms->terms[left_term_id];
	const struct prototype_term* right = &right_terms->terms[right_term_id];
	if (left->tag != right->tag) {
		return 0;
	}
#define REPRESENTATION_TERMS_EQUAL(left_id, right_id) \
	representation_terms_equal_at_depth( \
		left_terms, left_db, (left_id), right_terms, right_db, (right_id), \
		env, depth + 1 \
	)
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return representation_binders_equal(
				env, left->as.var.binding_id, right->as.var.binding_id
			);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id ==
					right->as.constructor.constructor_id &&
				REPRESENTATION_TERMS_EQUAL(
					left->as.constructor.owner, right->as.constructor.owner
				);
		case PROTOTYPE_TERM_APP:
			return REPRESENTATION_TERMS_EQUAL(
					left->as.app.function, right->as.app.function
				) && REPRESENTATION_TERMS_EQUAL(
					left->as.app.argument, right->as.app.argument
				);
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_PI: {
			uint32_t left_body = PROTOTYPE_INVALID_ID;
			uint32_t right_body = PROTOTYPE_INVALID_ID;
			uint32_t left_binder = PROTOTYPE_INVALID_ID;
			uint32_t right_binder = PROTOTYPE_INVALID_ID;
			if (left->tag == PROTOTYPE_TERM_LAMBDA) {
				left_binder = left->as.lambda.binding_id;
				right_binder = right->as.lambda.binding_id;
				left_body = left->as.lambda.body;
				right_body = right->as.lambda.body;
			} else {
				if (prototype_term_pure_family_parts(
						left_terms,
						left->as.pi.codomain_family,
						&left_binder,
						&left_body
					) != 0 || prototype_term_pure_family_parts(
						right_terms,
						right->as.pi.codomain_family,
						&right_binder,
						&right_body
					) != 0) {
					return 0;
				}
			}
			uint32_t saved = env->binder_count;
			if (left->tag == PROTOTYPE_TERM_PI &&
				!REPRESENTATION_TERMS_EQUAL(
					left->as.pi.domain, right->as.pi.domain
				)) {
				return 0;
			}
			if (left_binder != PROTOTYPE_INVALID_ID &&
				representation_push_binder(env, left_binder, right_binder) != 0) {
				return 0;
			}
			int equal = REPRESENTATION_TERMS_EQUAL(left_body, right_body);
			env->binder_count = saved;
			return equal;
		}
		case PROTOTYPE_TERM_MATCH:
			if (left->as.match.case_count != right->as.match.case_count ||
				left->as.match.first_case + left->as.match.case_count >
					left_terms->case_count ||
				right->as.match.first_case + right->as.match.case_count >
					right_terms->case_count ||
				!REPRESENTATION_TERMS_EQUAL(
					left->as.match.scrutinee, right->as.match.scrutinee
				)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				if (!representation_match_cases_equal_at_depth(
						left_terms,
						left_db,
						&left_terms->cases[left->as.match.first_case + i],
						right_terms,
						right_db,
						&right_terms->cases[right->as.match.first_case + i],
						env,
						depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_VIEW:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.type_view.source, right->as.type_view.source
			);
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return representation_types_equal_at_depth(
				left_terms, left_db, left->as.type_declaration.type_id,
				right_terms, right_db, right->as.type_declaration.type_id,
				env, depth + 1
			);
		case PROTOTYPE_TERM_TYPE_FORMER: {
			uint32_t left_representation = left->as.type_former.representation_id;
			uint32_t right_representation = right->as.type_former.representation_id;
			if (left_db == right_db && left_representation == right_representation) {
				return 1;
			}
			if (left_representation >= left_db->representation_count ||
				right_representation >= right_db->representation_count) {
				return 0;
			}
			return representation_types_equal_at_depth(
				left_terms,
				left_db,
				left_db->representations[left_representation].representative_type_id,
				right_terms,
				right_db,
				right_db->representations[right_representation].representative_type_id,
				env,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return 1;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
			return 1;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return left->as.external_ref.name.namespace_symbol_id ==
					right->as.external_ref.name.namespace_symbol_id &&
				left->as.external_ref.name.name_symbol_id ==
					right->as.external_ref.name.name_symbol_id;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			return left->as.pure_primitive.primitive_id ==
					right->as.pure_primitive.primitive_id &&
				left->as.pure_primitive.type_symbol_id == right->as.pure_primitive.type_symbol_id;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return left->as.effect_operation.operation_id ==
					right->as.effect_operation.operation_id &&
				REPRESENTATION_TERMS_EQUAL(
					left->as.effect_operation.classifier,
					right->as.effect_operation.classifier
				);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.induction_hypothesis.argument,
				right->as.induction_hypothesis.argument
			);
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return representation_binders_equal(
				env, left->as.effect_row_var.binding_id, right->as.effect_row_var.binding_id
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return REPRESENTATION_TERMS_EQUAL(
					left->as.effect_row_union.left, right->as.effect_row_union.left
				) && REPRESENTATION_TERMS_EQUAL(
					left->as.effect_row_union.right, right->as.effect_row_union.right
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t saved = env->binder_count;
			if (representation_push_binder(
					env,
					left->as.effect_row_forall.binding_id,
					right->as.effect_row_forall.binding_id
				) != 0) {
				return 0;
			}
			int equal = REPRESENTATION_TERMS_EQUAL(
				left->as.effect_row_forall.body,
				right->as.effect_row_forall.body
			);
			env->binder_count = saved;
			return equal;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return left->as.effect_row_operation.operation_id ==
					right->as.effect_row_operation.operation_id &&
				REPRESENTATION_TERMS_EQUAL(
					left->as.effect_row_operation.latent_row,
					right->as.effect_row_operation.latent_row
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return REPRESENTATION_TERMS_EQUAL(
					left->as.computation_type.label,
					right->as.computation_type.label
				) && REPRESENTATION_TERMS_EQUAL(
					left->as.computation_type.result,
					right->as.computation_type.result
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.thunk_type.computation,
				right->as.thunk_type.computation
			);
		case PROTOTYPE_TERM_RETURN:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.return_term.value, right->as.return_term.value
			);
		case PROTOTYPE_TERM_THUNK:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.thunk.computation, right->as.thunk.computation
			);
		case PROTOTYPE_TERM_FORCE:
			return REPRESENTATION_TERMS_EQUAL(
				left->as.force.value, right->as.force.value
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			if (!REPRESENTATION_TERMS_EQUAL(
					left->as.computation_fold.computation,
					right->as.computation_fold.computation
				) || !REPRESENTATION_TERMS_EQUAL(
					left->as.computation_fold.return_clause,
					right->as.computation_fold.return_clause
				) || left->as.computation_fold.clause_count != right->as.computation_fold.clause_count) {
				return 0;
			}
			if (left->as.computation_fold.first_clause +
					left->as.computation_fold.clause_count >
					left_terms->computation_fold_clause_count ||
				right->as.computation_fold.first_clause +
					right->as.computation_fold.clause_count >
					right_terms->computation_fold_clause_count) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&left_terms->computation_fold_clauses[
						left->as.computation_fold.first_clause + i
					];
				const struct prototype_computation_fold_clause* right_clause =
					&right_terms->computation_fold_clauses[
						right->as.computation_fold.first_clause + i
					];
				if (!REPRESENTATION_TERMS_EQUAL(
						left_clause->operation, right_clause->operation
					) || !REPRESENTATION_TERMS_EQUAL(
						left_clause->body, right_clause->body
					)) {
					return 0;
				}
			}
			return 1;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return REPRESENTATION_TERMS_EQUAL(
					left->as.operation_request.operation,
					right->as.operation_request.operation
				) && REPRESENTATION_TERMS_EQUAL(
					left->as.operation_request.argument,
					right->as.operation_request.argument
				) && REPRESENTATION_TERMS_EQUAL(
					left->as.operation_request.continuation,
					right->as.operation_request.continuation
			);
		case PROTOTYPE_TERM_TEXT_LITERAL:
			return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
		case PROTOTYPE_TERM_INT_LITERAL:
			return left->as.int_literal.value == right->as.int_literal.value;
		default:
			return 0;
	}
#undef REPRESENTATION_TERMS_EQUAL
}

static int representation_types_equal_at_depth(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	uint32_t left_type_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	uint32_t right_type_id,
	struct representation_compare_env* env,
	uint32_t depth
) {
	if (!left_terms || !left_db || !right_terms || !right_db || !env ||
		left_type_id >= left_db->type_count || right_type_id >= right_db->type_count ||
		depth > PROTOTYPE_REPRESENTATION_COMPARE_CAPACITY ||
		!type_declaration_present(&left_db->type_declarations[left_type_id]) ||
		!type_declaration_present(&right_db->type_declarations[right_type_id])) {
		return 0;
	}
	int pair_status = representation_type_pair_lookup(env, left_type_id, right_type_id);
	if (pair_status > 0) {
		return 1;
	}
	if (pair_status < 0) {
		return 0;
	}
	const struct prototype_type_declaration* left =
		&left_db->type_declarations[left_type_id];
	const struct prototype_type_declaration* right =
		&right_db->type_declarations[right_type_id];
	if (left->parameter_count != right->parameter_count ||
		left->index_count != right->index_count ||
		left->constructor_count != right->constructor_count ||
		left->parameter_count > 64 || left->index_count > 64 ||
		left->first_constructor + left->constructor_count >
			left_db->constructor_count ||
		right->first_constructor + right->constructor_count >
			right_db->constructor_count ||
		!env->left_contexts || !env->right_contexts ||
		representation_push_type_pair(env, left_type_id, right_type_id) != 0) {
		return 0;
	}
	uint32_t left_parameter_path[64];
	uint32_t right_parameter_path[64];
	uint32_t left_context = left->parameter_context;
	uint32_t right_context = right->parameter_context;
	for (uint32_t i = left->parameter_count; i > 0; --i) {
		const struct prototype_context* left_entry =
			prototype_context_get(env->left_contexts, left_context);
		const struct prototype_context* right_entry =
			prototype_context_get(env->right_contexts, right_context);
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
			prototype_context_get(env->left_contexts, left_parameter_path[i]);
		const struct prototype_context* right_parameter =
			prototype_context_get(env->right_contexts, right_parameter_path[i]);
		uint32_t left_classifier =
			prototype_context_classifier_term(left_parameter);
		uint32_t right_classifier =
			prototype_context_classifier_term(right_parameter);
		if (!left_parameter || !right_parameter ||
			left_classifier == PROTOTYPE_INVALID_ID ||
			right_classifier == PROTOTYPE_INVALID_ID ||
			!representation_terms_equal_at_depth(
				left_terms, left_db, left_classifier,
				right_terms, right_db, right_classifier, env, depth + 1
			) ||
			representation_push_binder(
				env, left_parameter->binding_id, right_parameter->binding_id
			) != 0) {
			env->binder_count = saved_binders;
			return 0;
		}
	}
	uint32_t parameter_binders = env->binder_count;
	uint32_t left_index_path[64];
	uint32_t right_index_path[64];
	uint32_t left_index_count;
	uint32_t right_index_count;
	if (prototype_context_extension_path(
			env->left_contexts,
			left->parameter_context,
			left->index_context,
			left_index_path,
			64,
			&left_index_count
		) != 0 || prototype_context_extension_path(
			env->right_contexts,
			right->parameter_context,
			right->index_context,
			right_index_path,
			64,
			&right_index_count
		) != 0 || left_index_count != left->index_count ||
		right_index_count != right->index_count) {
		env->binder_count = saved_binders;
		return 0;
	}
	for (uint32_t i = 0; i < left_index_count; ++i) {
		const struct prototype_context* left_index =
			prototype_context_get(env->left_contexts, left_index_path[i]);
		const struct prototype_context* right_index =
			prototype_context_get(env->right_contexts, right_index_path[i]);
		uint32_t left_classifier =
			prototype_context_classifier_term(left_index);
		uint32_t right_classifier =
			prototype_context_classifier_term(right_index);
		if (!left_index || !right_index ||
			left_classifier == PROTOTYPE_INVALID_ID ||
			right_classifier == PROTOTYPE_INVALID_ID ||
			!representation_terms_equal_at_depth(
				left_terms, left_db, left_classifier,
				right_terms, right_db, right_classifier, env, depth + 1
			) || representation_push_binder(
				env, left_index->binding_id, right_index->binding_id
			) != 0) {
			env->binder_count = saved_binders;
			return 0;
		}
	}
	for (uint32_t i = 0; i < left->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* left_constructor =
			&left_db->constructor_declarations[left->first_constructor + i];
		const struct prototype_type_constructor_declaration* right_constructor =
			&right_db->constructor_declarations[right->first_constructor + i];
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
				env->left_contexts, left_constructor->parameter_context,
				left_constructor->field_context, left_fields, 64,
				&left_field_count
			) != 0 ||
			prototype_context_extension_path(
				env->right_contexts, right_constructor->parameter_context,
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
				prototype_context_get(env->left_contexts, left_fields[j]);
			const struct prototype_context* right_field =
				prototype_context_get(env->right_contexts, right_fields[j]);
			uint32_t left_classifier =
				prototype_context_classifier_term(left_field);
			uint32_t right_classifier =
				prototype_context_classifier_term(right_field);
			if (!left_field || !right_field ||
				left_classifier == PROTOTYPE_INVALID_ID ||
				right_classifier == PROTOTYPE_INVALID_ID ||
				!representation_terms_equal_at_depth(
					left_terms, left_db, left_classifier,
					right_terms, right_db, right_classifier, env, depth + 1
				) ||
				representation_push_binder(
					env, left_field->binding_id, right_field->binding_id
				) != 0) {
				env->binder_count = saved_binders;
				return 0;
			}
		}
		if (!representation_terms_equal_at_depth(
				left_terms, left_db, left_constructor->result_classifier,
				right_terms, right_db, right_constructor->result_classifier,
				env, depth + 1
			)) {
			env->binder_count = saved_binders;
			return 0;
		}
	}
	env->binder_count = saved_binders;
	return 1;
}

static void type_representation_fingerprint_merge_referenced_key(
	struct prototype_type_representation_fingerprint* key,
	const struct prototype_type_representation_fingerprint* referenced
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

static int type_representation_fingerprint_term_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_representation_fingerprint_binder_env* env,
	struct prototype_type_representation_fingerprint* key,
	uint64_t* p_hash,
	uint32_t depth
);

static int type_representation_fingerprint_type_instance_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_representation_fingerprint_binder_env* env,
	struct prototype_type_representation_fingerprint* key,
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
	type_representation_fingerprint_hash_mix_tag(p_hash, 0x74797065U);
	if (type_id == self_type_id) {
		type_representation_fingerprint_hash_mix_tag(p_hash, 0x73656c66U);
	} else {
		struct prototype_type_representation_fingerprint referenced;
		if (prototype_type_declaration_representation_fingerprint(
				terms,
				db,
				env->contexts,
				type_id,
				&referenced
			) != 0) {
			return -1;
		}
		type_representation_fingerprint_hash_mix_tag(p_hash, 0x72656674U);
		type_representation_fingerprint_hash_mix_key(p_hash, &referenced);
		type_representation_fingerprint_merge_referenced_key(key, &referenced);
	}
	type_representation_fingerprint_hash_mix_u32(p_hash, arg_count);
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (type_representation_fingerprint_term_at_depth(
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

static int type_representation_fingerprint_match_case_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	const struct prototype_match_case* match_case,
	struct type_representation_fingerprint_binder_env* env,
	struct prototype_type_representation_fingerprint* key,
	uint64_t* p_hash,
	uint32_t depth
) {
	if (!terms || !db || !match_case || !env || !key || !p_hash ||
		depth > 256 ||
		match_case->first_binder + match_case->binder_count > terms->case_binder_count) {
		return -1;
	}
	type_representation_fingerprint_hash_mix_u32(p_hash, match_case->constructor_id);
	type_representation_fingerprint_hash_mix_u32(p_hash, match_case->binder_count);
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		type_representation_fingerprint_hash_mix_u32(p_hash, PROTOTYPE_INVALID_ID);
	} else if (type_representation_fingerprint_term_at_depth(
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
		if (type_representation_fingerprint_env_push(env, binder->binding_id) != 0) {
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return -1;
		}
		key->bound_binder_count++;
	}
	int status = type_representation_fingerprint_term_at_depth(
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

static int type_representation_fingerprint_term_at_depth(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t self_type_id,
	uint32_t term_id,
	struct type_representation_fingerprint_binder_env* env,
	struct prototype_type_representation_fingerprint* key,
	uint64_t* p_hash,
	uint32_t depth
) {
	if (!terms || !db || !env || !key || !p_hash ||
		term_id >= terms->term_count ||
		depth > 256) {
		return -1;
	}

	int handled = 0;
	if (type_representation_fingerprint_type_instance_at_depth(
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
	type_representation_fingerprint_hash_mix_tag(p_hash, (uint32_t)term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			uint32_t slot;
			if (type_representation_fingerprint_env_lookup(env, term->as.var.binding_id, &slot)) {
				type_representation_fingerprint_hash_mix_u32(p_hash, 1);
				type_representation_fingerprint_hash_mix_u32(p_hash, slot);
			} else {
				type_representation_fingerprint_hash_mix_u32(p_hash, 0);
				type_representation_fingerprint_hash_mix_u32(p_hash, term->as.var.binding_id);
				key->free_binder_count++;
			}
			return 0;
		}
		case PROTOTYPE_TERM_CONSTRUCTOR:
			type_representation_fingerprint_hash_mix_u32(p_hash, term->as.constructor.constructor_id);
			return type_representation_fingerprint_term_at_depth(
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
			if (type_representation_fingerprint_term_at_depth(
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
			return type_representation_fingerprint_term_at_depth(
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
			if (type_representation_fingerprint_env_push(env, term->as.lambda.binding_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = type_representation_fingerprint_term_at_depth(
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
			uint32_t binding_id;
			uint32_t body;
			if (type_representation_fingerprint_term_at_depth(
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
					&binding_id,
					&body
				) != 0) {
				return -1;
			}
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (type_representation_fingerprint_env_push(env, binding_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = type_representation_fingerprint_term_at_depth(
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
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			{
				uint32_t slot;
				if (type_representation_fingerprint_env_lookup(
						env, term->as.effect_row_var.binding_id, &slot
					)) {
					type_representation_fingerprint_hash_mix_u32(p_hash, slot);
				} else {
					type_representation_fingerprint_hash_mix_u32(p_hash, term->as.effect_row_var.binding_id);
				}
			}
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			if (type_representation_fingerprint_term_at_depth(
					terms, db, self_type_id, term->as.effect_row_union.left,
					env, key, p_hash, depth + 1
				) != 0) {
				return -1;
			}
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.effect_row_union.right,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			if (type_representation_fingerprint_term_at_depth(
					terms, db, self_type_id, term->as.computation_type.label,
					env, key, p_hash, depth + 1
				) != 0) {
				return -1;
			}
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.computation_type.result,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.thunk_type.computation,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.return_term.value,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.thunk.computation,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return type_representation_fingerprint_term_at_depth(
				terms, db, self_type_id, term->as.force.value,
				env, key, p_hash, depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			if (term->as.match.first_case + term->as.match.case_count > terms->case_count) {
				return -1;
			}
			type_representation_fingerprint_hash_mix_u32(p_hash, term->as.match.case_count);
			if (type_representation_fingerprint_term_at_depth(
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
				if (type_representation_fingerprint_match_case_at_depth(
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
			type_representation_fingerprint_hash_mix_u32(p_hash, term->as.induction_hypothesis.ih_scope_id);
			return type_representation_fingerprint_term_at_depth(
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
			if (type_representation_fingerprint_level_env_slot(
					env,
					term->as.universe_var.level_var,
					&slot
				) != 0) {
				return -1;
			}
			type_representation_fingerprint_hash_mix_u32(p_hash, slot);
			return 0;
		}
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)term->as.text_literal.text_symbol_id);
				return 0;
				case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)term->as.int_literal.value);
				type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)((uint64_t)term->as.int_literal.value >> 32));
				return 0;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				key->has_name_reference = 1;
				type_representation_fingerprint_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.namespace_symbol_id
				);
				type_representation_fingerprint_hash_mix_u32(
					p_hash,
					(uint32_t)term->as.external_ref.name.name_symbol_id
				);
				return 0;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			type_representation_fingerprint_hash_mix_u32(
				p_hash, (uint32_t)term->as.pure_primitive.primitive_id
			);
			type_representation_fingerprint_hash_mix_u32(p_hash, (uint32_t)term->as.pure_primitive.type_symbol_id);
			return 0;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			type_representation_fingerprint_hash_mix_u32(p_hash,
				(uint32_t)term->as.effect_operation.operation_id);
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (term->as.type_declaration.type_id == self_type_id) {
				type_representation_fingerprint_hash_mix_tag(p_hash, 0x73656c66U);
				return 0;
			}
			if (term->as.type_declaration.type_id >= db->type_count) {
				return -1;
			}
			{
				struct prototype_type_representation_fingerprint referenced;
				if (prototype_type_declaration_representation_fingerprint(
						terms,
						db,
						env->contexts,
						term->as.type_declaration.type_id,
						&referenced
					) != 0) {
					return -1;
				}
				type_representation_fingerprint_hash_mix_key(p_hash, &referenced);
				type_representation_fingerprint_merge_referenced_key(key, &referenced);
				return 0;
			}
		case PROTOTYPE_TERM_TYPE_VIEW:
			return type_representation_fingerprint_term_at_depth(
				terms,
				db,
				self_type_id,
				term->as.type_view.source,
				env,
				key,
				p_hash,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_FORMER:
			return -1;
		default:
			return -1;
	}
}

int prototype_type_declaration_representation_fingerprint(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t type_id,
	struct prototype_type_representation_fingerprint* p_key
) {
	if (!terms || !db || !contexts || !p_key || type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}

	const struct prototype_type_declaration* type = &db->type_declarations[type_id];
	if (type->first_parameter + type->parameter_count > db->parameter_count ||
		type->first_constructor + type->constructor_count > db->constructor_count ||
		type->parameter_count > 64 || type->index_count > 64 ||
		!prototype_context_get(contexts, type->parameter_context) ||
		!prototype_context_get(contexts, type->index_context)) {
		return -1;
	}
	struct type_representation_fingerprint_binder_env env;
	uint64_t hash = PROTOTYPE_TYPE_REPRESENTATION_FINGERPRINT_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	env.contexts = contexts;
	memset(p_key, 0, sizeof(*p_key));
	p_key->parameter_count = type->parameter_count;
	p_key->index_count = type->index_count;
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
	type_representation_fingerprint_hash_mix_u32(&hash, type->parameter_count);
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		const struct prototype_context* parameter =
			prototype_context_get(contexts, parameter_path[i]);
		uint32_t parameter_classifier =
			prototype_context_classifier_term(parameter);
		if (!parameter || parameter_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		type_representation_fingerprint_hash_mix_tag(&hash, 0x7061726dU);
		if (type_representation_fingerprint_term_at_depth(
				terms,
				db,
				type_id,
				parameter_classifier,
				&env,
				p_key,
				&hash,
				0
			) != 0 ||
			type_representation_fingerprint_env_push(&env, parameter->binding_id) != 0) {
			return -1;
		}
		p_key->bound_binder_count++;
	}

	uint32_t index_path[64];
	uint32_t index_count;
	if (prototype_context_extension_path(
			contexts,
			type->parameter_context,
			type->index_context,
			index_path,
			64,
			&index_count
		) != 0 || index_count != type->index_count) {
		return -1;
	}
	type_representation_fingerprint_hash_mix_u32(&hash, type->index_count);
	for (uint32_t i = 0; i < index_count; ++i) {
		const struct prototype_context* index =
			prototype_context_get(contexts, index_path[i]);
		uint32_t index_classifier = prototype_context_classifier_term(index);
		if (!index || index_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		type_representation_fingerprint_hash_mix_tag(&hash, 0x696e6478U);
		if (type_representation_fingerprint_term_at_depth(
				terms,
				db,
				type_id,
				index_classifier,
				&env,
				p_key,
				&hash,
				0
			) != 0 || type_representation_fingerprint_env_push(
				&env, index->binding_id
			) != 0) {
			return -1;
		}
		p_key->bound_binder_count++;
	}

	uint32_t parameter_binder_count = type->parameter_count;
	type_representation_fingerprint_hash_mix_u32(&hash, type->constructor_count);
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
		type_representation_fingerprint_hash_mix_tag(&hash, 0x636f6e73U);
		type_representation_fingerprint_hash_mix_u32(&hash, constructor->constructor_index);
		type_representation_fingerprint_hash_mix_u32(&hash, field_count);
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field =
				prototype_context_get(contexts, field_path[j]);
			uint32_t field_classifier =
				prototype_context_classifier_term(field);
			if (!field || field_classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			type_representation_fingerprint_hash_mix_tag(&hash, 0x6669656cU);
			if (type_representation_fingerprint_term_at_depth(
					terms,
					db,
					type_id,
					field_classifier,
					&env,
					p_key,
					&hash,
					0
				) != 0 ||
				type_representation_fingerprint_env_push(&env, field->binding_id) != 0) {
				return -1;
			}
			p_key->bound_binder_count++;
		}
		type_representation_fingerprint_hash_mix_tag(&hash, 0x72657375U);
		if (
			type_representation_fingerprint_term_at_depth(
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

int prototype_type_representation_fingerprints_equal(
	const struct prototype_type_representation_fingerprint* left,
	const struct prototype_type_representation_fingerprint* right
) {
	return left && right &&
		left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->parameter_count == right->parameter_count &&
		left->index_count == right->index_count &&
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

int prototype_type_declaration_instance_info(
	const struct prototype_type_declaration_db* db,
	const struct prototype_term_db* terms,
	uint32_t instance,
	uint32_t* p_type_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
) {
	if (!db || !terms || !p_type_id || !p_argument_count ||
		instance >= terms->term_count ||
		(argument_capacity > 0 && !arguments)) {
		return -1;
	}
	uint32_t named_arguments[16];
	uint32_t named_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			instance,
			p_type_id,
			named_arguments,
			&named_argument_count
		) == 0) {
		if (*p_type_id >= db->type_count ||
			named_argument_count > argument_capacity) {
			return -1;
		}
		for (uint32_t i = 0; i < named_argument_count; ++i) {
			arguments[i] = named_arguments[i];
		}
		*p_argument_count = named_argument_count;
		return 0;
	}

	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = instance;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16 || count >= argument_capacity) {
			return -1;
		}
		reversed[count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER ||
		prototype_type_declaration_representation_type_id(
			db,
			terms->terms[current].as.type_former.representation_id,
			p_type_id
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_argument_count = count;
	return 0;
}

int prototype_type_view_declaration_query(
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	uint32_t type_view,
	uint32_t* p_type_id,
	const struct prototype_type_declaration** p_declaration
) {
	if (!db || !contexts || !terms || !p_type_id || !p_declaration ||
		type_view >= terms->term_count ||
		terms->terms[type_view].tag != PROTOTYPE_TERM_TYPE_VIEW ||
		prototype_constructor_telescopes_validate(db, contexts, terms) != 0) {
		return -1;
	}
	uint32_t type_id = terms->terms[type_view].as.type_view.view_type_id;
	if (type_id >= db->type_count ||
		!type_declaration_present(&db->type_declarations[type_id])) {
		return -1;
	}
	const struct prototype_type_declaration* declaration =
		&db->type_declarations[type_id];
	if (declaration->type_index != type_id ||
		declaration->namespace_symbol_id !=
			terms->terms[type_view].as.type_view.identity.namespace_symbol_id ||
		declaration->name_symbol_id !=
			terms->terms[type_view].as.type_view.identity.name_symbol_id) {
		return -1;
	}
	*p_type_id = type_id;
	*p_declaration = declaration;
	return 0;
}

int prototype_type_view_constructor_telescope_query(
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	uint32_t type_view,
	uint32_t constructor_ordinal,
	const struct prototype_type_constructor_declaration** p_constructor
) {
	uint32_t type_id;
	const struct prototype_type_declaration* declaration;
	if (!p_constructor ||
		prototype_type_view_declaration_query(
			db,
			contexts,
			terms,
			type_view,
			&type_id,
			&declaration
		) != 0 || constructor_ordinal >= declaration->constructor_count ||
		declaration->first_constructor + constructor_ordinal >=
			db->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&db->constructor_declarations[
			declaration->first_constructor + constructor_ordinal
		];
	if (!constructor_declaration_present(constructor) ||
		constructor->owner_type != type_id ||
		constructor->constructor_index != constructor_ordinal) {
		return -1;
	}
	*p_constructor = constructor;
	return 0;
}

int prototype_type_declaration_rebuild_representations(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts
) {
	if (!terms || !db || !contexts) {
		return -1;
	}
	db->representation_count = 0;
	if (db->type_count == 0) {
		db->representations_dirty = 0;
		return 0;
	}
	if (!db->representations || db->representation_capacity < db->type_count) {
		return -1;
	}
	for (uint32_t type_id = 0; type_id < db->type_count; ++type_id) {
		if (!type_declaration_present(&db->type_declarations[type_id])) {
			continue;
		}
		struct prototype_type_representation_fingerprint fingerprint;
		if (prototype_type_declaration_representation_fingerprint(
				terms, db, contexts, type_id, &fingerprint
			) != 0) {
			fprintf(
				stderr,
				"type representation rebuild failed for type %u origin %d carrier %u\n",
				type_id,
				db->type_declarations[type_id].origin_kind,
				db->type_declarations[type_id].origin_source_carrier_term_id
			);
			return -1;
		}
		uint32_t representation_id = PROTOTYPE_INVALID_ID;
		for (uint32_t candidate_id = 0; candidate_id < db->representation_count; ++candidate_id) {
			const struct prototype_type_representation* candidate =
				&db->representations[candidate_id];
			if (!prototype_type_representation_fingerprints_equal(&fingerprint, &candidate->fingerprint)) {
				continue;
			}
			struct representation_compare_env env;
			memset(&env, 0, sizeof(env));
			env.left_contexts = contexts;
			env.right_contexts = contexts;
			if (representation_types_equal_at_depth(
					terms,
					db,
					type_id,
					terms,
					db,
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

int prototype_type_declaration_representations_equal(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	const struct prototype_context_db* left_contexts,
	uint32_t left_type_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	const struct prototype_context_db* right_contexts,
	uint32_t right_type_id,
	int* p_equal
) {
	if (!left_terms || !left_db || !left_contexts || !right_terms ||
		!right_db || !right_contexts || !p_equal) {
		return -1;
	}
	struct representation_compare_env env;
	memset(&env, 0, sizeof(env));
	env.left_contexts = left_contexts;
	env.right_contexts = right_contexts;
	*p_equal = representation_types_equal_at_depth(
		left_terms,
		left_db,
		left_type_id,
		right_terms,
		right_db,
		right_type_id,
		&env,
		0
	);
	return 0;
}
