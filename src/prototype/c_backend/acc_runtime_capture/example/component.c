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

/* Concrete LT creation below uses admitted prior/result index recipes. */
static const struct qs_lt *lt_step(struct qs_arena *, uint32_t);
static const struct qs_lt *lt_weaken(struct qs_arena *, uint32_t, uint32_t, const struct qs_lt *);
static const struct qs_lt *lt_lift(struct qs_arena *, uint32_t, uint32_t, const struct qs_lt *);

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
/* Bounded action candidate for the actual three accessibleSucc branches.
	* Only LT-constructor index refinements over the closed Nat representation are
	* represented. It is not generic Identity, source erasure or generated success.
	* Retain matched edge, paths, captured frame and every ordered map image.
	* The source-derived descriptor preserves scope sharing; action interpretation
	* remains manual. Source-derived endpoint operands select the affected indices;
	* frame positions also come from actual admitted binders. Concrete target field storage
	* and role values remain manual. Constructor indices now follow source recipes. Source-derived down/domain/result and field-direction recipes
	* drive this finite action; complete source equivalence is not established. */
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
enum gf_role { GF_PARAMETER, GF_PROOF, GF_CURRENT, GF_RAW_DOWN, GF_FOLDED_IH, GF_ARGUMENT, GF_EDGE,
	GF_CONSTRUCTOR_NAT0, GF_CONSTRUCTOR_NAT1, GF_CONSTRUCTOR_PRIOR, GF_PATH0, GF_PATH1 };
struct gf_binding { size_t slot; enum gf_role role; };
struct gf_frame { const struct gt_boundary *map; size_t count; struct gf_binding bindings[12]; };
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
	const struct gf_frame *binding;
	struct gt_value frame[12],left[10],right[10];
};
enum ge_kind { GE_ACC, GE_LT };
struct ge_endpoint { enum ge_kind kind; size_t count; struct gt_image indices[2]; };
struct ge_boundary { const struct gt_boundary *map; struct ge_endpoint left,right; };
enum ga_role { GA_ARGUMENT, GA_CAPTURE };
struct ga_operand { enum ga_role role; size_t slot; };
struct ga_down { size_t captured_field,down_field; struct ga_operand domain[2],result; };
enum ga_kind { GA_LT, GA_QUOTED_ACC };
struct ga_boundary {
	const struct gt_boundary *map;
	enum ga_kind kind;
	size_t source,destination;
	const struct ga_down *down;
};
enum gi_field_kind { GI_NAT, GI_LT };
struct gi_constructor { size_t count; enum gi_field_kind fields[3]; struct gt_image indices[2]; };
struct gc_constructor {
	const struct gi_constructor *constructor;
	size_t prior_field;
	struct gt_image prior_indices[2];
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
/* Actual admitted endpoint index operands, after the explicit known Nat.succ
	* Match clause. No source equality, cancellation or type evidence is made. */
static const struct ge_boundary ge_boundaries[] = {
	{&gt_boundaries[0],{GE_ACC,1,{{2,0}}},{GE_ACC,1,{{7,0}}}},
	{&gt_boundaries[1],{GE_LT,2,{{7,0},{2,0}}},{GE_LT,2,{{7,0},{8,0}}}},
	{&gt_boundaries[2],{GE_LT,2,{{7,0},{2,0}}},{GE_LT,2,{{7,0},{8,0}}}},
};
/* Actual admitted parameter/Fold/callback/constructor binders locate
	* every frame slot. Role representations remain bounded target choices. */
static const struct gf_frame gf_frames[] = {
	{&gt_boundaries[0],10,{{0,GF_PARAMETER},{1,GF_PROOF},{2,GF_CURRENT},{3,GF_RAW_DOWN},{4,GF_FOLDED_IH},{5,GF_ARGUMENT},{6,GF_EDGE},{7,GF_CONSTRUCTOR_NAT0},{8,GF_PATH0},{9,GF_PATH1}}},
	{&gt_boundaries[1],12,{{0,GF_PARAMETER},{1,GF_PROOF},{2,GF_CURRENT},{3,GF_RAW_DOWN},{4,GF_FOLDED_IH},{5,GF_ARGUMENT},{6,GF_EDGE},{7,GF_CONSTRUCTOR_NAT0},{8,GF_CONSTRUCTOR_NAT1},{9,GF_CONSTRUCTOR_PRIOR},{10,GF_PATH0},{11,GF_PATH1}}},
	{&gt_boundaries[2],12,{{0,GF_PARAMETER},{1,GF_PROOF},{2,GF_CURRENT},{3,GF_RAW_DOWN},{4,GF_FOLDED_IH},{5,GF_ARGUMENT},{6,GF_EDGE},{7,GF_CONSTRUCTOR_NAT0},{8,GF_CONSTRUCTOR_NAT1},{9,GF_CONSTRUCTOR_PRIOR},{10,GF_PATH0},{11,GF_PATH1}}},
};
/* Actual admitted Acc constructor/down classifier: Pi domain is
	* contravariant, pure TOTAL recursive result keeps its argument index.
	* These are bounded target recipes, not source action/Scope evidence. */
static const struct ga_down ga_down_recipe = {0,1,{{GA_ARGUMENT,0},{GA_CAPTURE,0}},{GA_ARGUMENT,0}};
static const struct ga_boundary ga_boundaries[] = {
	{&gt_boundaries[0],GA_QUOTED_ACC,0,1,&ga_down_recipe},
	{&gt_boundaries[1],GA_LT,1,0,NULL},
	{&gt_boundaries[2],GA_LT,1,0,NULL},
};
/* Actual admitted LT constructor field classifiers and ordered result
	* images drive finite Nat index checks; concrete storage remains manual. */
static const struct gi_constructor gi_constructors[] = {
	{1,{GI_NAT},{{0,0},{0,1}}},
	{3,{GI_NAT,GI_NAT,GI_LT},{{0,0},{1,1}}},
	{3,{GI_NAT,GI_NAT,GI_LT},{{0,1},{1,1}}},
};
/* Actual admitted recursive LT field arguments drive creation checks. */
static const struct gc_constructor gc_constructors[] = {
	{gi_constructors+0,SIZE_MAX,{{0,0},{0,0}}},
	{gi_constructors+1,2,{{0,0},{1,0}}},
	{gi_constructors+2,2,{{0,0},{1,0}}},
};

static struct gt_value gt_nat(uint32_t n)
{
	return (struct gt_value){.kind=GT_NAT,.value.natural=n};
}
/* Primitive field extraction follows the existing closed C layout. The admitted
	* constructor recipe selects and classifies fields; no source erasure occurs. */
static int gi_nat_field(struct qs_arena *a, const struct qs_lt *edge, size_t field, uint32_t *out)
{
	if (!index_check(a,edge && (unsigned)edge->tag<3)) return 0;
	const struct gi_constructor *recipe=gi_constructors+edge->tag;
	if (!index_check(a,field<recipe->count && field<3 && recipe->fields[field]==GI_NAT)) return 0;
	uint32_t value;
	switch (edge->tag) {
	case QS_LT_STEP: if (!index_check(a,field==0)) return 0; value=edge->fields.step; break;
	case QS_LT_WEAKEN_RIGHT: if (!index_check(a,field<2)) return 0;
		value=field ? edge->fields.weaken_right.n : edge->fields.weaken_right.m; break;
	case QS_LT_LIFT: if (!index_check(a,field<2)) return 0;
		value=field ? edge->fields.lift.n : edge->fields.lift.m; break;
	default: a->status=2; return 0;
	}
	*out=value; return 1;
}
static int gi_indices(struct qs_arena *a, const struct gi_constructor *recipe,
	const struct qs_lt *edge, uint32_t *out)
{
	if (!index_check(a,edge && (unsigned)edge->tag<3 && recipe==gi_constructors+edge->tag)) return 0;
	uint32_t values[2];
	for (size_t i=0; i<2; ++i) {
		const struct gt_image *image=recipe->indices+i;
		if (!index_check(a,image->successor==0 || image->successor==1)
			|| !gi_nat_field(a,edge,image->slot,values+i)) return 0;
		if (image->successor) { values[i]=succ(a,values[i]); if (a->status) return 0; }
	}
	for (size_t i=0; i<2; ++i) out[i]=values[i];
	return 1;
}
/* Source-selected field arguments validate the recursive prior before allocation.
	* Concrete union storage and borrowed immutable prior lifetime remain target choices.
	* Only the immediate prior constructor is checked, not its complete heap graph. */
static int gc_prior_field(struct qs_arena *a, const struct qs_lt *edge, size_t field,
	const struct qs_lt **out)
{
	if (!index_check(a,edge && (unsigned)edge->tag<3)) return 0;
	const struct gi_constructor *recipe=gi_constructors+edge->tag;
	if (!index_check(a,field<recipe->count && field==2 && recipe->fields[field]==GI_LT)) return 0;
	const struct qs_lt *prior;
	switch (edge->tag) {
	case QS_LT_WEAKEN_RIGHT: prior=edge->fields.weaken_right.prior; break;
	case QS_LT_LIFT: prior=edge->fields.lift.prior; break;
	default: a->status=2; return 0;
	}
	*out=prior; return 1;
}
static int gc_prior_indices(struct qs_arena *a, const struct gc_constructor *recipe,
	const struct qs_lt *fields, uint32_t *out)
{
	if (!index_check(a,fields && (unsigned)fields->tag<3 && recipe==gc_constructors+fields->tag
		&& recipe->constructor==gi_constructors+fields->tag && recipe->prior_field!=SIZE_MAX)) return 0;
	uint32_t values[2];
	for (size_t i=0; i<2; ++i) {
		const struct gt_image *image=recipe->prior_indices+i;
		if (!index_check(a,image->successor==0) || !gi_nat_field(a,fields,image->slot,values+i)) return 0;
	}
	for (size_t i=0; i<2; ++i) out[i]=values[i];
	return 1;
}
static const struct qs_lt *gc_create(struct qs_arena *a, const struct gc_constructor *recipe,
	const struct qs_lt *fields)
{
	if (!index_check(a,fields && (unsigned)fields->tag<3 && recipe==gc_constructors+fields->tag
		&& recipe->constructor==gi_constructors+fields->tag)) return NULL;
	if (recipe->prior_field!=SIZE_MAX) {
		const struct qs_lt *prior; uint32_t expected[2],actual[2];
		if (!gc_prior_field(a,fields,recipe->prior_field,&prior)
			|| !gc_prior_indices(a,recipe,fields,expected)
			|| !index_check(a,prior && prior->left==expected[0] && prior->right==expected[1]
				&& (unsigned)prior->tag<3)) return NULL;
		if (!gi_indices(a,gi_constructors+prior->tag,prior,actual)
			|| !index_check(a,actual[0]==prior->left && actual[1]==prior->right)) return NULL;
	}
	uint32_t indices[2];
	if (!gi_indices(a,recipe->constructor,fields,indices)) return NULL;
	struct qs_lt *p=allocate(a,sizeof(*p));
	if (p) { *p=*fields; p->left=indices[0]; p->right=indices[1]; }
	return p;
}
/* These wrappers assemble only existing concrete field storage. Source recipes
	* select prior/result indices; no source bounds or proof fields are erased. */
static const struct qs_lt *lt_step(struct qs_arena *a, uint32_t n)
{
	const struct qs_lt fields={.tag=QS_LT_STEP,.fields.step=n};
	return gc_create(a,gc_constructors+QS_LT_STEP,&fields);
}
static const struct qs_lt *lt_weaken(struct qs_arena *a, uint32_t m, uint32_t n, const struct qs_lt *prior)
{
	const struct qs_lt fields={.tag=QS_LT_WEAKEN_RIGHT,.fields.weaken_right={m,n,prior}};
	return gc_create(a,gc_constructors+QS_LT_WEAKEN_RIGHT,&fields);
}
static const struct qs_lt *lt_lift(struct qs_arena *a, uint32_t m, uint32_t n, const struct qs_lt *prior)
{
	const struct qs_lt fields={.tag=QS_LT_LIFT,.fields.lift={m,n,prior}};
	return gc_create(a,gc_constructors+QS_LT_LIFT,&fields);
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
static int gf_value(struct qs_arena *a, const struct gf_binding *binding,
	const struct qs_acc *proof, uint32_t y, const struct qs_lt *edge,
	const struct gd_nat_path *paths, struct gt_value *out)
{
	/* These role values are a bounded target choice; only their positions are
		* derived from the admitted source binders. No source typing is issued. */
	struct gt_value value;
	switch (binding->role) {
	case GF_PARAMETER: value=gt_nat(proof->subject); break;
	case GF_PROOF: value=(struct gt_value){.kind=GT_ACC,.value.access=proof}; break;
	case GF_CURRENT: value=gt_nat(proof->current); break;
	case GF_RAW_DOWN: value=(struct gt_value){.kind=GT_DOWN,.value.down=proof->down}; break;
	case GF_FOLDED_IH: value=(struct gt_value){.kind=GT_IH,.value.ih={proof->down,proof->current}}; break;
	case GF_ARGUMENT: value=gt_nat(y); break;
	case GF_EDGE: value=(struct gt_value){.kind=GT_LT,.value.edge=edge}; break;
	case GF_CONSTRUCTOR_NAT0: case GF_CONSTRUCTOR_NAT1: {
		uint32_t n;
		if (!gi_nat_field(a,edge,binding->role==GF_CONSTRUCTOR_NAT1,&n)) return 0;
		value=gt_nat(n); break;
	}
	case GF_CONSTRUCTOR_PRIOR: {
		const struct qs_lt *prior;
		if (!gc_prior_field(a,edge,2,&prior)) return 0;
		value=(struct gt_value){.kind=GT_LT,.value.edge=prior}; break;
	}
	case GF_PATH0: case GF_PATH1:
		value=(struct gt_value){.kind=GT_PATH,.path_slot=binding->slot,
			.value.path=paths[binding->role==GF_PATH1]}; break;
	default: a->status=2; return 0;
	}
	*out=value; return 1;
}
static int gt_bind(struct qs_arena *a, const struct qs_acc *proof, uint32_t y,
	const struct qs_lt *edge, const struct gd_nat_path *paths, struct gt_instance *out)
{
	if ((unsigned)edge->tag>2) { a->status=2; return 0; }
	const struct gt_boundary *b=gt_boundaries+edge->tag;
	if (!index_check(a,b->source && b->destination && b->source->depth==b->count
		&& b->count==(edge->tag==QS_LT_STEP ? 6u : 10u)
		&& b->destination->depth==(edge->tag==QS_LT_STEP ? 10u : 12u))) return 0;
	const struct gf_frame *frame=gf_frames+edge->tag;
	if (!index_check(a,frame->map==b && frame->count==b->destination->depth && frame->count<=12)) return 0;
	struct gt_instance result={.boundary=b,.binding=frame}; unsigned occupied=0;
	for (size_t i=0; i<frame->count; ++i) {
		const struct gf_binding *binding=frame->bindings+i;
		if (!index_check(a,binding->slot<frame->count && !(occupied&(1u<<binding->slot)))) return 0;
		if (binding->role==GF_PATH0 || binding->role==GF_PATH1)
			if (!index_check(a,binding->slot==b->paths[binding->role==GF_PATH1])) return 0;
		if (!gf_value(a,binding,proof,y,edge,paths,result.frame+binding->slot)) return 0;
		occupied|=1u<<binding->slot;
	}
	if (!index_check(a,occupied==(1u<<frame->count)-1)) return 0;
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
	const struct ge_boundary *endpoints;
	const struct ga_boundary *recipe;
	const struct gi_constructor *constructor;
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
	uint32_t indices[2];
	if (!index_check(a,(unsigned)edge->tag<3)
		|| !gi_indices(a,gi_constructors+edge->tag,edge,indices)
		|| !index_check(a,edge->left==indices[0] && edge->right==indices[1])) return 0;
	struct gd_refinement result={.branch=edge->tag,.captured=proof,.edge=edge,
		.paths={{y,indices[0],edge},{edge->right,indices[1],edge}},.map_count=edge->tag==QS_LT_STEP ? 6 : 10,
		.endpoints=ge_boundaries+edge->tag,.recipe=ga_boundaries+edge->tag,
		.constructor=gi_constructors+edge->tag};
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
	if (!index_check(a,r->map_count==expected.map_count && r->maps.boundary==expected.maps.boundary
		&& r->endpoints==expected.endpoints && r->endpoints->map==r->maps.boundary
		&& r->maps.binding==expected.maps.binding && r->recipe==expected.recipe
		&& r->recipe->map==r->maps.boundary && r->constructor==expected.constructor)) return 0;
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
static int ge_indices(struct qs_arena *a, const struct gd_refinement *r,
	int right, enum ge_kind kind, uint32_t *indices)
{
	const struct ge_endpoint *endpoint=right ? &r->endpoints->right : &r->endpoints->left;
	if (!index_check(a,endpoint->kind==kind && endpoint->count==(kind==GE_ACC ? 1u : 2u))) return 0;
	for (size_t i=0; i<endpoint->count; ++i) {
		const struct gt_image *image=endpoint->indices+i;
		if (!index_check(a,image->slot<r->maps.boundary->destination->depth
			&& (image->successor==0 || image->successor==1))) return 0;
		const struct gt_value *value=r->maps.frame+image->slot;
		if (!index_check(a,value->kind==GT_NAT)) return 0;
		indices[i]=image->successor ? succ(a,value->value.natural) : value->value.natural;
		if (a->status) return 0;
	}
	return 1;
}
static int ga_nat_value(struct qs_arena *a, const struct ga_operand *operand,
	uint32_t argument, uint32_t captured, uint32_t *out)
{
	if (!index_check(a,operand && operand->slot==0)) return 0;
	uint32_t value;
	switch (operand->role) {
	case GA_ARGUMENT: value=argument; break;
	case GA_CAPTURE: value=captured; break;
	default: a->status=2; return 0;
	}
	*out=value; return 1;
}
static int ga_domain_values(struct qs_arena *a, const struct ga_down *down,
	uint32_t argument, uint32_t captured, uint32_t *out)
{
	if (!index_check(a,down==&ga_down_recipe && down->captured_field==0 && down->down_field==1)) return 0;
	uint32_t values[2];
	for (size_t i=0; i<2; ++i)
		if (!ga_nat_value(a,down->domain+i,argument,captured,values+i)) return 0;
	for (size_t i=0; i<2; ++i) out[i]=values[i];
	return 1;
}
static const struct qs_lt *gd_transport_left(struct qs_arena *a, const struct gd_refinement *r, const struct qs_lt *prior)
{
	uint32_t left[2],right[2];
	if (!gd_path_valid(a,r) || !index_check(a,r->recipe->kind==GA_LT && !r->recipe->down && prior)
		|| !ge_indices(a,r,(int)r->recipe->destination,GE_LT,left)
		|| !ge_indices(a,r,(int)r->recipe->source,GE_LT,right)
		|| !index_check(a,prior->left==right[0] && prior->right==right[1])) return NULL;
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*prior,*r,prior};
	/* Act into the captured callback domain, preserving constructor fields,
		* witness/refinement and original nominal LT value. No pointer erasure. */
	acted->value.left=left[0]; acted->value.right=left[1];
	return &acted->value;
}
static const struct qs_acc *gd_action_down(struct qs_arena *a, const void *context, uint32_t y, const struct qs_lt *edge)
{
	const struct gd_acc_action *action=context;
	uint32_t source[1],destination[1],caller[2],original[2],result_index;
	if (!action || !gd_path_valid(a,&action->refinement)) return NULL;
	const struct ga_boundary *recipe=action->refinement.recipe;
	if (!index_check(a,recipe->kind==GA_QUOTED_ACC && recipe->down==&ga_down_recipe)
		|| !ge_indices(a,&action->refinement,(int)recipe->source,GE_ACC,source)
		|| !ge_indices(a,&action->refinement,(int)recipe->destination,GE_ACC,destination)
		|| !ga_domain_values(a,recipe->down,y,destination[0],caller)
		|| !ga_domain_values(a,recipe->down,y,source[0],original)
		|| !index_check(a,action->original && action->original->current==source[0]
			&& edge && edge->left==caller[0] && edge->right==caller[1])) return NULL;
	/* Source Pi domain acts in the inverse direction before calling the
		* original down. Preserve the exact nominal edge/refinement/capture. */
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*edge,action->refinement,edge};
	acted->value.left=original[0]; acted->value.right=original[1];
	const struct qs_acc *child=call_raw_down(a,action->original,y,&acted->value);
	if (!child || !ga_nat_value(a,&recipe->down->result,y,source[0],&result_index)
		|| !index_check(a,child->subject==result_index && child->current==result_index)) return NULL;
	return child;
}
static int gd_transport_right(struct qs_arena *a, const struct gd_refinement *r,
	const struct qs_acc *proof, struct gd_quoted_acc *out)
{
	uint32_t left[1];
	if (!gd_path_valid(a,r) || !index_check(a,r->recipe->kind==GA_QUOTED_ACC
			&& r->recipe->down==&ga_down_recipe && proof==r->captured)
		|| !ge_indices(a,r,(int)r->recipe->source,GE_ACC,left)
		|| !index_check(a,proof->subject==left[0] && proof->current==left[0])) return 0;
	/* Source transports a quoted Acc before FORCE; constructing this record
		* does not call the original down or a recursive IH. */
	*out=(struct gd_quoted_acc){*r,proof}; return 1;
}
static const struct qs_acc *gd_force_acc(struct qs_arena *a, const struct gd_quoted_acc *quoted)
{
	uint32_t right[1];
	if (!quoted || !gd_path_valid(a,&quoted->refinement) || !quoted->original
		|| !index_check(a,quoted->refinement.recipe->kind==GA_QUOTED_ACC
			&& quoted->refinement.recipe->down==&ga_down_recipe)
		|| !ge_indices(a,&quoted->refinement,(int)quoted->refinement.recipe->destination,GE_ACC,right)) return NULL;
	struct gd_acc_action *action=allocate(a,sizeof(*action)); struct qs_acc *result=allocate(a,sizeof(*result));
	if (!action || !result) return NULL;
	*action=(struct gd_acc_action){quoted->refinement,quoted->original};
	*result=(struct qs_acc){&nat_type,&lt_relation,right[0],right[0],{gd_action_down,action}};
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
/* Generated actual runtime source Bool capture and both matcher clauses.
	* The field comes from the admitted outer parameter, not a foreign callback. */
struct gruntime_context { unsigned boolean_tag; };
static int gruntime_compare(struct qs_arena *a, const void *context, uint32_t left, uint32_t right)
{
	const struct gruntime_context *capture=context;
	if (!index_check(a,capture && capture->boolean_tag<2)) return 0;
	if (capture->boolean_tag==0) return gc_compare(a,NULL,right,left);
	return gc_compare(a,NULL,left,right);
}

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
