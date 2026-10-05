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
#include "transport.inc"

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
	case GF_CONSTRUCTOR_PRIOR:
		if (!index_check(a,(unsigned)edge->tag<3 && gi_constructors[edge->tag].count==3
			&& gi_constructors[edge->tag].fields[2]==GI_LT)) return 0;
		value=(struct gt_value){.kind=GT_LT,.value.edge=edge->tag==QS_LT_LIFT
			? edge->fields.lift.prior : edge->fields.weaken_right.prior}; break;
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
