#ifndef A_PROGRAM_PROTOTYPE_CORE_INTRINSIC_H
#define A_PROGRAM_PROTOTYPE_CORE_INTRINSIC_H

#include <stddef.h>
#include <stdint.h>

#define PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY 2

enum prototype_pure_primitive_id {
	PROTOTYPE_PURE_PRIMITIVE_UNKNOWN = 0,
	PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT = 1,
	PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT = 2,
	PROTOTYPE_PURE_PRIMITIVE_INT_ADD = 3,
	PROTOTYPE_PURE_PRIMITIVE_INT_SUB = 4,
	PROTOTYPE_PURE_PRIMITIVE_INT_MUL = 5,
	PROTOTYPE_PURE_PRIMITIVE_INT_NEG = 6,
	PROTOTYPE_PURE_PRIMITIVE_INT64_ADD = 7,
	PROTOTYPE_PURE_PRIMITIVE_INT64_SUB = 8,
	PROTOTYPE_PURE_PRIMITIVE_INT64_MUL = 9,
	PROTOTYPE_PURE_PRIMITIVE_INT64_NEG = 10
};

enum prototype_effect_operation_id {
	PROTOTYPE_EFFECT_OPERATION_UNKNOWN = 0,
	PROTOTYPE_EFFECT_OPERATION_PRINT = 1,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT = 2,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE = 3,
	PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT = 4
};

enum prototype_effect_operation_inner_policy {
	PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE = 0,
	PROTOTYPE_EFFECT_OPERATION_INNER_SCOPED = 1
};

enum prototype_effect_operation_resumption_multiplicity {
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT = 0,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT = 1,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE = 2
};

enum prototype_host_type_id {
	PROTOTYPE_HOST_TYPE_INVALID = 0,
	PROTOTYPE_HOST_TYPE_TEXT = 1,
	PROTOTYPE_HOST_TYPE_INT32 = 2,
	PROTOTYPE_HOST_TYPE_INT64 = 3
};

enum prototype_host_oracle_kind {
	PROTOTYPE_HOST_ORACLE_NONE = 0,
	PROTOTYPE_HOST_ORACLE_PRINT = 1,
	PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT = 2,
	PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT = 3,
	PROTOTYPE_HOST_ORACLE_INT_ADD = 4,
	PROTOTYPE_HOST_ORACLE_INT_SUB = 5,
	PROTOTYPE_HOST_ORACLE_INT_MUL = 6,
	PROTOTYPE_HOST_ORACLE_INT_NEG = 7
};

enum prototype_host_effect_flag {
	PROTOTYPE_HOST_EFFECT_NONE = 0,
	PROTOTYPE_HOST_EFFECT_TERMINAL = 1u << 0
};

struct prototype_core_pure_primitive_declaration {
	int primitive_id;
	uint32_t arity;
	/* Host representation needed by the machine reducer, not a Layer T
	 * classifier. INVALID denotes a result reconstructed by another rule. */
	int result_host_type;
};

struct prototype_core_effect_operation_declaration {
	int operation_id;
	unsigned required_host_effects;
	uint32_t arity;
	int inner_policy;
	int resumption_multiplicity;
};

const struct prototype_core_pure_primitive_declaration*
prototype_core_pure_primitive_declaration(int primitive_id);
const struct prototype_core_effect_operation_declaration*
prototype_core_effect_operation_declaration(int operation_id);
uint64_t prototype_core_intrinsic_fingerprint(void);

#endif
