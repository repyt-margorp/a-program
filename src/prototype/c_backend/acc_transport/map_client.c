#include "component.c"
#include <assert.h>
#include <stdio.h>

static void clear(struct qs_arena *arena)
{
	while (arena->first) { struct qs_allocation *p=arena->first; arena->first=p->next; free(p); }
}

int main(void)
{
	struct qs_arena arena={0}; const struct qs_acc *proof=gn_nat_accessible(&arena,4);
	assert(proof && !arena.status);
	const struct qs_lt *prior=lt_weaken(&arena,2,3,lt_step(&arena,2));
	const struct qs_lt *edges[]={lt_step(&arena,4),lt_weaken(&arena,2,4,prior),lt_lift(&arena,2,4,prior)};
	size_t perturbations=0;
	for (size_t branch=0; branch<3; ++branch) {
		struct gd_refinement refinement; const struct qs_lt *edge=edges[branch];
		assert(gd_refine(&arena,proof,edge->left,edge,&refinement) && gd_path_valid(&arena,&refinement));
		const struct gt_instance *maps=&refinement.maps; const struct gt_boundary *boundary=maps->boundary;
		assert(boundary==gt_boundaries+branch && boundary->source->depth==refinement.map_count);
		assert(boundary->direction==(branch==0) && boundary->destination->depth==(branch ? 12u : 10u));
		assert(maps->frame[0].value.natural==4 && maps->frame[1].value.access==proof);
		assert(maps->frame[3].value.down.call==proof->down.call && maps->frame[3].value.down.context==proof->down.context);
		assert(maps->frame[4].value.ih.original.call==proof->down.call && maps->frame[4].value.ih.parent==4);
		assert(maps->frame[6].value.edge==edge && boundary->paths[0]!=boundary->paths[1]);
		for (size_t i=0; i<2; ++i) {
			const struct gt_value *path=maps->frame+boundary->paths[i];
			assert(path->kind==GT_PATH && path->path_slot==boundary->paths[i] && path->value.path.center==edge);
		}
		/* Equal closed Nat endpoints still retain distinct path binder slots. */
		assert(maps->left[boundary->count-2].value.natural==maps->right[boundary->count-2].value.natural);
		if (branch) {
			assert(maps->left[3].value.down.call==proof->down.call && maps->right[4].value.ih.parent==4);
			const struct qs_lt *acted=gd_transport_left(&arena,&refinement,prior);
			assert(acted && ((const struct gd_lt_action *)acted)->refinement.maps.boundary==boundary);
		} else {
			struct gd_quoted_acc quoted;
			assert(gd_transport_right(&arena,&refinement,proof,&quoted));
			const struct qs_acc *acted=gd_force_acc(&arena,&quoted);
			assert(acted && ((const struct gd_acc_action *)acted->down.context)->refinement.maps.boundary==boundary);
		}
		for (size_t fault=0; fault<4; ++fault) {
			struct gd_refinement changed=refinement; struct gt_boundary foreign=*boundary;
			if (fault==0) changed.maps.boundary=&foreign;
			if (fault==1) changed.maps.right[0]=gt_nat(99);
			if (fault==2) changed.maps.frame[boundary->paths[0]].path_slot=boundary->paths[1];
			if (fault==3) changed.maps.frame[3].value.down.context=NULL;
			size_t count=arena.count;
			assert(!gd_path_valid(&arena,&changed) && arena.status==2 && arena.count==count);
			arena.status=0; ++perturbations;
		}
	}
	assert(perturbations==12 && !arena.status); clear(&arena);
	puts("Actual ordered map images/captured frame/scope tokens/path binders: three branches, twelve target refusals");
	return 0;
}
