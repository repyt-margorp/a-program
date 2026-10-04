/* Generated actual natAccessible Nat Fold/Acc construction/SEQ/IH call.
	* accessibleSucc and exact unreachable zero-down body remain manual C33.
	* Their checked Identity transports are not erased or lowered here. */
static const struct qs_acc *gn_nat_accessible(struct qs_arena *a, uint32_t n)
{
	const struct qs_acc *result = NULL; if (!enter(a)) return NULL;
	if (!n) {
		struct qs_acc *v0 = allocate(a,sizeof(*v0));
		if (!v0 || a->status) goto done;
		*v0 = (struct qs_acc){&nat_type,&lt_relation,0,0,((struct qs_acc_down){zero_down,NULL})};
		result = v0;
	} else if (n) {
		uint32_t predecessor = n - 1;
		const struct qs_acc *v1 = gn_nat_accessible(a,predecessor);
		if (!v1 || a->status) goto done;
		const struct qs_acc *v2 = qs_accessible_succ(a,predecessor,v1);
		if (!v2 || a->status) goto done;
		result = v2;
	}
done:
	--a->depth; return result;
}
