#include "emit.h"
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

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *s=pg_program_select_name(p,root,token); uint64_t steps;
	assert(s && pg_artifact_revalidate(p,s,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(s); assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}
int main(int argc, char **argv)
{
	assert(argc==5 || argc==6); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_c_indexed_entry entries[]={{.name="bool"},{.name="nat"},{.name="lt"},{.name="acc"},{.name="sized"},{.name="measured"},{.name="partition"},{.name="list"}};
	const struct pg_term *types[8]; for (size_t i=0; i<8; ++i) types[i]=select_subject(p,roots[0],names[i])->core;
	struct pg_c_acc_measure_sources sources={select_subject(p,roots[0],argv[3]),select_subject(p,roots[0],"measure_fn"),
		select_subject(p,roots[0],argv[4]),select_subject(p,roots[0],"outer_fn"),select_subject(p,roots[0],"measure_generic"),
		select_subject(p,roots[0],"nat_access"),select_subject(p,roots[0],"sort_generic"),select_subject(p,roots[0],"compare_fn")};
	if (argc==6 && !strcmp(argv[5],"same_reference")) { sources.measure_reference=sources.measure; sources.outer_reference=sources.outer; }
	size_t terms=p->graph.terms.count,objects=p->graph.objects.count,proofs=p->typing.proofs.count,occurrences=p->typing.occurrences.count;
	struct pg_graph storage; assert(!pg_graph_init(&storage)); FILE *out=fopen(argv[2],"w"); assert(out);
	int prior=argc==6 && !strcmp(argv[5],"prior"); if (prior) assert(fputs("KEPT\n",out)>=0);
	emitting=1; for (size_t i=0; i<8; ++i) assert(!pg_c_indexed_view_read(&storage,types[i],&entries[i].view));
	if (argc==6 && !strcmp(argv[5],"wrong_family")) entries[4].view=entries[7].view;
	int status=pg_c_acc_measure_emit(out,&storage,&p->typing,&sources,entries); emitting=0;
	if (prior) assert(status && ftell(out)==5);
	assert(!fclose(out) && p->graph.terms.count==terms && p->graph.objects.count==objects && p->typing.proofs.count==proofs && p->typing.occurrences.count==occurrences);
	pg_graph_destroy(&storage); pg_program_destroy(p); return status ? 4 : 0;
}
