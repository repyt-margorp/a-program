#include "mockup.h"
#include <stdlib.h>

/* Hand-authored, source-specific C transcription of the actual admitted
	* sorted-proof-provider.p definitions. It retains proof data and indices.
	* No alternate array partition/sorter or general lowering is substituted. */
struct qs_nat_type { const char *name; };
struct qs_lt_relation { const struct qs_nat_type *domain; const char *name; };
static const struct qs_nat_type nat_type = {"Nat"};
static const struct qs_lt_relation lt_relation = {&nat_type,"LT"};

enum qs_list_tag { QS_NIL, QS_CONS };
struct qs_list {
	const struct qs_nat_type *element_type;
	enum qs_list_tag tag;
	uint32_t head;
	const struct qs_list *tail;
};
struct qs_sized_list {
	const struct qs_nat_type *element_type;
	enum qs_list_tag tag;
	uint32_t index, tail_size, head;
	const struct qs_sized_list *tail;
};
struct qs_measured { uint32_t size; const struct qs_sized_list *values; };
struct qs_allocation { struct qs_allocation *next; };
struct qs_arena {
	struct qs_allocation *first;
	size_t count, depth;
	int status;
	struct qs_trace trace;
};

static void *allocate(struct qs_arena *a, size_t size)
{
	if (a->status) return NULL;
	if (a->count == 65536 || size > SIZE_MAX - sizeof(struct qs_allocation)) { a->status = 3; return NULL; }
	struct qs_allocation *p = malloc(sizeof(*p) + size);
	if (!p) { a->status = 3; return NULL; }
	p->next = a->first; a->first = p; ++a->count; return p + 1;
}

static int enter(struct qs_arena *a)
{
	if (a->status) return 0;
	if (a->depth == 256) { a->status = 4; return 0; }
	++a->depth; return 1;
}

/* Target metadata consistency, not source conversion or proof admission. */
static int index_check(struct qs_arena *a, int valid)
{
	++a->trace.index_checks;
	if (!valid && !a->status) a->status = 2;
	return valid && !a->status;
}

static uint32_t succ(struct qs_arena *a, uint32_t n)
{
	if (n == UINT32_MAX) { a->status = 5; return 0; }
	return n + 1;
}

static const struct qs_list *list_nil(struct qs_arena *a, const struct qs_nat_type *type)
{
	struct qs_list *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_list){.element_type = type,.tag = QS_NIL};
	return p;
}

static const struct qs_list *list_cons(struct qs_arena *a, const struct qs_nat_type *type, uint32_t head, const struct qs_list *tail)
{
	if (!index_check(a,tail && tail->element_type == type)) return NULL;
	struct qs_list *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_list){type,QS_CONS,head,tail};
	return p;
}

static const struct qs_sized_list *sized_nil(struct qs_arena *a, const struct qs_nat_type *type)
{
	struct qs_sized_list *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_sized_list){.element_type = type,.tag = QS_NIL};
	return p;
}

/* SizedList.cons n head tail : SizedList (succ n), source95-98. */
static const struct qs_sized_list *sized_cons(struct qs_arena *a, const struct qs_nat_type *type, uint32_t n, uint32_t head, const struct qs_sized_list *tail)
{
	if (!index_check(a,tail && tail->element_type == type && tail->index == n)) return NULL;
	uint32_t index = succ(a,n); struct qs_sized_list *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_sized_list){type,QS_CONS,index,n,head,tail};
	return p;
}

/* The three actual LT constructors, source60-67; no bounds are erased. */
static const struct qs_lt *lt_step(struct qs_arena *a, uint32_t n)
{
	uint32_t right = succ(a,n); struct qs_lt *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_lt){.tag = QS_LT_STEP,.left = n,.right = right,.fields.step = n};
	return p;
}

static const struct qs_lt *lt_weaken(struct qs_arena *a, uint32_t m, uint32_t n, const struct qs_lt *prior)
{
	if (!index_check(a,prior && prior->left == m && prior->right == n)) return NULL;
	uint32_t right = succ(a,n); struct qs_lt *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_lt){.tag = QS_LT_WEAKEN_RIGHT,.left = m,.right = right,.fields.weaken_right = {m,n,prior}};
	return p;
}

static const struct qs_lt *lt_lift(struct qs_arena *a, uint32_t m, uint32_t n, const struct qs_lt *prior)
{
	if (!index_check(a,prior && prior->left == m && prior->right == n)) return NULL;
	uint32_t left = succ(a,m), right = succ(a,n); struct qs_lt *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_lt){.tag = QS_LT_LIFT,.left = left,.right = right,.fields.lift = {m,n,prior}};
	return p;
}

static const struct qs_acc *call_raw_down(struct qs_arena *a, const struct qs_acc *proof, uint32_t y, const struct qs_lt *edge)
{
	if (!index_check(a,proof && proof->domain == &nat_type && proof->relation == &lt_relation &&
		proof->subject == proof->current && edge && edge->left == y && edge->right == proof->current && proof->down.call)) return NULL;
	return proof->down.call(a,proof->down.context,y,edge);
}

static const struct qs_acc *zero_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	(void)context; (void)y; (void)edge;
	/* No admitted LT y zero constructor can reach this source83-91 branch.
		* Foreign malformed metadata refuses; no runtime proof is fabricated. */
	a->status = 2; return NULL;
}

/* accessibleSucc's down closure captures the entire original proof. The
	* raw/folded distinction is visible in the weaken/lift clauses, source73-79. */
static const struct qs_acc *successor_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct qs_acc *proof = context, *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,proof && edge && edge->left == y && edge->right == succ(a,proof->current))) goto done;
	switch (edge->tag) {
	case QS_LT_STEP:
		++a->trace.raw_down_step;
		if (index_check(a,edge->fields.step == proof->current && y == proof->current)) result = proof;
		break;
	case QS_LT_WEAKEN_RIGHT:
		++a->trace.raw_down_weaken;
		if (index_check(a,edge->fields.weaken_right.m == y && edge->fields.weaken_right.n == proof->current))
			result = call_raw_down(a,proof,y,edge->fields.weaken_right.prior);
		break;
	case QS_LT_LIFT:
		++a->trace.raw_down_lift;
		if (index_check(a,edge->fields.lift.n == proof->current && succ(a,edge->fields.lift.m) == y)) {
			const struct qs_acc *prior = call_raw_down(a,proof,edge->fields.lift.m,edge->fields.lift.prior);
			if (prior) result = qs_accessible_succ(a,edge->fields.lift.m,prior);
		}
		break;
	default: a->status = 2;
	}
done:
	--a->depth; return result;
}

const struct qs_acc *qs_accessible_succ(struct qs_arena *a, uint32_t n, const struct qs_acc *proof)
{
	if (!index_check(a,proof && proof->domain == &nat_type && proof->relation == &lt_relation && proof->subject == n && proof->current == n)) return NULL;
	uint32_t index = succ(a,proof->current); struct qs_acc *p = allocate(a,sizeof(*p));
	if (p) *p = (struct qs_acc){&nat_type,&lt_relation,index,index,{successor_down,proof}};
	return p;
}

const struct qs_acc *qs_nat_accessible(struct qs_arena *a, uint32_t n)
{
	const struct qs_acc *result = NULL; if (!enter(a)) return NULL;
	if (!n) {
		struct qs_acc *p = allocate(a,sizeof(*p));
		if (p) *p = (struct qs_acc){&nat_type,&lt_relation,0,0,{zero_down,NULL}};
		result = p;
	} else {
		const struct qs_acc *prior = qs_nat_accessible(a,n-1);
		if (prior) result = qs_accessible_succ(a,n-1,prior);
	}
	--a->depth; return result;
}

/* measure folds List, retaining measured n and SizedList n, source164-170. */
static struct qs_measured measure(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *xs)
{
	struct qs_measured result = {0}; if (!enter(a)) return result;
	if (!index_check(a,xs && xs->element_type == type)) goto done;
	if (xs->tag == QS_NIL) result.values = sized_nil(a,type);
	else if (index_check(a,xs->tag == QS_CONS)) {
		struct qs_measured tail = measure(a,type,xs->tail);
		if (tail.values) { result.size = succ(a,tail.size); result.values = sized_cons(a,type,tail.size,xs->head,tail.values); }
	}
done:
	--a->depth; return result;
}

static int nat_less_or_equal(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)
{
	(void)context; int result = 0; if (!enter(a)) return 0;
	/* Actual Nat fold and recursive predecessor call, source21-28. */
	if (!left) result = 1;
	else if (right) result = nat_less_or_equal(a,NULL,left-1,right-1);
	--a->depth; return result;
}

static const struct qs_partition *partition_lower(struct qs_arena *a, uint32_t head, uint32_t size, const struct qs_partition *p)
{
	if (!index_check(a,p && p->bound == size && p->lower && p->upper && p->lower->index == p->lower_size && p->upper->index == p->upper_size)) return NULL;
	++a->trace.partition_lower;
	uint32_t bound = succ(a,size), lower_size = succ(a,p->lower_size);
	const struct qs_sized_list *lower = sized_cons(a,p->element_type,p->lower_size,head,p->lower);
	const struct qs_lt *lb = lt_lift(a,p->lower_size,bound,p->lower_bound), *ub = lt_weaken(a,p->upper_size,bound,p->upper_bound);
	struct qs_partition *out = allocate(a,sizeof(*out));
	if (out) *out = (struct qs_partition){p->element_type,bound,lower_size,lower,p->upper_size,p->upper,lb,ub};
	return out;
}

static const struct qs_partition *partition_upper(struct qs_arena *a, uint32_t head, uint32_t size, const struct qs_partition *p)
{
	if (!index_check(a,p && p->bound == size && p->lower && p->upper && p->lower->index == p->lower_size && p->upper->index == p->upper_size)) return NULL;
	++a->trace.partition_upper;
	uint32_t bound = succ(a,size), upper_size = succ(a,p->upper_size);
	const struct qs_sized_list *upper = sized_cons(a,p->element_type,p->upper_size,head,p->upper);
	const struct qs_lt *lb = lt_weaken(a,p->lower_size,bound,p->lower_bound), *ub = lt_lift(a,p->upper_size,bound,p->upper_bound);
	struct qs_partition *out = allocate(a,sizeof(*out));
	if (out) *out = (struct qs_partition){p->element_type,bound,p->lower_size,p->lower,upper_size,upper,lb,ub};
	return out;
}

struct partition_capture { const struct qs_nat_type *type; struct qs_compare le; uint32_t pivot; };
static const struct qs_partition *partition_fold(struct qs_arena *a, const struct partition_capture *env, uint32_t size, const struct qs_sized_list *xs)
{
	const struct qs_partition *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,xs && xs->element_type == env->type && xs->index == size && env->le.call)) goto done;
	if (xs->tag == QS_NIL) {
		if (!index_check(a,size == 0)) goto done;
		const struct qs_sized_list *lower = sized_nil(a,env->type), *upper = sized_nil(a,env->type);
		const struct qs_lt *lb = lt_step(a,0), *ub = lt_step(a,0);
		struct qs_partition *p = allocate(a,sizeof(*p));
		if (p) *p = (struct qs_partition){env->type,0,0,lower,0,upper,lb,ub};
		result = p;
	} else if (index_check(a,xs->tag == QS_CONS && succ(a,xs->tail_size) == size)) {
		/* decision precedes *tail; the Fold's recursive result captures le/pivot. */
		int decision = env->le.call(a,env->le.context,xs->head,env->pivot);
		const struct qs_partition *tail = partition_fold(a,env,xs->tail_size,xs->tail);
		if (tail) result = decision ? partition_lower(a,xs->head,xs->tail_size,tail) : partition_upper(a,xs->head,xs->tail_size,tail);
	}
done:
	--a->depth; return result;
}

const struct qs_partition *qs_partition(struct qs_arena *a, const struct qs_nat_type *type, struct qs_compare le, uint32_t pivot, uint32_t size, const struct qs_sized_list *xs)
{
	struct partition_capture capture = {type,le,pivot}; return partition_fold(a,&capture,size,xs);
}

static const struct qs_list *append(struct qs_arena *a, const struct qs_nat_type *type, const struct qs_list *left, const struct qs_list *right)
{
	const struct qs_list *result = NULL; if (!enter(a)) return NULL;
	if (!index_check(a,left && right && left->element_type == type && right->element_type == type)) goto done;
	/* Source175-178: the folded tail is callable and captures head/type;
		* applying it to right retains the original List recursion. */
	if (left->tag == QS_NIL) result = right;
	else if (index_check(a,left->tag == QS_CONS)) {
		const struct qs_list *tail = append(a,type,left->tail,right);
		if (tail) result = list_cons(a,type,left->head,tail);
	}
done:
	--a->depth; return result;
}

struct qs_sort_closure qs_quick_sort_acc(const struct qs_nat_type *type, struct qs_compare le, uint32_t size, const struct qs_acc *access)
{
	/* Acc Fold produces a callable over input, capturing A/le/current/access. */
	return (struct qs_sort_closure){type,le,access,size};
}

struct qs_sort_closure qs_force_down(struct qs_arena *a, const struct qs_folded_down *down, uint32_t y, const struct qs_lt *edge)
{
	++a->trace.folded_down;
	if (!index_check(a,down && edge && edge->left == y && edge->right == down->parent_index && down->original.call)) return (struct qs_sort_closure){0};
	const struct qs_acc *child = down->original.call(a,down->original.context,y,edge);
	if (!index_check(a,child && child->domain == &nat_type && child->relation == &lt_relation && child->subject == y && child->current == y)) return (struct qs_sort_closure){0};
	/* The recursive Fold result retains the same type/comparison capture. */
	return qs_quick_sort_acc(down->element_type,down->comparison,y,child);
}

const struct qs_list *qs_apply_sort(struct qs_arena *a, const struct qs_sort_closure *f, const struct qs_sized_list *input)
{
	const struct qs_list *result = NULL; if (!enter(a)) return NULL;
	const struct qs_acc *access = f->access;
	if (!index_check(a,f->element_type == &nat_type && f->comparison.call && access && access->domain == &nat_type && access->relation == &lt_relation &&
		access->subject == f->input_index && access->current == f->input_index && input && input->element_type == f->element_type && input->index == access->current)) goto done;
	++a->trace.acc_branch;
	struct qs_folded_down down = {f->element_type,f->comparison,access->down,access->current};
	if (input->tag == QS_NIL) {
		if (index_check(a,input->index == 0)) result = list_nil(a,f->element_type);
	} else if (index_check(a,input->tag == QS_CONS && succ(a,input->tail_size) == access->current)) {
		/* Actual quickSortAcc source281-289: partition, both *down calls,
			* then append lowerResult (cons pivot upperResult). */
		const struct qs_partition *p = qs_partition(a,f->element_type,f->comparison,input->head,input->tail_size,input->tail);
		if (!p) goto done;
		struct qs_sort_closure lower_call = qs_force_down(a,&down,p->lower_size,p->lower_bound);
		const struct qs_list *lower_result = a->status ? NULL : qs_apply_sort(a,&lower_call,p->lower);
		struct qs_sort_closure upper_call = qs_force_down(a,&down,p->upper_size,p->upper_bound);
		const struct qs_list *upper_result = a->status ? NULL : qs_apply_sort(a,&upper_call,p->upper);
		if (lower_result && upper_result) {
			const struct qs_list *with_pivot = list_cons(a,f->element_type,input->head,upper_result);
			if (with_pivot) result = append(a,f->element_type,lower_result,with_pivot);
		}
	}
done:
	--a->depth; return result;
}

int qs_mockup_sort(const uint32_t *input, size_t count, uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (count > UINT32_MAX || count == SIZE_MAX) return 6;
	struct qs_arena arena = {0}; const struct qs_list *xs = list_nil(&arena,&nat_type);
	for (size_t i = count; i && !arena.status; --i) xs = list_cons(&arena,&nat_type,input[i-1],xs);
	/* Actual quickSort source295-297; accessibility is constructed and used. */
	struct qs_measured measured = measure(&arena,&nat_type,xs);
	const struct qs_acc *access = measured.values ? qs_nat_accessible(&arena,measured.size) : NULL;
	struct qs_sort_closure call = qs_quick_sort_acc(&nat_type,(struct qs_compare){nat_less_or_equal,NULL},measured.size,access);
	const struct qs_list *result = arena.status ? NULL : qs_apply_sort(&arena,&call,measured.values);
	if (!arena.status && result) {
		/* Atomic finite copy-out after the source-defined computation. */
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
