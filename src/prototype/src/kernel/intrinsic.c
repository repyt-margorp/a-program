#include "a_program/kernel/intrinsic.h"

#include <limits.h>
#include <string.h>

#include "a_program/core/term.h"

static const struct prototype_pure_primitive_declaration
pure_primitive_declarations[] = {
	{ PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT,
		{ PROTOTYPE_HOST_TYPE_TEXT, PROTOTYPE_HOST_TYPE_INVALID },
		PROTOTYPE_HOST_TYPE_INVALID },
	{ PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT,
		{ PROTOTYPE_HOST_TYPE_INVALID, PROTOTYPE_HOST_TYPE_INVALID },
		PROTOTYPE_HOST_TYPE_TEXT },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_ADD,
		{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
		PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_SUB,
		{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
		PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_MUL,
		{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INT32 },
		PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT_NEG,
		{ PROTOTYPE_HOST_TYPE_INT32, PROTOTYPE_HOST_TYPE_INVALID },
		PROTOTYPE_HOST_TYPE_INT32 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_ADD,
		{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
		PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_SUB,
		{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
		PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_MUL,
		{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INT64 },
		PROTOTYPE_HOST_TYPE_INT64 },
	{ PROTOTYPE_PURE_PRIMITIVE_INT64_NEG,
		{ PROTOTYPE_HOST_TYPE_INT64, PROTOTYPE_HOST_TYPE_INVALID },
		PROTOTYPE_HOST_TYPE_INT64 }
};

static const struct prototype_effect_operation_declaration
effect_operation_declarations[] = {
	{ PROTOTYPE_EFFECT_OPERATION_PRINT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT },
	{ PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT },
	{ PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT },
	{ PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT,
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT }
};

static const struct prototype_intrinsic_namespace_binding intrinsic_namespace[] = {
	{ "Text", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE,
		PROTOTYPE_HOST_TYPE_TEXT },
	{ "Int", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE,
		PROTOTYPE_HOST_TYPE_INT32 },
	{ "Int32", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE,
		PROTOTYPE_HOST_TYPE_INT32 },
	{ "Int64", PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE,
		PROTOTYPE_HOST_TYPE_INT64 },
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

static const struct prototype_intrinsic_typing_environment default_intrinsic_environment = {
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

int prototype_intrinsic_namespace_lookup(
	const struct prototype_intrinsic_typing_environment* environment,
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

int prototype_intrinsic_host_type_from_source_name(
	const struct prototype_intrinsic_typing_environment* environment,
	const char* name,
	int* p_type_id
) {
	if (!p_type_id) {
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

const char* prototype_intrinsic_namespace_source_name(
	const struct prototype_intrinsic_typing_environment* environment,
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

const struct prototype_intrinsic_typing_environment*
prototype_default_intrinsic_typing_environment(void) {
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

uint64_t prototype_intrinsic_typing_fingerprint(
	const struct prototype_intrinsic_typing_environment* environment
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
	}
	return hash;
}

const struct prototype_pure_primitive_declaration*
prototype_intrinsic_pure_primitive_declaration(int primitive_id) {
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
prototype_intrinsic_effect_operation_declaration(int operation_id) {
	for (size_t i = 0;
		i < sizeof(effect_operation_declarations) /
			sizeof(effect_operation_declarations[0]);
		++i) {
		if (effect_operation_declarations[i].operation_id == operation_id) {
			return &effect_operation_declarations[i];
		}
	}
	return NULL;
}

int prototype_intrinsic_classifier_has_suspended_argument(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	int* p_has_suspended_argument
) {
	if (!terms || !p_has_suspended_argument || classifier >= terms->term_count) {
		return -1;
	}
	uint32_t body = classifier;
	if (terms->terms[body].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		body = terms->terms[body].as.effect_row_forall.body;
	}
	if (body >= terms->term_count ||
		terms->terms[body].tag != PROTOTYPE_TERM_PI ||
		terms->terms[body].as.pi.domain >= terms->term_count) {
		return -1;
	}
	*p_has_suspended_argument = terms->terms[
		terms->terms[body].as.pi.domain
	].tag == PROTOTYPE_TERM_THUNK_TYPE;
	return 0;
}
