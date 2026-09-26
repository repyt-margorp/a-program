#ifndef A_PROGRAM_PROTOTYPE_KERNEL_INTRINSIC_H
#define A_PROGRAM_PROTOTYPE_KERNEL_INTRINSIC_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/intrinsic.h"

struct prototype_term_db;

enum prototype_effect_operation_classifier_schema {
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_INVALID = 0,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT = 1,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT = 2
};

#define PROTOTYPE_EFFECT_OPERATION_MAX_ARITY 1

struct prototype_pure_primitive_declaration {
	int primitive_id;
	int argument_types[PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY];
	int result_type;
};

struct prototype_effect_operation_declaration {
	int operation_id;
	int classifier_schema;
};

enum prototype_intrinsic_namespace_binding_kind {
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_UNKNOWN = 0,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE = 1,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE = 2,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION = 3,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN = 4
};

struct prototype_intrinsic_namespace_binding {
	const char* source_name;
	int kind;
	int target_id;
};

/* Layer T language interface. Core reduction receives only the operational
 * declarations from core/intrinsic.h. */
struct prototype_intrinsic_typing_environment {
	const struct prototype_intrinsic_namespace_binding* namespace_bindings;
	size_t namespace_binding_count;
	const struct prototype_pure_primitive_declaration* pure_primitives;
	size_t pure_primitive_count;
	const struct prototype_effect_operation_declaration* effect_operations;
	size_t effect_operation_count;
	int default_integer_host_type;
};

int prototype_intrinsic_host_type_from_source_name(
	const struct prototype_intrinsic_typing_environment* environment,
	const char* name,
	int* p_type_id
);
int prototype_intrinsic_namespace_lookup(
	const struct prototype_intrinsic_typing_environment* environment,
	const char* name,
	struct prototype_intrinsic_namespace_binding* p_binding
);
const char* prototype_intrinsic_namespace_source_name(
	const struct prototype_intrinsic_typing_environment* environment,
	int kind,
	int target_id
);
const struct prototype_intrinsic_typing_environment*
prototype_default_intrinsic_typing_environment(void);
uint64_t prototype_intrinsic_typing_fingerprint(
	const struct prototype_intrinsic_typing_environment* environment
);
const struct prototype_pure_primitive_declaration*
prototype_intrinsic_pure_primitive_declaration(int primitive_id);
const struct prototype_effect_operation_declaration*
prototype_intrinsic_effect_operation_declaration(int operation_id);
int prototype_intrinsic_classifier_has_suspended_argument(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	int* p_has_suspended_argument
);

#endif
