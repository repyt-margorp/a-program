#include "a_program/core/term.h"

#include "a_program/kernel/type_declaration.h"

#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
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
	const char* debug_name;
	int term_tag;
	int type_expr_tag;
	int bit_width;
};

static const struct prototype_host_type_descriptor host_types[] = {
	{
		PROTOTYPE_HOST_TYPE_TEXT,
		"Text",
		PROTOTYPE_TERM_PRIMITIVE_TEXT,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT,
		0
	},
	{
		PROTOTYPE_HOST_TYPE_INT32,
		"Int",
		PROTOTYPE_TERM_PRIMITIVE_INT,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT,
		32
	},
	{
		PROTOTYPE_HOST_TYPE_INT64,
		"Int64",
		PROTOTYPE_TERM_PRIMITIVE_INT64,
		PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64,
		64
	}
};

static const struct prototype_pure_primitive_declaration
pure_primitive_declarations[] = {
	{
			PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT,
			1,
			{ PROTOTYPE_HOST_TYPE_TEXT, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INVALID
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT,
			1,
			{ PROTOTYPE_HOST_TYPE_INVALID, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_TEXT
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT_ADD,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT_SUB,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT_MUL,
			2,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT_NEG,
			1,
			{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INT32
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT64_ADD,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT64_SUB,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT64_MUL,
			2,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
			PROTOTYPE_HOST_TYPE_INT64
	},
	{
			PROTOTYPE_PURE_PRIMITIVE_INT64_NEG,
			1,
			{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INVALID },
			PROTOTYPE_HOST_TYPE_INT64
	}
};

static const struct prototype_effect_operation_declaration
effect_operation_declarations[] = {
	{
		PROTOTYPE_EFFECT_OPERATION_PRINT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT,
		PROTOTYPE_HOST_EFFECT_TERMINAL,
		1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT
	},
	{
		PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT,
		PROTOTYPE_HOST_EFFECT_NONE,
		1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT
	},
	{
		PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT,
		PROTOTYPE_HOST_EFFECT_NONE,
		1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT
	},
	{
		PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT,
		PROTOTYPE_HOST_EFFECT_NONE,
		1,
		PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE,
		PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE
	}
};

static const struct prototype_intrinsic_namespace_binding intrinsic_namespace[] = {
	{ "Text", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE, PROTOTYPE_HOST_TYPE_TEXT },
	{ "Int", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE, PROTOTYPE_HOST_TYPE_INT32 },
	{ "Int32", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE, PROTOTYPE_HOST_TYPE_INT32 },
	{ "Int64", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE, PROTOTYPE_HOST_TYPE_INT64 },
	{ "text_to_nat", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT },
	{ "nat_to_text", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT },
	{ "int_add", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT_ADD },
	{ "int_sub", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT_SUB },
	{ "int_mul", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT_MUL },
	{ "int_neg", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT_NEG },
	{ "int64_add", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT64_ADD },
	{ "int64_sub", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT64_SUB },
	{ "int64_mul", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT64_MUL },
	{ "int64_neg", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
		PROTOTYPE_PURE_PRIMITIVE_INT64_NEG },
	{ "print", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
		PROTOTYPE_EFFECT_OPERATION_PRINT },
	{ "scope_text", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
		PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT },
	{ "scope_text_once", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
		PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE },
	{ "abort_text", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
		PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT },
	{ "return", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN, 0 }
};

static const struct prototype_intrinsic_environment default_intrinsic_environment = {
	.namespace_bindings = intrinsic_namespace,
	.namespace_binding_count = sizeof(intrinsic_namespace) /
		sizeof(intrinsic_namespace[0]),
	.pure_primitives = pure_primitive_declarations,
	.pure_primitive_count = sizeof(pure_primitive_declarations) /
		sizeof(pure_primitive_declarations[0]),
	.effect_operations = effect_operation_declarations,
	.effect_operation_count = sizeof(effect_operation_declarations) /
		sizeof(effect_operation_declarations[0]),
	.default_integer_host_type = PROTOTYPE_HOST_TYPE_INT32
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

int prototype_term_host_type_from_source_name(
	const struct prototype_intrinsic_environment* environment,
	const char* name,
	int* p_type_id
) {
	if (!environment || !name || !p_type_id) {
		return -1;
	}
	struct prototype_intrinsic_namespace_binding binding;
	int status = prototype_intrinsic_namespace_lookup(
		environment, name, &binding
	);
	if (status != 0) {
		return status;
	}
	if (binding.kind != PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE) {
		return 1;
	}
	*p_type_id = binding.target_id;
	return 0;
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

const char* prototype_term_host_type_source_name(
	const struct prototype_intrinsic_environment* environment,
	int type_id
) {
	return prototype_intrinsic_namespace_source_name(
		environment,
		PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE, type_id
	);
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

int prototype_intrinsic_namespace_lookup(
	const struct prototype_intrinsic_environment* environment,
	const char* name,
	struct prototype_intrinsic_namespace_binding* p_binding
) {
	if (!environment || !name || !p_binding ||
		(!environment->namespace_bindings &&
			environment->namespace_binding_count != 0)) {
		return -1;
	}
	for (size_t i = 0; i < environment->namespace_binding_count; ++i) {
		if (strcmp(name, environment->namespace_bindings[i].source_name) == 0) {
			*p_binding = environment->namespace_bindings[i];
			return 0;
		}
	}
	return 1;
}

const char* prototype_intrinsic_namespace_source_name(
	const struct prototype_intrinsic_environment* environment,
	int kind,
	int target_id
) {
	if (!environment || (!environment->namespace_bindings &&
		environment->namespace_binding_count != 0)) {
		return NULL;
	}
	for (size_t i = 0; i < environment->namespace_binding_count; ++i) {
		if (environment->namespace_bindings[i].kind == kind &&
			environment->namespace_bindings[i].target_id == target_id) {
			return environment->namespace_bindings[i].source_name;
		}
	}
	return NULL;
}

const struct prototype_intrinsic_environment*
prototype_default_intrinsic_environment(void) {
	return &default_intrinsic_environment;
}

static void intrinsic_fingerprint_mix_u64(uint64_t* p_hash, uint64_t value) {
	for (unsigned i = 0; i < 8; ++i) {
		*p_hash ^= (value >> (i * 8)) & 0xffu;
		*p_hash *= UINT64_C(1099511628211);
	}
}

static void intrinsic_fingerprint_mix_string(uint64_t* p_hash, const char* value) {
	if (!value) {
		intrinsic_fingerprint_mix_u64(p_hash, UINT64_MAX);
		return;
	}
	for (const unsigned char* cursor = (const unsigned char*)value;
		*cursor != '\0'; ++cursor) {
		*p_hash ^= *cursor;
		*p_hash *= UINT64_C(1099511628211);
	}
	*p_hash ^= 0;
	*p_hash *= UINT64_C(1099511628211);
}

uint64_t prototype_intrinsic_environment_fingerprint(
	const struct prototype_intrinsic_environment* environment
) {
	if (!environment) {
		return 0;
	}
	uint64_t hash = UINT64_C(1469598103934665603);
	intrinsic_fingerprint_mix_u64(
		&hash, (uint64_t)(uint32_t)environment->default_integer_host_type
	);
	intrinsic_fingerprint_mix_u64(&hash, environment->namespace_binding_count);
	for (size_t i = 0; i < environment->namespace_binding_count; ++i) {
		const struct prototype_intrinsic_namespace_binding* binding =
			&environment->namespace_bindings[i];
		intrinsic_fingerprint_mix_string(&hash, binding->source_name);
		intrinsic_fingerprint_mix_u64(&hash, (uint64_t)(uint32_t)binding->kind);
		intrinsic_fingerprint_mix_u64(&hash, (uint64_t)(uint32_t)binding->target_id);
	}
	intrinsic_fingerprint_mix_u64(&hash, environment->pure_primitive_count);
	for (size_t i = 0; i < environment->pure_primitive_count; ++i) {
		const struct prototype_pure_primitive_declaration* declaration =
			&environment->pure_primitives[i];
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->primitive_id
		);
		intrinsic_fingerprint_mix_u64(&hash, declaration->arity);
		for (size_t j = 0; j < PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY; ++j) {
			intrinsic_fingerprint_mix_u64(
				&hash, (uint64_t)(uint32_t)declaration->argument_types[j]
			);
		}
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->result_type
		);
	}
	intrinsic_fingerprint_mix_u64(&hash, environment->effect_operation_count);
	for (size_t i = 0; i < environment->effect_operation_count; ++i) {
		const struct prototype_effect_operation_declaration* declaration =
			&environment->effect_operations[i];
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->operation_id
		);
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->classifier_schema
		);
		intrinsic_fingerprint_mix_u64(&hash, declaration->required_host_effects);
		intrinsic_fingerprint_mix_u64(&hash, declaration->arity);
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->inner_policy
		);
		intrinsic_fingerprint_mix_u64(
			&hash, (uint64_t)(uint32_t)declaration->resumption_multiplicity
		);
	}
	return hash;
}

const struct prototype_pure_primitive_declaration*
prototype_term_pure_primitive_declaration(int primitive_id) {
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

const struct prototype_effect_operation_declaration*
prototype_term_effect_operation_declaration(int operation_id) {
	for (size_t i = 0;
		i < sizeof(effect_operation_declarations) / sizeof(effect_operation_declarations[0]);
		++i) {
		if (effect_operation_declarations[i].operation_id == operation_id) {
			return &effect_operation_declarations[i];
		}
	}
	return NULL;
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
				p_ret->layer = PROTOTYPE_TERM_LAYER_TYPE_FORMER;
					break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
		case PROTOTYPE_TERM_THUNK:
		case PROTOTYPE_TERM_TEXT_LITERAL:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
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
	uint32_t count = 0;
	switch (term->tag) {
		case PROTOTYPE_TERM_APP:
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_TYPE_VIEW:
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			count = 2;
			break;
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_TERM_EFFECT_OPERATION:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
		case PROTOTYPE_TERM_THUNK_TYPE:
		case PROTOTYPE_TERM_RETURN:
		case PROTOTYPE_TERM_THUNK:
		case PROTOTYPE_TERM_FORCE:
			count = 1;
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			count = 3;
			break;
		case PROTOTYPE_TERM_MATCH:
			if (term->as.match.first_case > db->case_count ||
				term->as.match.case_count >
					db->case_count - term->as.match.first_case) {
				return -1;
			}
			count = 1 + term->as.match.case_count;
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (term->as.computation_fold.first_clause >
					db->computation_fold_clause_count ||
				term->as.computation_fold.clause_count >
					db->computation_fold_clause_count -
						term->as.computation_fold.first_clause ||
				term->as.computation_fold.clause_count > (UINT32_MAX - 2) / 2) {
				return -1;
			}
			count = 2 + 2 * term->as.computation_fold.clause_count;
			break;
		case PROTOTYPE_TERM_VAR:
		case PROTOTYPE_TERM_CONSTRUCTOR:
		case PROTOTYPE_TERM_TYPE_FORMER:
		case PROTOTYPE_TERM_TYPE_DECLARATION:
		case PROTOTYPE_TERM_UNIVERSE_VAR:
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_TEXT_LITERAL:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_INT_LITERAL:
		case PROTOTYPE_TERM_EXTERNAL_REF:
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
			break;
		default:
			return -1;
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
	switch (term->tag) {
		case PROTOTYPE_TERM_APP:
			return term_child_result(
				db,
				child_index == 0 ? PROTOTYPE_TERM_CHILD_FUNCTION :
					PROTOTYPE_TERM_CHILD_ARGUMENT,
				0,
				child_index == 0 ? term->as.app.function : term->as.app.argument,
				p_child
			);
		case PROTOTYPE_TERM_LAMBDA:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_BODY, 0, term->as.lambda.body, p_child
			);
		case PROTOTYPE_TERM_PI:
			return term_child_result(
				db,
				child_index == 0 ? PROTOTYPE_TERM_CHILD_DOMAIN :
					PROTOTYPE_TERM_CHILD_CODOMAIN_FAMILY,
				0,
				child_index == 0 ? term->as.pi.domain : term->as.pi.codomain_family,
				p_child
			);
		case PROTOTYPE_TERM_MATCH:
			if (child_index == 0) {
				return term_child_result(
					db, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					term->as.match.scrutinee, p_child
				);
			}
			return term_child_result(
				db,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				child_index - 1,
				db->cases[term->as.match.first_case + child_index - 1].body,
				p_child
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return term_child_result(
				db,
				child_index == 0 ? PROTOTYPE_TERM_CHILD_TYPE_VIEW_CORE :
					PROTOTYPE_TERM_CHILD_TYPE_VIEW_SOURCE,
				0,
				child_index == 0 ? term->as.type_view.core : term->as.type_view.source,
				p_child
			);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
				term->as.induction_hypothesis.argument, p_child
			);
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_EFFECT_OPERATION_CLASSIFIER, 0,
				term->as.effect_operation.classifier, p_child
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return term_child_result(
				db,
				child_index == 0 ? PROTOTYPE_TERM_CHILD_EFFECT_ROW_LEFT :
					PROTOTYPE_TERM_CHILD_EFFECT_ROW_RIGHT,
				0,
				child_index == 0 ? term->as.effect_row_union.left :
					term->as.effect_row_union.right,
				p_child
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_EFFECT_ROW_BODY, 0,
				term->as.effect_row_forall.body, p_child
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_EFFECT_ROW_LATENT, 0,
				term->as.effect_row_operation.latent_row, p_child
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return term_child_result(
				db,
				child_index == 0 ? PROTOTYPE_TERM_CHILD_COMPUTATION_EFFECT_ROW :
					PROTOTYPE_TERM_CHILD_COMPUTATION_RESULT,
				0,
				child_index == 0 ? term->as.computation_type.label :
					term->as.computation_type.result,
				p_child
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_THUNK_TYPE_COMPUTATION, 0,
				term->as.thunk_type.computation, p_child
			);
		case PROTOTYPE_TERM_RETURN:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0,
				term->as.return_term.value, p_child
			);
		case PROTOTYPE_TERM_THUNK:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
				term->as.thunk.computation, p_child
			);
		case PROTOTYPE_TERM_FORCE:
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0,
				term->as.force.value, p_child
			);
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			if (child_index == 0) {
				return term_child_result(
					db, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					term->as.operation_request.operation, p_child
				);
			}
			if (child_index == 1) {
				return term_child_result(
					db, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					term->as.operation_request.argument, p_child
				);
			}
			return term_child_result(
				db, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
				term->as.operation_request.continuation, p_child
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (child_index == 0) {
				return term_child_result(
					db, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					term->as.computation_fold.computation, p_child
				);
			}
			if (child_index == 1) {
				return term_child_result(
					db, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					term->as.computation_fold.return_clause, p_child
				);
			}
			{
				uint32_t clause_offset = (child_index - 2) / 2;
				const struct prototype_computation_fold_clause* clause =
					&db->computation_fold_clauses[
						term->as.computation_fold.first_clause + clause_offset
					];
				return term_child_result(
					db,
					(child_index - 2) % 2 == 0 ?
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION :
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
					clause_offset,
					(child_index - 2) % 2 == 0 ? clause->operation : clause->body,
					p_child
				);
			}
		default:
			return -1;
	}
}

int prototype_term_classifier_view(
	const struct prototype_term_db* db,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret
) {
	if (!db || !p_ret || classifier >= db->term_count) {
		return -1;
	}
	memset(p_ret, 0, sizeof(*p_ret));
	const struct prototype_term* term = &db->terms[classifier];
	if (term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		if (term->as.computation_type.label >= db->term_count ||
			term->as.computation_type.result >= db->term_count) {
			return -1;
		}
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		p_ret->effect_row = term->as.computation_type.label;
		p_ret->result = term->as.computation_type.result;
		return 0;
	}
	/* PI(A, C) classifies a computation-level function. A source function is a
	 * value only after THUNK introduces THUNK_TYPE(PI(A, C)). */
	if (term->tag == PROTOTYPE_TERM_PI) {
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
		p_ret->effect_row = PROTOTYPE_INVALID_ID;
		p_ret->result = classifier;
		return 0;
	}
	if (term->tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		p_ret->category = PROTOTYPE_TERM_CATEGORY_TYPE;
		p_ret->result = classifier;
		return 0;
	}
	p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ret->result = classifier;
	return 0;
}

struct term_scope_env {
	uint32_t left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t count;
	uint32_t frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t frame_count;
	uint32_t visited_frame_left[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t visited_frame_right[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t visited_frame_count;
};

enum prototype_type_view_compare_mode {
	PROTOTYPE_TYPE_VIEW_COMPARE_VIEW = 1,
	PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
	PROTOTYPE_TYPE_VIEW_COMPARE_SOURCE
};

struct canonical_binder_env {
	uint32_t binding_id[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t slot[PROTOTYPE_CANONICAL_BINDER_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
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
	uint64_t binding_scope_key;
	uint64_t ih_scope_clone_key;
};

struct substitution_context {
	const struct substitution_ih_scope_clone* ih_scope_clone;
	const struct substitution_binding_scope* binding_scope;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_binding_replacement* bindings;
	size_t binding_count;
	uint32_t exact_term;
	uint32_t exact_replacement;
	int replace_exact_term;
	struct substitution_memo_entry* memo;
	size_t memo_count;
	size_t memo_capacity;
	uint64_t next_scope_key;
};
