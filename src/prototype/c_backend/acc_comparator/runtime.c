/* Target bridge for the actual admitted QuickSort comparator parameter.
	* The algorithm, partition, Acc/down indices and recursive captures are in
	* the unchanged source-emitted bodies above. This adapter supplies C storage
	* and a borrowed synchronous code/context, not a substitute sorter. */
static int gs_borrowed_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)
{
	const struct gs_comparator *compare=context;
	int result=compare->call(compare->context,left,right);
	if (!index_check(a,result==0 || result==1)) return 0;
	return result;
}

int gs_sort_with(const struct gs_comparator *compare, const uint32_t *input, size_t count,
	uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!compare || !compare->call || !written || (count && !input)) return 1;
	if (count>UINT32_MAX || count==SIZE_MAX) return 6;
	/* Nested Acc closures borrow this stable copy only within this call. */
	const struct gs_comparator borrowed=*compare;
	struct qs_arena arena={0}; const struct qs_list *xs=list_nil(&arena,&nat_type);
	for (size_t i=count; i && !arena.status; --i) xs=list_cons(&arena,&nat_type,input[i-1],xs);
	const struct qs_list *result=arena.status ? NULL : go_outer(&arena,&nat_type,
		(struct qs_compare){gs_borrowed_compare,&borrowed},xs);
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
