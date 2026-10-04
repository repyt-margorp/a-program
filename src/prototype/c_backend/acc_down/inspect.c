#include "artifact/file.h"
#include "action.h"
#include "dag.h"
#include <assert.h>
#include <string.h>

/* Read accepted typed owners and direct field/boundary inputs. This neither
	* checks Identity nor treats a raw APP or equal numeric index as admission. */
static int inspecting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *w, uint64_t n) { assert(!inspecting); return __real_pg_eval_advance(w,n); }
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n) { assert(!inspecting); return __real_pg_whnf_advance(w,n); }
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *w, uint64_t n) { assert(!inspecting); return __real_pg_substitution_advance(w,n); }
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *w, uint64_t n) { assert(!inspecting); return __real_pg_typed_query_advance(w,n); }

static int child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s=key;
	if (slot<s->operand_count) { *out=s->operands[slot]; return *out ? 1 : 2; }
	slot-=s->operand_count;
	if (!slot) { *out=s->type; return *out ? 1 : 2; }
	if (slot==1) { *out=s->origin; return *out ? 1 : 2; }
	slot-=2; const struct pg_context_map *const *maps=pg_occurrence_maps(s);
	for (size_t i=0; i<=s->map_count; ++i) {
		const struct pg_context_map *m=i ? maps[i-1] : s->map; if (!m) continue;
		if (slot<m->count) { *out=m->images[slot]; return *out ? 1 : 2; }
		slot-=m->count;
	}
	return 0;
}
static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *s=pg_program_select_name(p,root,token); uint64_t steps;
	assert(s && pg_artifact_revalidate(p,s,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(s); assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}
static const struct pg_occurrence *direct_boundary(const struct pg_occurrence *s, struct pg_identity_boundary *out, size_t *wrappers)
{
	for (*wrappers=0; s && *wrappers<256; ++*wrappers) {
		if (pg_identity_boundary_view(s,out)) return s;
		s=s->origin;
	}
	return NULL;
}
int main(int argc, char **argv)
{
	assert(argc==3); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const struct pg_occurrence *root=select_subject(p,roots[0],argv[2]);
	size_t terms=p->graph.terms.count,objects=p->graph.objects.count,proofs=p->typing.proofs.count,occurrences=p->typing.occurrences.count;
	struct pg_dag dag; assert(!pg_dag_init(&dag,child,NULL) && !pg_dag_add(&dag,root)); inspecting=1;
	size_t right=0,left=0,matched=0,unexposed=0;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; const struct pg_term *family,*value; enum pg_identity_direction direction; int lift;
		if (!pg_identity_field_view(s->core,&family,&value,&direction,&lift)) continue;
		const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(&p->typing,s,proof)))
			if (pg_evidence_rule(proof)==PG_IDENTITY_TRANSPORT || pg_evidence_rule(proof)==PG_IDENTITY_LIFT) break;
		if (!proof) continue;
		assert(pg_evidence_owned_by(proof,&p->typing));
		if (s->operand_count!=2 || s->operands[0]->core!=family || s->operands[1]->core!=value) { ++unexposed; continue; }
		++matched; if (direction==PG_IDENTITY_RIGHT) ++right; else ++left;
		struct pg_identity_boundary boundary; size_t wrappers;
		const struct pg_occurrence *formation=direct_boundary(s->operands[0]->type,&boundary,&wrappers);
		printf("FIELD node%zu %s lift%d context%p family-owner%s value-judgement%d ",n->id,direction==PG_IDENTITY_RIGHT ? "right" : "left",lift,(void *)s->context,
			family->kind==PG_REFERENCE && family->as.reference->owner ? family->as.reference->owner->name : "application",s->operands[1]->judgement);
		if (formation) {
			printf("boundary-wrappers%zu maps%zu/%zu paths%zu\n",wrappers,boundary.left_substitution ? boundary.left_substitution->count : 0,
				boundary.right_substitution ? boundary.right_substitution->count : 0,boundary.path_count);
			for (size_t i=0; i<boundary.path_count; ++i) printf("PATH%zu context%p judgement%d operands%zu maps%zu\n",i,(void *)boundary.paths[i]->context,boundary.paths[i]->judgement,boundary.paths[i]->operand_count,boundary.paths[i]->map_count);
		} else { puts("boundary-not-directly-exposed"); ++unexposed; }
	}
	assert(p->graph.terms.count==terms && p->graph.objects.count==objects && p->typing.proofs.count==proofs && p->typing.occurrences.count==occurrences);
	inspecting=0; printf("OWNED fields%zu right%zu left%zu unexposed%zu INERT terms%zu objects%zu proofs%zu occurrences%zu\n",matched,right,left,unexposed,terms,objects,proofs,occurrences);
	pg_dag_destroy(&dag); pg_program_destroy(p); return ferror(stdout) ? 2 : 0;
}
