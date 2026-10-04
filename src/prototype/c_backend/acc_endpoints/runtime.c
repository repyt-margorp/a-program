/* Hand-authored action candidate for the actual three accessibleSucc branches.
	* Only LT-constructor index refinements over the closed Nat representation are
	* represented. It is not generic Identity, source erasure or generated success.
	* Retain matched edge, paths, captured frame and every ordered map image.
	* The source-derived descriptor preserves scope sharing; action interpretation
	* remains manual. Source-derived endpoint operands select the affected indices;
	* equal integers alone cannot build a refinement. */
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
enum ge_kind { GE_ACC, GE_LT };
struct ge_endpoint { enum ge_kind kind; size_t count; struct gt_image indices[2]; };
struct ge_boundary { const struct gt_boundary *map; struct ge_endpoint left,right; };
#include "transport.inc"

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
	const struct ge_boundary *endpoints;
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
		.paths={{y,left,edge},{edge->right,right,edge}},.map_count=edge->tag==QS_LT_STEP ? 6 : 10,
		.endpoints=ge_boundaries+edge->tag};
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
		&& r->endpoints==expected.endpoints && r->endpoints->map==r->maps.boundary)) return 0;
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
static const struct qs_lt *gd_transport_left(struct qs_arena *a, const struct gd_refinement *r, const struct qs_lt *prior)
{
	uint32_t left[2],right[2];
	if (!gd_path_valid(a,r) || !index_check(a,r->branch!=QS_LT_STEP && prior)
		|| !ge_indices(a,r,0,GE_LT,left) || !ge_indices(a,r,1,GE_LT,right)
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
	uint32_t left[1],right[1];
	if (!action || !gd_path_valid(a,&action->refinement)
		|| !ge_indices(a,&action->refinement,0,GE_ACC,left) || !ge_indices(a,&action->refinement,1,GE_ACC,right)
		|| !index_check(a,action->original && action->original->current==left[0]
			&& edge && edge->left==y && edge->right==right[0])) return NULL;
	/* The Acc field is contravariant in its LT argument: carry the same
		* retained path action into the original nominal callback domain. */
	struct gd_lt_action *acted=allocate(a,sizeof(*acted)); if (!acted) return NULL;
	*acted=(struct gd_lt_action){*edge,action->refinement,edge}; acted->value.right=left[0];
	return call_raw_down(a,action->original,y,&acted->value);
}
static int gd_transport_right(struct qs_arena *a, const struct gd_refinement *r,
	const struct qs_acc *proof, struct gd_quoted_acc *out)
{
	uint32_t left[1];
	if (!gd_path_valid(a,r) || !index_check(a,r->branch==QS_LT_STEP && proof==r->captured)
		|| !ge_indices(a,r,0,GE_ACC,left)
		|| !index_check(a,proof->subject==left[0] && proof->current==left[0])) return 0;
	/* Source transports a quoted Acc before FORCE; constructing this record
		* does not call the original down or a recursive IH. */
	*out=(struct gd_quoted_acc){*r,proof}; return 1;
}
static const struct qs_acc *gd_force_acc(struct qs_arena *a, const struct gd_quoted_acc *quoted)
{
	uint32_t right[1];
	if (!quoted || !gd_path_valid(a,&quoted->refinement) || !quoted->original
		|| !ge_indices(a,&quoted->refinement,1,GE_ACC,right)) return NULL;
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
