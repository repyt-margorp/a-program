/* Private composed experiment. The included C33 implementation is immutable,
	* manual source-specific support. Its manual qs_apply_sort is not our entry. */
#include "../acc_quicksort_mockup/mockup.c"
#include "clause.inc"

/* Manual array/measure/accessibility/copy-out orchestration remains explicit.
	* Only gs_apply's clause expressions and validated Fold/IH adapter are emitted. */
int gs_sort(const uint32_t *input, size_t count, uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (count > UINT32_MAX || count == SIZE_MAX) return 6;
	struct qs_arena arena = {0}; const struct qs_list *xs = list_nil(&arena,&nat_type);
	for (size_t i = count; i && !arena.status; --i) xs = list_cons(&arena,&nat_type,input[i-1],xs);
	struct qs_measured measured = measure(&arena,&nat_type,xs);
	const struct qs_acc *access = measured.values ? qs_nat_accessible(&arena,measured.size) : NULL;
	struct qs_sort_closure call = gs_fold(&nat_type,(struct qs_compare){nat_less_or_equal,NULL},measured.size,access);
	const struct qs_list *result = arena.status ? NULL : gs_apply(&arena,&call,measured.values);
	if (!arena.status && result) {
		size_t length = 0;
		for (const struct qs_list *p = result; p && p->tag == QS_CONS; p = p->tail) ++length;
		if (length > capacity) arena.status = 6;
		else if (length && !buffer) arena.status = 1;
		else {
			const struct qs_list *p = result;
			for (size_t i = 0; i < length; ++i) { buffer[i] = p->head; p = p->tail; }
			*written = length;
		}
	}
	if (trace) *trace = arena.trace;
	while (arena.first) { struct qs_allocation *p = arena.first; arena.first = p->next; free(p); }
	return arena.status;
}
