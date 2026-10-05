/* Preserve the parent map/capture refusal controls against this new module. */
#include <string.h>
#define main c47_map_client_main
#include "../acc_transport/map_client.c"
#undef main

static int c48_map_client_main(void)
{
	assert(!c47_map_client_main());
	struct qs_arena arena={0}; const struct qs_acc *proof=gn_nat_accessible(&arena,4);
	assert(proof && !arena.status);
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	const struct qs_lt *edges[]={lt_step(&arena,4),lt_weaken(&arena,2,4,prior),lt_lift(&arena,2,4,prior)};
	for (size_t branch=0; branch<3; ++branch) {
		struct gd_refinement refinement; const struct qs_lt *edge=edges[branch];
		assert(gd_refine(&arena,proof,edge->left,edge,&refinement) && gd_path_valid(&arena,&refinement));
		assert(refinement.endpoints==ge_boundaries+branch && refinement.endpoints->map==refinement.maps.boundary);
		uint32_t left[2],right[2]; enum ge_kind kind=branch ? GE_LT : GE_ACC;
		assert(ge_indices(&arena,&refinement,0,kind,left) && ge_indices(&arena,&refinement,1,kind,right));
		if (branch) {
			assert(left[0]==2 && left[1]==4 && right[0]==2 && right[1]==4);
			const struct qs_lt *acted=gd_transport_left(&arena,&refinement,prior);
			assert(acted && acted->left==left[0] && acted->right==left[1]);
			const struct qs_lt *wrong=lt_step(&arena,3);
			size_t count=arena.count;
			assert(!gd_transport_left(&arena,&refinement,wrong) && arena.status==2 && arena.count==count);
			arena.status=0;
		} else {
			assert(left[0]==4 && right[0]==4);
			struct gd_quoted_acc quoted; assert(gd_transport_right(&arena,&refinement,proof,&quoted));
			const struct qs_acc *acted=gd_force_acc(&arena,&quoted);
			assert(acted && acted->subject==right[0] && acted->current==right[0]);
		}
		/* Equal machine indices do not authorize a foreign endpoint descriptor. */
		struct gd_refinement changed=refinement; struct ge_boundary foreign=*refinement.endpoints;
		changed.endpoints=&foreign; size_t count=arena.count;
		assert(!gd_path_valid(&arena,&changed) && arena.status==2 && arena.count==count); arena.status=0;
		assert(!ge_indices(&arena,&refinement,0,branch ? GE_ACC : GE_LT,left) && arena.status==2 && arena.count==count);
		arena.status=0;
	}
	assert(!arena.status); clear(&arena);
	puts("Actual Acc/LT endpoint projections: three branches; foreign descriptors/kinds and two wrong LT domains refuse");
	return 0;
}

static int c49_map_client_main(void)
{
	assert(!c48_map_client_main());
	struct qs_arena arena={0}; const struct qs_acc *proof=gn_nat_accessible(&arena,4);
	assert(proof && !arena.status);
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	const struct qs_lt *edges[]={lt_step(&arena,4),lt_weaken(&arena,2,4,prior),lt_lift(&arena,2,4,prior)};
	size_t refusals=0;
	for (size_t branch=0; branch<3; ++branch) {
		struct gd_refinement refinement; const struct qs_lt *edge=edges[branch];
		assert(gd_refine(&arena,proof,edge->left,edge,&refinement) && gd_path_valid(&arena,&refinement));
		const struct gf_frame *frame=refinement.maps.binding;
		assert(frame==gf_frames+branch && frame->map==refinement.maps.boundary && frame->count==frame->map->destination->depth);
		unsigned occupied=0;
		for (size_t i=0; i<frame->count; ++i) {
			assert(frame->bindings[i].slot<frame->count && !(occupied&(1u<<frame->bindings[i].slot)));
			occupied|=1u<<frame->bindings[i].slot;
		}
		assert(occupied==(1u<<frame->count)-1);
		struct gf_frame foreign=*frame; struct gd_refinement changed=refinement; changed.maps.binding=&foreign;
		size_t count=arena.count;
		assert(!gd_path_valid(&arena,&changed) && arena.status==2 && arena.count==count); arena.status=0; ++refusals;
		struct gf_binding bad={0,(enum gf_role)99}; struct gt_value value={.kind=GT_ACC,.value.access=proof};
		assert(!gf_value(&arena,&bad,proof,edge->left,edge,refinement.paths,&value) && arena.status==2 && arena.count==count);
		assert(value.kind==GT_ACC && value.value.access==proof); arena.status=0; ++refusals;
		if (!branch) {
			const enum gf_role absent[]={GF_CONSTRUCTOR_NAT1,GF_CONSTRUCTOR_PRIOR};
			for (size_t i=0; i<2; ++i) {
				bad.role=absent[i];
				assert(!gf_value(&arena,&bad,proof,edge->left,edge,refinement.paths,&value) && arena.status==2 && arena.count==count);
				assert(value.kind==GT_ACC && value.value.access==proof); arena.status=0; ++refusals;
			}
		}
	}
	assert(refusals==8 && !arena.status); clear(&arena);
	puts("Actual source binder frame positions: three branches, eight foreign-frame/unknown-role/unselected-field target refusals");
	return 0;
}

static int c51_map_client_main(void)
{
	assert(!c49_map_client_main());
	struct qs_arena arena={0}; const struct qs_acc *proof=gn_nat_accessible(&arena,4);
	assert(proof && !arena.status);
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	const struct qs_lt *edges[]={lt_step(&arena,4),lt_weaken(&arena,2,4,prior),lt_lift(&arena,2,4,prior)};
	for (size_t branch=0; branch<3; ++branch) {
		struct gd_refinement refinement; const struct qs_lt *edge=edges[branch];
		assert(gd_refine(&arena,proof,edge->left,edge,&refinement) && gd_path_valid(&arena,&refinement));
		const struct ga_boundary *recipe=refinement.recipe;
		assert(recipe==ga_boundaries+branch && recipe->map==refinement.maps.boundary);
		assert(recipe->source==!!branch && recipe->destination==!branch);
		assert(recipe->kind==(branch ? GA_LT : GA_QUOTED_ACC) && recipe->down==(branch ? NULL : &ga_down_recipe));
		struct ga_boundary foreign=*recipe; struct gd_refinement changed=refinement; changed.recipe=&foreign;
		size_t count=arena.count;
		assert(!gd_path_valid(&arena,&changed) && arena.status==2 && arena.count==count); arena.status=0;
	}
	uint32_t values[2]={77,88}; size_t count=arena.count;
	assert(ga_domain_values(&arena,&ga_down_recipe,2,4,values) && values[0]==2 && values[1]==4);
	struct ga_down foreign=ga_down_recipe; values[0]=77; values[1]=88;
	assert(!ga_domain_values(&arena,&foreign,2,4,values) && arena.status==2 && arena.count==count);
	assert(values[0]==77 && values[1]==88); arena.status=0;
	struct ga_operand unknown={(enum ga_role)99,0};
	assert(!ga_nat_value(&arena,&unknown,2,4,values) && arena.status==2 && arena.count==count && values[0]==77); arena.status=0;
	unknown=(struct ga_operand){GA_ARGUMENT,1};
	assert(!ga_nat_value(&arena,&unknown,2,4,values) && arena.status==2 && arena.count==count && values[0]==77); arena.status=0;
	unknown=(struct ga_operand){GA_CAPTURE,1};
	assert(!ga_nat_value(&arena,&unknown,2,4,values) && arena.status==2 && arena.count==count && values[0]==77); arena.status=0;
	assert(ga_nat_value(&arena,&ga_down_recipe.result,2,4,values) && values[0]==2 && arena.count==count);
	clear(&arena); puts("Actual Acc down/action recipe: three directions/domain-result projections and seven foreign-recipe/role/slot refusals");
	return 0;
}

static int c53_map_client_main(void)
{
	assert(!c51_map_client_main());
	struct qs_arena arena={0}; const struct qs_acc *proof=gn_nat_accessible(&arena,4);
	assert(proof && !arena.status);
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	const struct qs_lt *edges[]={lt_step(&arena,4),lt_weaken(&arena,2,4,prior),lt_lift(&arena,2,4,prior)};
	size_t refusals=0;
	for (size_t branch=0; branch<3; ++branch) {
		const struct qs_lt *edge=edges[branch]; const struct gi_constructor *recipe=gi_constructors+branch;
		uint32_t indices[2]; size_t count=arena.count;
		assert(gi_indices(&arena,recipe,edge,indices) && indices[0]==edge->left && indices[1]==edge->right);
		struct gd_refinement refinement;
		assert(gd_refine(&arena,proof,edge->left,edge,&refinement) && refinement.constructor==recipe);
		struct gi_constructor foreign=*recipe; indices[0]=77; indices[1]=88;
		assert(!gi_indices(&arena,&foreign,edge,indices) && arena.status==2 && arena.count==count);
		assert(indices[0]==77 && indices[1]==88); arena.status=0; ++refusals;
		struct gd_refinement changed=refinement; changed.constructor=&foreign;
		assert(!gd_path_valid(&arena,&changed) && arena.status==2 && arena.count==count); arena.status=0; ++refusals;
		uint32_t field=99;
		assert(!gi_nat_field(&arena,edge,recipe->count,&field) && arena.status==2 && arena.count==count && field==99);
		arena.status=0; ++refusals;
		if (branch) {
			assert(!gi_nat_field(&arena,edge,2,&field) && arena.status==2 && arena.count==count && field==99);
			arena.status=0; ++refusals;
		}
		struct qs_lt wrong=*edge;
		if (!branch) ++wrong.fields.step;
		else if (branch==1) ++wrong.fields.weaken_right.m;
		else ++wrong.fields.lift.m;
		changed=(struct gd_refinement){.map_count=999};
		assert(!gd_refine(&arena,proof,wrong.left,&wrong,&changed) && arena.status==2 && arena.count==count && changed.map_count==999);
		arena.status=0; ++refusals;
	}
	struct qs_lt unknown=*edges[0]; unknown.tag=(enum qs_lt_tag)99;
	uint32_t indices[2]={77,88}; size_t count=arena.count;
	assert(!gi_indices(&arena,gi_constructors,&unknown,indices) && arena.status==2 && arena.count==count);
	assert(indices[0]==77 && indices[1]==88); arena.status=0; ++refusals;
	struct gd_refinement changed={.map_count=999};
	assert(!gd_refine(&arena,proof,unknown.left,&unknown,&changed) && arena.status==2 && arena.count==count && changed.map_count==999);
	arena.status=0; ++refusals;
	assert(refusals==16 && !arena.status); clear(&arena);
	puts("Actual LT constructor index recipes: three branches, sixteen foreign-recipe/field/kind/tag/index refusals");
	return 0;
}

static int creation_failure(struct qs_arena *arena, const struct gc_constructor *recipe,
	const struct qs_lt *fields, int expected)
{
	size_t count=arena->count; struct qs_allocation *first=arena->first;
	unsigned char before[sizeof(struct qs_lt)];
	if (fields) memcpy(before,fields,sizeof(before));
	assert(!gc_create(arena,recipe,fields) && arena->status==expected
		&& arena->count==count && arena->first==first);
	if (fields) assert(!memcmp(before,fields,sizeof(before)));
	arena->status=0; return 1;
}
int main(void)
{
	assert(!c53_map_client_main());
	struct qs_arena arena={0};
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	assert(prior && !arena.status && prior->left==2 && prior->right==4);
	struct qs_lt fields[3]={
		{.tag=QS_LT_STEP,.left=77,.right=88,.fields.step=4},
		{.tag=QS_LT_WEAKEN_RIGHT,.left=77,.right=88,.fields.weaken_right={2,4,prior}},
		{.tag=QS_LT_LIFT,.left=77,.right=88,.fields.lift={2,4,prior}},
	};
	const uint32_t expected[3][2]={{4,5},{2,5},{3,5}};
	size_t refusals=0;
	for (size_t branch=0; branch<3; ++branch) {
		const struct gc_constructor *recipe=gc_constructors+branch;
		size_t count=arena.count; const struct qs_lt *created=gc_create(&arena,recipe,fields+branch);
		assert(created && !arena.status && arena.count==count+1 && created->tag==fields[branch].tag
			&& created->left==expected[branch][0] && created->right==expected[branch][1]);
		struct gc_constructor foreign=*recipe;
		refusals+=creation_failure(&arena,&foreign,fields+branch,2);
		const struct qs_lt *kept=prior;
		assert(!gc_prior_field(&arena,fields+branch,0,&kept) && arena.status==2
			&& arena.count==count+1 && kept==prior); arena.status=0; ++refusals;
		arena.status=6; refusals+=creation_failure(&arena,recipe,fields+branch,6);
		count=arena.count; arena.count=65536;
		refusals+=creation_failure(&arena,recipe,fields+branch,3); arena.count=count;
		if (!branch) continue;
		uint32_t indices[2];
		assert(gc_prior_indices(&arena,recipe,fields+branch,indices) && indices[0]==2 && indices[1]==4);
		assert(gc_prior_field(&arena,created,recipe->prior_field,&kept) && kept==prior);
		indices[0]=77; indices[1]=88;
		assert(!gc_prior_indices(&arena,&foreign,fields+branch,indices) && arena.status==2
			&& arena.count==count && indices[0]==77 && indices[1]==88); arena.status=0; ++refusals;
		struct qs_lt changed=fields[branch];
		if (branch==1) changed.fields.weaken_right.prior=NULL; else changed.fields.lift.prior=NULL;
		refusals+=creation_failure(&arena,recipe,&changed,2);
		changed=fields[branch];
		if (branch==1) changed.fields.weaken_right.m=3; else changed.fields.lift.m=3;
		refusals+=creation_failure(&arena,recipe,&changed,2);
		struct qs_lt bad=*prior; bad.tag=(enum qs_lt_tag)99; changed=fields[branch];
		if (branch==1) changed.fields.weaken_right.prior=&bad; else changed.fields.lift.prior=&bad;
		refusals+=creation_failure(&arena,recipe,&changed,2);
		bad=*prior; ++bad.fields.weaken_right.m;
		refusals+=creation_failure(&arena,recipe,&changed,2);
	}
	refusals+=creation_failure(&arena,gc_constructors,NULL,2);
	struct qs_lt unknown=fields[0]; unknown.tag=(enum qs_lt_tag)99;
	refusals+=creation_failure(&arena,gc_constructors,&unknown,2);
	uint32_t indices[2]={77,88}; size_t count=arena.count;
	assert(!gc_prior_indices(&arena,gc_constructors,fields,indices) && arena.status==2
		&& arena.count==count && indices[0]==77 && indices[1]==88); arena.status=0; ++refusals;
	const struct qs_lt *limit=lt_step(&arena,UINT32_MAX-1);
	assert(limit && !arena.status && limit->left==UINT32_MAX-1 && limit->right==UINT32_MAX);
	struct qs_lt overflow[3]={
		{.tag=QS_LT_STEP,.fields.step=UINT32_MAX},
		{.tag=QS_LT_WEAKEN_RIGHT,.fields.weaken_right={UINT32_MAX-1,UINT32_MAX,limit}},
		{.tag=QS_LT_LIFT,.fields.lift={UINT32_MAX-1,UINT32_MAX,limit}},
	};
	for (size_t branch=0; branch<3; ++branch)
		refusals+=creation_failure(&arena,gc_constructors+branch,overflow+branch,5);
	/* A mismatched prior retains metadata status2 before result overflow5. */
	overflow[1].fields.weaken_right.m=3;
	refusals+=creation_failure(&arena,gc_constructors+1,overflow+1,2);
	assert(refusals==29 && !arena.status); clear(&arena);
	puts("Actual LT creation: source-selected prior/result indices,29 metadata/borrowed-input/overflow/allocation/status controls");
	return 0;
}
