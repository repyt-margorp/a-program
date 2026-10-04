/* Sealed C33 storage/LT/successor/zero-down/measure primitives remain manual. */
#include "../acc_append/module.h"
#include "../acc_quicksort_mockup/mockup.c"
#include "accessibility.inc"
#include "comparison.inc"
#include "partition.inc"
#include "append.inc"

/* Bind call sites without editing sealed generated Acc/partition/append. */
#define qs_partition(...) gp_partition(__VA_ARGS__)
#define append(...) ga_append(__VA_ARGS__)
#include "clause.inc"
#undef append
#undef qs_partition

/* Outer source orchestration/array/copy-out is still explicitly manual. */
int gs_sort(const uint32_t *input, size_t count, uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (count>UINT32_MAX || count==SIZE_MAX) return 6;
	struct qs_arena arena={0}; const struct qs_list *xs=list_nil(&arena,&nat_type);
	for (size_t i=count; i && !arena.status; --i) xs=list_cons(&arena,&nat_type,input[i-1],xs);
	struct qs_measured measured=measure(&arena,&nat_type,xs);
	const struct qs_acc *access=measured.values ? gn_nat_accessible(&arena,measured.size) : NULL;
	struct qs_sort_closure call=gs_fold(&nat_type,(struct qs_compare){gc_compare,NULL},measured.size,access);
	const struct qs_list *result=arena.status ? NULL : gs_apply(&arena,&call,measured.values);
	if (!arena.status && result) {
		size_t n=0;
		for (const struct qs_list *p=result; p && p->tag==QS_CONS; p=p->tail) ++n;
		if (n>capacity) arena.status=6;
		else if (n && !buffer) arena.status=1;
		else {
			const struct qs_list *p=result;
			for (size_t i=0; i<n; ++i) { buffer[i]=p->head; p=p->tail; }
			*written=n;
		}
	}
	if (trace) *trace=arena.trace;
	while (arena.first) { struct qs_allocation *p=arena.first; arena.first=p->next; free(p); }
	return arena.status;
}
