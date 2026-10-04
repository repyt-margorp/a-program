/* Fixed actual Acc candidate: source bodies and manual target extent are
	* labeled below. Historical body comments describe their original epoch.
	* This packager does not perform source admission or general lowering. */

/* preamble: target packaging. */
#include "component.h"
#include <stdlib.h>

/* private representations: manual target layout. */
struct qs_arena;
struct qs_nat_type;
struct qs_lt_relation;
struct qs_list;
struct qs_sized_list;
struct qs_acc;

enum qs_lt_tag { QS_LT_STEP, QS_LT_WEAKEN_RIGHT, QS_LT_LIFT };
struct qs_lt {
	enum qs_lt_tag tag;
	uint32_t left, right;
	union {
		uint32_t step;
		struct { uint32_t m, n; const struct qs_lt *prior; } weaken_right;
		struct { uint32_t m, n; const struct qs_lt *prior; } lift;
	} fields;
};

/* Acc Nat LT subject has constructor acc(current, raw_down).
	* Calling the original field returns Acc at the requested smaller index. */
struct qs_acc_down {
	const struct qs_acc *(*call)(struct qs_arena *, const void *, uint32_t, const struct qs_lt *);
	const void *context;
};
struct qs_acc {
	const struct qs_nat_type *domain;
	const struct qs_lt_relation *relation;
	uint32_t subject, current;
	struct qs_acc_down down;
};

struct qs_compare {
	int (*call)(struct qs_arena *, const void *, uint32_t, uint32_t);
	const void *context;
};

/* A Fold over Acc returns a function SizedList Nat current -> List Nat.
	* The recursive field supplies such functions; forcing/applying *down is
	* distinct from calling Acc's original down to obtain a smaller Acc. */
struct qs_sort_closure {
	const struct qs_nat_type *element_type;
	struct qs_compare comparison;
	const struct qs_acc *access;
	uint32_t input_index;
};
struct qs_folded_down {
	const struct qs_nat_type *element_type;
	struct qs_compare comparison;
	struct qs_acc_down original;
	uint32_t parent_index;
};

struct qs_partition {
	const struct qs_nat_type *element_type;
	uint32_t bound;
	uint32_t lower_size;
	const struct qs_sized_list *lower;
	uint32_t upper_size;
	const struct qs_sized_list *upper;
	const struct qs_lt *lower_bound, *upper_bound;
};


/* storage and Nat/LT primitives: manual target primitives; zero-down foreign refusal. */
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


/* scoped actions: manual bounded action/closure representation. */
/* Hand-authored action candidate for the actual three accessibleSucc branches.
	* Only LT-constructor index refinements over the closed Nat representation are
	* represented. It is not generic Identity, source erasure or generated success.
	* Retain matched edge, paths, captured frame and every ordered map image.
	* The source-derived descriptor preserves scope sharing; action interpretation
	* remains manual. Equal integers alone cannot build a refinement. */
struct gd_nat_path { uint32_t left, right; const struct qs_lt *center; };
struct gd_folded_ih { struct qs_acc_down original; uint32_t parent; };
struct gt_scope { size_t identity, depth; };
struct gt_image { size_t slot; int successor; };
struct gt_boundary {
	const struct gt_scope *source,*destination;
	size_t count;
	int direction;
	struct gt_image left[10],right[10];
	size_t paths[2];
};
enum gt_kind { GT_NAT, GT_ACC, GT_DOWN, GT_IH, GT_LT, GT_PATH };
struct gt_value {
	enum gt_kind kind;
	size_t path_slot;
	union {
		uint32_t natural;
		const struct qs_acc *access;
		struct qs_acc_down down;
		struct gd_folded_ih ih;
		const struct qs_lt *edge;
		struct gd_nat_path path;
	} value;
};
struct gt_instance {
	const struct gt_boundary *boundary;
	struct gt_value frame[12],left[10],right[10];
};
/* Actual admitted ordered map images and path slots. Target scope tokens
	* preserve sharing/distinction; complete source context semantics are not
	* rechecked here. Manual bounded action interpretation remains separate. */
static const struct gt_scope gt_scopes[] = {
	{0,6},
	{1,10},
	{2,10},
	{3,12},
	{4,10},
	{5,12},
};
static const struct gt_boundary gt_boundaries[] = {
	{&gt_scopes[0],&gt_scopes[1],6,1,{{0,0},{1,0},{5,0},{7,0},{5,0},{2,1}},{{0,0},{1,0},{5,0},{7,0},{7,0},{7,1}},{8,9}},
	{&gt_scopes[2],&gt_scopes[3],10,0,{{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{5,0},{2,1}},{{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{7,0},{8,1}},{10,11}},
	{&gt_scopes[4],&gt_scopes[5],10,0,{{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{5,0},{2,1}},{{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{7,1},{8,1}},{10,11}},
};

static struct gt_value gt_nat(uint32_t n)
{
	return (struct gt_value){.kind=GT_NAT,.value.natural=n};
}
static int gt_same_value(const struct gt_value *left, const struct gt_value *right)
{
	if (left->kind!=right->kind || left->path_slot!=right->path_slot) return 0;
	switch (left->kind) {
	case GT_NAT: return left->value.natural==right->value.natural;
	case GT_ACC: return left->value.access==right->value.access;
	case GT_DOWN: return left->value.down.call==right->value.down.call && left->value.down.context==right->value.down.context;
	case GT_IH: return left->value.ih.original.call==right->value.ih.original.call
		&& left->value.ih.original.context==right->value.ih.original.context && left->value.ih.parent==right->value.ih.parent;
	case GT_LT: return left->value.edge==right->value.edge;
	case GT_PATH: return left->value.path.left==right->value.path.left && left->value.path.right==right->value.path.right
		&& left->value.path.center==right->value.path.center;
	}
	return 0;
}
static int gt_bind(struct qs_arena *a, const struct qs_acc *proof, uint32_t y,
	const struct qs_lt *edge, const struct gd_nat_path *paths, struct gt_instance *out)
{
	if ((unsigned)edge->tag>2) { a->status=2; return 0; }
	const struct gt_boundary *b=gt_boundaries+edge->tag;
	if (!index_check(a,b->source && b->destination && b->source->depth==b->count
		&& b->count==(edge->tag==QS_LT_STEP ? 6u : 10u)
		&& b->destination->depth==(edge->tag==QS_LT_STEP ? 10u : 12u))) return 0;
	struct gt_instance result={.boundary=b};
	result.frame[0]=gt_nat(proof->subject);
	result.frame[1]=(struct gt_value){.kind=GT_ACC,.value.access=proof};
	result.frame[2]=gt_nat(proof->current);
	result.frame[3]=(struct gt_value){.kind=GT_DOWN,.value.down=proof->down};
	result.frame[4]=(struct gt_value){.kind=GT_IH,.value.ih={proof->down,proof->current}};
	result.frame[5]=gt_nat(y);
	result.frame[6]=(struct gt_value){.kind=GT_LT,.value.edge=edge};
	if (edge->tag==QS_LT_STEP) result.frame[7]=gt_nat(edge->fields.step);
	else {
		uint32_t m=edge->tag==QS_LT_LIFT ? edge->fields.lift.m : edge->fields.weaken_right.m;
		uint32_t n=edge->tag==QS_LT_LIFT ? edge->fields.lift.n : edge->fields.weaken_right.n;
		const struct qs_lt *prior=edge->tag==QS_LT_LIFT ? edge->fields.lift.prior : edge->fields.weaken_right.prior;
		result.frame[7]=gt_nat(m); result.frame[8]=gt_nat(n);
		result.frame[9]=(struct gt_value){.kind=GT_LT,.value.edge=prior};
	}
	for (size_t i=0; i<2; ++i) {
		if (!index_check(a,b->paths[i]<b->destination->depth && b->paths[i]>=8)) return 0;
		result.frame[b->paths[i]]=(struct gt_value){.kind=GT_PATH,.path_slot=b->paths[i],.value.path=paths[i]};
	}
	for (size_t side=0; side<2; ++side) for (size_t i=0; i<b->count; ++i) {
		const struct gt_image *image=(side ? b->right : b->left)+i;
		if (!index_check(a,image->slot<b->destination->depth && (image->successor==0 || image->successor==1))) return 0;
		struct gt_value value=result.frame[image->slot];
		if (image->successor) {
			if (!index_check(a,value.kind==GT_NAT)) return 0;
			value=gt_nat(succ(a,value.value.natural)); if (a->status) return 0;
		}
		(side ? result.right : result.left)[i]=value;
	}
	*out=result; return 1;
}
struct gd_refinement {
	enum qs_lt_tag branch;
	const struct qs_acc *captured;
	const struct qs_lt *edge;
	struct gd_nat_path paths[2];
	size_t map_count;
	struct gt_instance maps;
};
struct gd_lt_action { struct qs_lt value; struct gd_refinement refinement; const struct qs_lt *original; };
struct gd_acc_action { struct gd_refinement refinement; const struct qs_acc *original; };
struct gd_quoted_acc { struct gd_refinement refinement; const struct qs_acc *original; };
struct gd_capture { uint32_t parameter; const struct qs_acc *proof; struct gd_folded_ih ih; };

static const struct qs_acc *gs_accessible_succ(struct qs_arena *, uint32_t, const struct qs_acc *);

static int gd_refine(struct qs_arena *a, const struct qs_acc *proof, uint32_t y,
	const struct qs_lt *edge, struct gd_refinement *out)
{
	if (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation
		&& proof->subject==proof->current && edge && edge->left==y && edge->right==succ(a,proof->current))) return 0;
	uint32_t left,right;
	switch (edge->tag) {
	case QS_LT_STEP: left=edge->fields.step; right=succ(a,left); break;
	case QS_LT_WEAKEN_RIGHT: left=edge->fields.weaken_right.m; right=succ(a,edge->fields.weaken_right.n); break;
	case QS_LT_LIFT: left=succ(a,edge->fields.lift.m); right=succ(a,edge->fields.lift.n); break;
	default: a->status=2; return 0;
	}
	if (!index_check(a,edge->left==left && edge->right==right)) return 0;
	struct gd_refinement result={.branch=edge->tag,.captured=proof,.edge=edge,
		.paths={{y,left,edge},{edge->right,right,edge}},.map_count=edge->tag==QS_LT_STEP ? 6 : 10};
	if (!gt_bind(a,proof,y,edge,result.paths,&result.maps)) return 0;
	*out=result; return 1;
}
static int gd_path_valid(struct qs_arena *a, const struct gd_refinement *r)
{
	/* Check the closed target frame and all retained images, without treating
		* this as source scope checking or an equality decision. */
	if (!r || !r->captured || !r->edge || r->branch!=r->edge->tag) { a->status=2; return 0; }
	struct gd_refinement expected;
	if (!gd_refine(a,r->captured,r->edge->left,r->edge,&expected)) return 0;
	if (!index_check(a,r->map_count==expected.map_count && r->maps.boundary==expected.maps.boundary)) return 0;
	for (size_t i=0; i<2; ++i)
		if (!index_check(a,r->paths[i].left==expected.paths[i].left && r->paths[i].right==expected.paths[i].right
			&& r->paths[i].center==expected.paths[i].center)) return 0;
	for (size_t i=0; i<expected.maps.boundary->destination->depth; ++i)
		if (!index_check(a,gt_same_value(r->maps.frame+i,expected.maps.frame+i))) return 0;
	for (size_t i=0; i<expected.map_count; ++i)
		if (!index_check(a,gt_same_value(r->maps.left+i,expected.maps.left+i)
			&& gt_same_value(r->maps.right+i,expected.maps.right+i))) return 0;
	return 1;
}
static const struct qs_lt *gd_transport_left(struct qs_arena *a, const struct gd_refinement *r, const struct qs_lt *prior)
{
	if (!gd_path_valid(a,r) || !index_check(a,r->branch!=QS_LT_STEP && prior
		&& prior->left==(r->branch==QS_LT_LIFT ? r->edge->fields.lift.m : r->edge->fields.weaken_right.m)
		&& prior->right==(r->branch==QS_LT_LIFT ? r->edge->fields.lift.n : r->edge->fields.weaken_right.n))) return NULL;
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*prior,*r,prior};
	/* Act into the captured callback domain, preserving constructor fields,
		* witness/refinement and original nominal LT value. No pointer erasure. */
	acted->value.right=r->captured->current;
	return &acted->value;
}
static const struct qs_acc *gd_action_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_acc_action *action=context;
	if (!action || !gd_path_valid(a,&action->refinement) || !index_check(a,action->original && edge
		&& edge->left==y && edge->right==action->refinement.paths[0].left)) return NULL;
	/* The Acc field is contravariant in its LT argument: carry the same
		* retained path action into the original nominal callback domain. */
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*edge,action->refinement,edge}; acted->value.right=action->original->current;
	return call_raw_down(a,action->original,y,&acted->value);
}
static int gd_transport_right(struct qs_arena *a, const struct gd_refinement *r,
	const struct qs_acc *proof, struct gd_quoted_acc *out)
{
	if (!gd_path_valid(a,r) || !index_check(a,r->branch==QS_LT_STEP && proof==r->captured
		&& proof->subject==r->edge->fields.step && proof->current==r->edge->fields.step)) return 0;
	/* Source transports a quoted Acc before FORCE; constructing this record
		* does not call the original down or a recursive IH. */
	*out=(struct gd_quoted_acc){*r,proof}; return 1;
}
static const struct qs_acc *gd_force_acc(struct qs_arena *a, const struct gd_quoted_acc *quoted)
{
	if (!quoted || !gd_path_valid(a,&quoted->refinement) || !quoted->original) return NULL;
	struct gd_acc_action *action=allocate(a,sizeof(*action)); struct qs_acc *result=allocate(a,sizeof(*result));
	if (!action || !result) return NULL;
	*action=(struct gd_acc_action){quoted->refinement,quoted->original};
	*result=(struct qs_acc){&nat_type,&lt_relation,quoted->refinement.paths[0].left,
		quoted->refinement.paths[0].left,{gd_action_down,action}};
	return result;
}
static const struct qs_acc *gd_force_ih(struct qs_arena *a, const struct gd_folded_ih *ih, uint32_t y, const struct qs_lt *edge)
{
	if (!index_check(a,ih && ih->original.call && edge && edge->left==y && edge->right==ih->parent)) return NULL;
	const struct qs_acc *child=ih->original.call(a,ih->original.context,y,edge);
	return child && !a->status ? gs_accessible_succ(a,y,child) : NULL;
}

/* successor down and captures: actual source expression emission C43/C44. */
/* Generated actual successor down branch sequencing into manual C42 actions. */
static const struct qs_acc *gs_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_capture *capture=context; const struct qs_acc *result=NULL; if (!enter(a)) return NULL;
	struct gd_refinement refinement;
	if (!capture || !index_check(a,capture->proof && capture->parameter==capture->proof->subject)
		|| !gd_refine(a,capture->proof,y,edge,&refinement)) goto done;
	switch (edge->tag) {
	case QS_LT_STEP: {
		++a->trace.raw_down_step;
		struct gd_quoted_acc v0;
		if (!gd_transport_right(a,&refinement,capture->proof,&v0)) goto done;
		const struct qs_acc *v1 = gd_force_acc(a,&v0);
		if (!v1 || a->status) goto done;
		result=v1; break;
	}
	case QS_LT_WEAKEN_RIGHT: {
		++a->trace.raw_down_weaken;
		const struct qs_lt *v3 = gd_transport_left(a,&refinement,edge->fields.weaken_right.prior);
		if (!v3) goto done;
		const struct qs_acc *v2 = call_raw_down(a,capture->proof,edge->fields.weaken_right.m,v3);
		if (!v2 || a->status) goto done;
		result=v2; break;
	}
	case QS_LT_LIFT: {
		++a->trace.raw_down_lift;
		const struct qs_lt *v5 = gd_transport_left(a,&refinement,edge->fields.lift.prior);
		if (!v5) goto done;
		const struct qs_acc *v4 = gd_force_ih(a,&capture->ih,edge->fields.lift.m,v5);
		if (!v4 || a->status) goto done;
		result=v4; break;
	}
	default: a->status=2;
	}
done:
	--a->depth; return result;
}
/* Generated actual successor Acc Fold/Nat/Acc/captured down construction.
	* Bounded action/closure representation remains an explicit target choice. */
static const struct qs_acc *gs_accessible_succ(struct qs_arena *a, uint32_t n, const struct qs_acc *proof)
{
	if (!index_check(a,proof && proof->domain==&nat_type && proof->relation==&lt_relation && proof->subject==n && proof->current==n)) return NULL;
	const struct qs_acc *result=NULL;
	uint32_t c0=succ(a,proof->current);
	if (a->status) goto done;
	struct gd_capture *c1=allocate(a,sizeof(*c1));
	if (!c1) goto done;
	*c1=(struct gd_capture){n,proof,((struct gd_folded_ih){proof->down,proof->current})};
	struct qs_acc *c2=allocate(a,sizeof(*c2));
	if (!c2) goto done;
	*c2=(struct qs_acc){&nat_type,&lt_relation,c0,c0,((struct qs_acc_down){gs_down,c1})};
	result=c2;
done:
	return result;
}

/* successor adapter: target composition. */
#define qs_accessible_succ(...) gs_accessible_succ(__VA_ARGS__)

/* Nat accessibility: actual source expression emission C39. */
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

/* successor adapter end: target composition. */
#undef qs_accessible_succ

/* comparison: actual source expression emission C41. */
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

/* partition: actual source expression emission C37. */
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

/* append: actual source expression emission C38. */
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

/* partition/append adapters: target composition. */
#define qs_partition(...) gp_partition(__VA_ARGS__)
#define append(...) ga_append(__VA_ARGS__)

/* Acc QuickSort callable Fold: actual source expression emission C36. */
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

/* adapters end: target composition. */
#undef append
#undef qs_partition

/* measure and outer QuickSort: actual source expression emission C40. */
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

/* array boundary: manual target staging/copy-out/lifetime. */
/* Target array staging/copy-out/arena lifetime; source outer calls emitted. */
int gs_sort(const uint32_t *input, size_t count, uint32_t *buffer, size_t capacity, size_t *written, struct qs_trace *trace)
{
	if (!written || (count && !input)) return 1;
	if (count>UINT32_MAX || count==SIZE_MAX) return 6;
	struct qs_arena arena={0}; const struct qs_list *xs=list_nil(&arena,&nat_type);
	for (size_t i=count; i && !arena.status; --i) xs=list_cons(&arena,&nat_type,input[i-1],xs);
	const struct qs_list *result=arena.status ? NULL : go_outer(&arena,&nat_type,(struct qs_compare){gc_compare,NULL},xs);
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
