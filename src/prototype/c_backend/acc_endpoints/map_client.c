/* Preserve the parent map/capture refusal controls against this new module. */
#define main c47_map_client_main
#include "../acc_transport/map_client.c"
#undef main

int main(void)
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
