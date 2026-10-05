/* Manual target array boundary for the admitted source Bool parameter.
	* The local context stays alive through both synchronous Acc recursive calls. */
int gs_sort_mode(enum gs_bool_mode mode, const uint32_t *input, size_t count,
	uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (mode!=GS_BOOL_FIRST && mode!=GS_BOOL_SECOND) return 2;
	if (count>UINT32_MAX || count==SIZE_MAX) return 6;
	const struct gruntime_context context={(unsigned)mode};
	struct qs_arena arena={0}; const struct qs_list *xs=list_nil(&arena,&nat_type);
	for (size_t i=count; i && !arena.status; --i) xs=list_cons(&arena,&nat_type,input[i-1],xs);
	const struct qs_list *result=arena.status ? NULL
		: go_outer(&arena,&nat_type,(struct qs_compare){gruntime_compare,&context},xs);
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
