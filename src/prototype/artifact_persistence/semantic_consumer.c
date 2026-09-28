#include "typing.h"
#include <assert.h>

/* This downstream reader has no source, scheduler or evidence dependency. */
int inspect_semantic_root(const struct pg_occurrence *root)
{
	if (!root) return 0;
	assert(root->core);
	return 1;
}
