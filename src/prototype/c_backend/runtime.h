#ifndef A_PROGRAM_C_RUNTIME_H
#define A_PROGRAM_C_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#define AP_C_RUNTIME_ABI 2

struct ap_value;
struct ap_env;
struct ap_allocation;
enum ap_projection { AP_VALUE, AP_LEFT, AP_RIGHT, AP_RELATION };
struct ap_runtime {
	struct ap_allocation *allocations;
	jmp_buf failure;
	int status;
};

/* Target-local closures and Oracle realizations, not serialized Core tags. */
enum ap_operation {
	AP_RETURN, AP_THUNK, AP_FORCE, AP_TOTAL_RESULT, AP_FOLD, AP_REQUEST,
	AP_ADD, AP_SUBTRACT, AP_MULTIPLY, AP_NEGATE, AP_DECIMAL,
	AP_CONSTRUCTOR, AP_MATCH, AP_LABEL, AP_ATOM,
	AP_ID_ACTION, AP_ID_FIELD, AP_F_FAMILY, AP_U_FAMILY, AP_PI_FAMILY
};
struct ap_descriptor {
	enum ap_operation operation;
	size_t arity, width, family, position, clause_count;
	const size_t *labels;
	int host_print;
};

struct ap_value *ap_delay(struct ap_runtime *,
	struct ap_value *(*)(struct ap_runtime *, const struct ap_env *, enum ap_projection),
	const struct ap_env *, enum ap_projection);
struct ap_value *ap_function(struct ap_runtime *, size_t,
	struct ap_value *(*)(struct ap_runtime *, const struct ap_env *, enum ap_projection),
	const struct ap_env *, enum ap_projection);
struct ap_value *ap_lookup(struct ap_runtime *, const struct ap_env *, size_t, enum ap_projection);
struct ap_value *ap_action(struct ap_runtime *, struct ap_value *);
struct ap_value *ap_family(struct ap_runtime *, enum ap_operation, struct ap_value *, struct ap_value *);
struct ap_value *ap_apply(struct ap_runtime *, struct ap_value *, struct ap_value *);
struct ap_value *ap_primitive(struct ap_runtime *, const struct ap_descriptor *);
struct ap_value *ap_integer(struct ap_runtime *, uint64_t, size_t);
struct ap_value *ap_text(struct ap_runtime *, const unsigned char *, size_t);
void ap_run(struct ap_runtime *, struct ap_value *, int);
void ap_destroy(struct ap_runtime *);
#endif
