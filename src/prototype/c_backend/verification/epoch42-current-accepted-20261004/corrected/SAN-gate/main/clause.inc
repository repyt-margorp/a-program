/* Generated from actual admitted Acc clause terms. C33 representations,
	* accessibility/partition/append/comparison and outer entry remain manual. */
static const struct qs_list *gs_apply(struct qs_arena *, const struct qs_sort_closure *, const struct qs_sized_list *);

static struct qs_sort_closure gs_fold(const struct qs_nat_type *type, struct qs_compare le, uint32_t size, const struct qs_acc *access)
{
	return (struct qs_sort_closure){type,le,access,size};
}

static struct qs_sort_closure gs_force_down(struct qs_arena *a, const struct qs_folded_down *down, uint32_t y, const struct qs_lt *edge)
{
	++a->trace.folded_down;
	if (!index_check(a,down && edge && edge->left == y && edge->right == down->parent_index && down->original.call)) return (struct qs_sort_closure){0};
	const struct qs_acc *child = down->original.call(a,down->original.context,y,edge);
	if (!index_check(a,child && child->domain == &nat_type && child->relation == &lt_relation && child->subject == y && child->current == y)) return (struct qs_sort_closure){0};
	return gs_fold(down->element_type,down->comparison,y,child);
}

static const struct qs_list *gs_apply(struct qs_arena *a, const struct qs_sort_closure *f, const struct qs_sized_list *input)
{
	const struct qs_list *result = NULL; if (!enter(a)) return NULL;
	const struct qs_acc *access = f->access;
	if (!index_check(a,f->element_type == &nat_type && f->comparison.call && access && access->domain == &nat_type && access->relation == &lt_relation &&
		access->subject == f->input_index && access->current == f->input_index && input && input->element_type == f->element_type && input->index == access->current)) goto done;
	++a->trace.acc_branch;
	struct qs_acc_down original = access->down;
	struct qs_folded_down down = {f->element_type,f->comparison,original,access->current};
	(void)down;
	const struct qs_list * v0 = NULL;
	if (!index_check(a,input && input->element_type == f->element_type)) goto done;
	/* Source SizedList Match; extra original-down/IH arguments stay scoped. */
	if (input->tag == QS_NIL) {
		if (!index_check(a,input->index == 0)) goto done;
		const struct qs_list * v1 = list_nil(a,f->element_type);
		if (!v1 || a->status) goto done;
		v0 = v1;
	} else if (input->tag == QS_CONS) {
		if (!index_check(a,succ(a,input->tail_size) == input->index)) goto done;
		const struct qs_partition * v2 = qs_partition(a,f->element_type,f->comparison,input->head,input->tail_size,input->tail);
		if (!v2 || a->status) goto done;
		const struct qs_list * v3 = NULL;
		if (!index_check(a,v2 && v2->element_type == f->element_type)) goto done;
		{ /* Source Partition.parts fields in declaration order. */
			struct qs_sort_closure call4 = gs_force_down(a,&down,v2->lower_size,v2->lower_bound);
			if (a->status) goto done;
			const struct qs_list * v5 = gs_apply(a,&call4,v2->lower);
			if (!v5 || a->status) goto done;
			struct qs_sort_closure call6 = gs_force_down(a,&down,v2->upper_size,v2->upper_bound);
			if (a->status) goto done;
			const struct qs_list * v7 = gs_apply(a,&call6,v2->upper);
			if (!v7 || a->status) goto done;
			const struct qs_list * v8 = list_cons(a,f->element_type,input->head,v7);
			if (!v8 || a->status) goto done;
			const struct qs_list * v9 = append(a,f->element_type,v5,v8);
			if (!v9 || a->status) goto done;
			v3 = v9;
		}
		v0 = v3;
	} else { a->status = 2; goto done; }
	result = v0;
done:
	--a->depth; return result;
}
