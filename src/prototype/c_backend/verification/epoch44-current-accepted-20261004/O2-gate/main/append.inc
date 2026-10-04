/* Generated actual source append callable Fold/clauses. C33 List/Nat
	* storage primitives remain manual; no generalized closure ABI. */
struct ga_result { const struct qs_nat_type *type; enum qs_list_tag tag; uint32_t head; const struct qs_list *original_tail; };
struct ga_tail { const struct qs_nat_type *type; const struct qs_list *original; };
static const struct qs_list *ga_apply(struct qs_arena *, const struct ga_result *, const struct qs_list *);
static struct ga_result ga_fold(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *left)
{
	struct ga_result result = {0}; if (!enter(a)) return result;
	if (!index_check(a,left && left->element_type == type && (left->tag == QS_NIL || left->tag == QS_CONS))) goto done;
	/* Constructor selection/field capture precedes applying the callable. */
	result = (struct ga_result){type,left->tag,left->tag == QS_CONS ? left->head : 0,left->tag == QS_CONS ? left->tail : NULL};
done:
	--a->depth; return result;
}
static struct ga_result ga_force(struct qs_arena *a, const struct ga_tail *tail)
{
	return ga_fold(a,tail->type,tail->original);
}
static const struct qs_list *ga_apply(struct qs_arena *a, const struct ga_result *f, const struct qs_list *right)
{
	const struct qs_list *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,f && right && right->element_type == f->type)) goto done;
	if (f->tag == QS_NIL) {
		result = right;
	} else if (f->tag == QS_CONS) {
		struct ga_tail tail = {f->type,f->original_tail};
		(void)tail;
		struct ga_result child0 = ga_force(a,&tail);
		if (a->status) goto done;
		const struct qs_list *v1 = ga_apply(a,&child0,right);
		if (!v1 || a->status) goto done;
		const struct qs_list *v2 = list_cons(a,f->type,f->head,v1);
		if (!v2 || a->status) goto done;
		result = v2;
	} else { a->status = 2; goto done; }
done:
	--a->depth; return result;
}
static const struct qs_list *ga_append(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *left, const struct qs_list *right)
{
	struct ga_result f = ga_fold(a,type,left); return a->status ? NULL : ga_apply(a,&f,right);
}
