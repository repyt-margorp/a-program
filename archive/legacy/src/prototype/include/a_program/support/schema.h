#ifndef __PROTOTYPE_SUPPORT_SCHEMA_H__
#define __PROTOTYPE_SUPPORT_SCHEMA_H__

#include <stdint.h>

/* Shared scalar identities do not belong to TermDB or declaration storage. */
#define PROTOTYPE_INVALID_ID UINT32_MAX
#define PROTOTYPE_BASE_NAMESPACE_ID (-1)

/*
 * A qualified source-level address used while a graph reference has not yet
 * been linked to a concrete node. It is not part of core computation identity.
 */
struct prototype_qualified_name {
	int namespace_symbol_id;
	int name_symbol_id;
};

#endif
