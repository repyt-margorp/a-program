/* Generated actual measure List Fold, Measured Match and indexed constructors. */
static struct qs_measured gm_fold(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *input)
{
	struct qs_measured result = {0}; if (!enter(a)) return result;
	if (!index_check(a,input && input->element_type == type && type == &nat_type)) goto done;
	if (input->tag == QS_NIL) {
		const struct qs_sized_list *v0 = sized_nil(a,type);
		if (!v0 || a->status) goto done;
		if (!index_check(a,v0->index == 0)) goto done;
		if (!v0 || a->status) goto done;
		if (!index_check(a,v0->element_type == type && v0->index == 0)) goto done;
		struct qs_measured v1 = {0,v0};
		result = v1;
	} else if (input->tag == QS_CONS) {
		struct qs_measured v2 = gm_fold(a,type,input->tail);
		if (!v2.values || a->status) goto done;
		if (!v2.values || a->status) goto done;
		uint32_t v3 = v2.size;
		const struct qs_sized_list *v4 = v2.values;
		uint32_t v5 = succ(a,v3);
		const struct qs_sized_list *v6 = sized_cons(a,type,v3,input->head,v4);
		if (!v6 || a->status) goto done;
		if (!index_check(a,v6->index == v5)) goto done;
		uint32_t v7 = succ(a,v3);
		if (a->status) goto done;
		if (!v6 || a->status) goto done;
		if (!index_check(a,v6->element_type == type && v6->index == v7)) goto done;
		struct qs_measured v8 = {v7,v6};
		result = v8;
	} else { a->status = 2; }
done:
	--a->depth; return result;
}
/* Generated actual quickSort measure/Match/accessibility/Acc application.
	* C33 storage/LT/successor-down transport and array copy-out remain manual. */
static const struct qs_list *go_outer(struct qs_arena *a, const struct qs_nat_type *type, struct qs_compare le, const struct qs_list *input)
{
	const struct qs_list *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,type == &nat_type && le.call && input && input->element_type == type)) goto done;
	struct qs_measured v9 = gm_fold(a,type,input);
	if (!v9.values || a->status) goto done;
	if (!v9.values || a->status) goto done;
	uint32_t v10 = v9.size;
	const struct qs_sized_list *v11 = v9.values;
	const struct qs_acc *v12 = gn_nat_accessible(a,v10);
	if (!v12 || a->status) goto done;
	struct qs_sort_closure call13 = gs_fold(type,le,v10,v12);
	const struct qs_list *v14 = gs_apply(a,&call13,v11);
	if (!v14 || a->status) goto done;
	result = v14;
done:
	--a->depth; return result;
}
