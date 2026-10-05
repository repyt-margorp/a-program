#include "emit.h"
#include "../source_observer/observe.h"
#include "artifact/file.h"
#include <assert.h>
#include <string.h>

static int emitting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *w, uint64_t n) { assert(!emitting); return __real_pg_eval_advance(w,n); }
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n) { assert(!emitting); return __real_pg_whnf_advance(w,n); }
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *w, uint64_t n) { assert(!emitting); return __real_pg_substitution_advance(w,n); }
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *w, uint64_t n) { assert(!emitting); return __real_pg_typed_query_advance(w,n); }

static const struct pg_occurrence *select_subject(struct pg_program *p,
	struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *selected=pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

int main(int argc, char **argv)
{
	assert(argc==4 || argc==5); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_c_indexed_entry entries[8]={0}; const struct pg_term *types[8];
	for (size_t i=0; i<8; ++i) types[i]=select_subject(p,roots[0],names[i])->core;
	int only=argc==5 && (!strcmp(argv[4],"lt_only") || !strcmp(argv[4],"lt_prior"));
	if (only) types[2]=select_subject(p,roots[0],argv[3])->core;
	const struct pg_occurrence *root=select_subject(p,roots[0],only ? "succ_access" : argv[3]),
		*reference=select_subject(p,roots[0],"succ_access");
	struct pg_c_source_snapshot *snapshot=pg_c_source_snapshot_create(&p->graph,&p->typing); assert(snapshot);
	struct pg_graph storage; assert(!pg_graph_init(&storage)); FILE *out=fopen(argv[2],"w"); assert(out);
	int prior=argc==5 && (!strcmp(argv[4],"prior") || !strcmp(argv[4],"lt_prior")); if (prior) assert(fputs("KEPT\n",out)>=0);
	emitting=1;
	for (size_t i=0; i<8; ++i) assert(!pg_c_indexed_view_read(&storage,types[i],&entries[i].view));
	if (argc==5 && !strcmp(argv[4],"wrong_family")) entries[1].view=entries[0].view;
	if (argc==5 && !strcmp(argv[4],"wrong_acc")) entries[3].view=entries[1].view;
	if (argc==5 && !strcmp(argv[4],"wrong_lt")) entries[2].view=entries[1].view;
	if (argc==5 && !strcmp(argv[4],"wrong_domain")) entries[3].view.arguments[0]=(struct pg_c_indexed_operand){types[0],NULL};
	if (argc==5 && !strcmp(argv[4],"wrong_relation")) entries[3].view.arguments[1]=(struct pg_c_indexed_operand){types[1],NULL};
	if (argc==5 && !strcmp(argv[4],"missing_parameters")) entries[3].view.count=0;
	int status=only ? pg_c_lt_index_recipe_emit(out,&storage,entries)
		: pg_c_acc_index_emit(out,&storage,&p->typing,root,reference,entries); emitting=0;
	if (status) assert(ftell(out)==(prior ? 5 : 0));
	assert(!fclose(out) && pg_c_source_snapshot_unchanged(snapshot));
	pg_c_source_snapshot_destroy(snapshot);
	pg_graph_destroy(&storage); pg_program_destroy(p); return status ? 4 : 0;
}
