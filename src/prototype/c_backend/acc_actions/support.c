/* Actual-source composition with a labeled manual scoped action candidate. */
#include "../acc_append/module.h"
#include "../acc_quicksort_mockup/mockup.c"
#include "runtime.c"
#include "actions.inc"

/* Target capture construction remains manual; down sequencing now emits. */
static const struct qs_acc *gs_accessible_succ(struct qs_arena *a, uint32_t n, const struct qs_acc *proof)
{
	if (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation && proof->subject==n && proof->current==n)) return NULL;
	uint32_t next=succ(a,proof->current); if (a->status) return NULL;
	struct gd_capture *capture=allocate(a,sizeof(*capture)); struct qs_acc *result=allocate(a,sizeof(*result));
	if (!capture || !result) return NULL;
	*capture=(struct gd_capture){n,proof,{proof->down,proof->current}};
	*result=(struct qs_acc){&nat_type,&lt_relation,next,next,{gs_down,capture}}; return result;
}
#define qs_accessible_succ(...) gs_accessible_succ(__VA_ARGS__)
#include "accessibility.inc"
#undef qs_accessible_succ
#include "comparison.inc"
#include "partition.inc"
#include "append.inc"
#define qs_partition(...) gp_partition(__VA_ARGS__)
#define append(...) ga_append(__VA_ARGS__)
#include "clause.inc"
#undef append
#undef qs_partition
#include "measure.inc"

/* Target array staging/copy-out/arena lifetime; source outer calls emitted. */
int gs_sort(const uint32_t *input, size_t count, uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (count>UINT32_MAX || count==SIZE_MAX) return 6;
	struct qs_arena arena={0}; const struct qs_list *xs=list_nil(&arena,&nat_type);
	for (size_t i=count; i && !arena.status; --i) xs=list_cons(&arena,&nat_type,input[i-1],xs);
	const struct qs_list *result=arena.status ? NULL : go_outer(&arena,&nat_type,(struct qs_compare){gc_compare,NULL},xs);
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
