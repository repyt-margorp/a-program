/* Generated actual partition Fold and inline source helper expressions.
	* C33 storage/Nat/LT primitives and outer entry remain manual. */
struct gp_capture { const struct qs_nat_type *type; struct qs_compare le; uint32_t pivot; };
struct gp_tail { struct gp_capture capture; uint32_t size; const struct qs_sized_list *values; };
static const struct qs_partition *gp_fold(struct qs_arena *a, const struct gp_capture *capture, uint32_t size, const struct qs_sized_list *input)
{
	const struct qs_partition *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,input && input->element_type == capture->type && input->index == size && capture->le.call)) goto done;
	if (input->tag == QS_NIL) {
		if (!index_check(a,size == 0)) goto done;
		const struct qs_lt * v0 = lt_step(a,0);
		if (!v0 || a->status) goto done;
		const struct qs_lt * v1 = lt_step(a,0);
		if (!v1 || a->status) goto done;
		const struct qs_sized_list * v2 = sized_nil(a,capture->type);
		if (!v2 || a->status) goto done;
		if (!index_check(a,v2->index == 0)) goto done;
		const struct qs_sized_list * v3 = sized_nil(a,capture->type);
		if (!v3 || a->status) goto done;
		if (!index_check(a,v3->index == 0)) goto done;
		struct qs_partition *v4 = allocate(a,sizeof(*v4));
		if (!v4 || a->status) goto done;
		*v4 = (struct qs_partition){capture->type,0,0,v2,0,v3,v1,v0};
		result = v4;
	} else if (input->tag == QS_CONS) {
		if (!index_check(a,succ(a,input->tail_size) == size)) goto done;
		struct gp_tail tail = {*capture,input->tail_size,input->tail};
		int v5 = capture->le.call(a,capture->le.context,input->head,capture->pivot);
		if (a->status) goto done;
		if (!index_check(a,v5 == 0 || v5 == 1)) goto done;
		const struct qs_partition * v6 = gp_fold(a,&tail.capture,tail.size,tail.values);
		if (!v6 || a->status) goto done;
		const struct qs_partition *v7 = NULL;
		if (v5) {
			++a->trace.partition_lower;
			const struct qs_partition *v8 = NULL;
			if (!v6 || a->status) goto done;
			uint32_t v9 = succ(a,input->tail_size);
			const struct qs_lt * v10 = lt_weaken(a,v6->upper_size,v9,v6->upper_bound);
			if (!v10 || a->status) goto done;
			uint32_t v11 = succ(a,input->tail_size);
			const struct qs_lt * v12 = lt_lift(a,v6->lower_size,v11,v6->lower_bound);
			if (!v12 || a->status) goto done;
			uint32_t v13 = succ(a,v6->lower_size);
			const struct qs_sized_list * v14 = sized_cons(a,capture->type,v6->lower_size,input->head,v6->lower);
			if (!v14 || a->status) goto done;
			if (!index_check(a,v14->index == v13)) goto done;
			uint32_t v15 = succ(a,v6->lower_size);
			uint32_t v16 = succ(a,input->tail_size);
			struct qs_partition *v17 = allocate(a,sizeof(*v17));
			if (!v17 || a->status) goto done;
			*v17 = (struct qs_partition){capture->type,v16,v15,v14,v6->upper_size,v6->upper,v12,v10};
			v8 = v17;
			v7 = v8;
		} else {
			++a->trace.partition_upper;
			const struct qs_partition *v18 = NULL;
			if (!v6 || a->status) goto done;
			uint32_t v19 = succ(a,input->tail_size);
			const struct qs_lt * v20 = lt_lift(a,v6->upper_size,v19,v6->upper_bound);
			if (!v20 || a->status) goto done;
			uint32_t v21 = succ(a,input->tail_size);
			const struct qs_lt * v22 = lt_weaken(a,v6->lower_size,v21,v6->lower_bound);
			if (!v22 || a->status) goto done;
			uint32_t v23 = succ(a,v6->upper_size);
			const struct qs_sized_list * v24 = sized_cons(a,capture->type,v6->upper_size,input->head,v6->upper);
			if (!v24 || a->status) goto done;
			if (!index_check(a,v24->index == v23)) goto done;
			uint32_t v25 = succ(a,v6->upper_size);
			uint32_t v26 = succ(a,input->tail_size);
			struct qs_partition *v27 = allocate(a,sizeof(*v27));
			if (!v27 || a->status) goto done;
			*v27 = (struct qs_partition){capture->type,v26,v6->lower_size,v6->lower,v25,v24,v22,v20};
			v18 = v27;
			v7 = v18;
		}
		result = v7;
	} else { a->status = 2; goto done; }
done:
	--a->depth; return result;
}

static const struct qs_partition *gp_partition(struct qs_arena *a, const struct qs_nat_type *type, struct qs_compare le, uint32_t pivot, uint32_t size, const struct qs_sized_list *input)
{
	struct gp_capture capture = {type,le,pivot}; return gp_fold(a,&capture,size,input);
}
