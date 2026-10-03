/* Inspect the actual private receipt layout, without replacing its declarations.
 * Unused checker functions are discarded by the diagnostic link. */
#include "evidence.c"

#include <stdio.h>

int main(void)
{
	size_t alignment = _Alignof(max_align_t);
	size_t parent = sizeof(struct pg_evidence) + sizeof(struct receipt_selection);
	size_t candidate = sizeof(struct pg_evidence) + sizeof(const struct pg_evidence *);
	printf("receipt_header\tselection\tpointer\tarena_alignment\tparent_aligned\tcandidate_aligned\n");
	printf("%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n", sizeof(struct pg_evidence),
		sizeof(struct receipt_selection), sizeof(const struct pg_evidence *), alignment,
		(parent + alignment - 1) / alignment * alignment,
		(candidate + alignment - 1) / alignment * alignment);
	return 0;
}
