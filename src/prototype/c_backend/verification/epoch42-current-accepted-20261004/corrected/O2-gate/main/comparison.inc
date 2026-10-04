/* Generated actual natLessOrEqual callable Nat Fold/clauses.
	* Same private Bool callback convention:1 true,0 false; status separate. */
struct gc_result { int nonzero; uint32_t predecessor; };
static int gc_apply(struct qs_arena *, const struct gc_result *, uint32_t);
static struct gc_result gc_fold(struct qs_arena *a, uint32_t left)
{
	struct gc_result result = {0}; if (!enter(a)) return result;
	result = (struct gc_result){left != 0,left ? left - 1 : 0};
	--a->depth; return result;
}
static int gc_apply(struct qs_arena *a, const struct gc_result *f, uint32_t right)
{
	int result = 0; if (!enter(a)) return 0;
	if (!index_check(a,f && (f->nonzero == 0 || f->nonzero == 1))) goto done;
	if (!f->nonzero) {
		result = 1;
	} else if (f->nonzero) {
		int v0 = 0;
		if (!right) {
			v0 = 0;
		} else {
			uint32_t v1 = right - 1;
			struct gc_result child2 = gc_fold(a,f->predecessor);
			if (a->status) goto done;
			int v3 = gc_apply(a,&child2,v1);
			if (a->status) goto done;
			v0 = v3;
		}
		result = v0;
	}
done:
	--a->depth; return result;
}
static int gc_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)
{
	(void)context; struct gc_result f = gc_fold(a,left);
	return a->status ? 0 : gc_apply(a,&f,right);
}
