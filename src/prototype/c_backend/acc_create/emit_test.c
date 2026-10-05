#include "emit.h"

/* Run the sealed admission/inertness/refusal harness through the new emitter. */
#define pg_c_acc_index_emit pg_c_acc_create_emit
#include "../acc_indices/emit_test.c"
#undef pg_c_acc_index_emit
