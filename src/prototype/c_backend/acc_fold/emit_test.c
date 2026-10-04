#include "emit.h"
#include "artifact/file.h"
#include <assert.h>
#include <string.h>

static int emitting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_eval_advance(work,budget);
}
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_substitution_advance(work,budget);
}
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_whnf_advance(work,budget);
}
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_typed_query_advance(work,budget);
}

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *selected=pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

int main(int argc, char **argv)
{
	assert(argc==5); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_c_indexed_entry entries[]={{.name="bool"},{.name="nat"},{.name="lt"},{.name="acc"},{.name="sized"},{.name="measured"},{.name="partition"},{.name="list"}};
	const struct pg_term *types[8];
	for (size_t i=0; i<8; ++i) types[i]=select_subject(p,roots[0],names[i])->core;
	const struct pg_occurrence *sort=select_subject(p,roots[0],"sort_acc");
	const struct pg_occurrence *other=select_subject(p,roots[0],"successor_access");
	const struct pg_occurrence *natural=select_subject(p,roots[0],"natural_access");
	const struct pg_occurrence *partition=select_subject(p,roots[0],"partition_nat");
	struct pg_graph storage; assert(!pg_graph_init(&storage));
	size_t terms=p->graph.terms.count, objects=p->graph.objects.count;
	size_t proofs=p->typing.proofs.count, occurrences=p->typing.occurrences.count;
	FILE *source=fopen(argv[2],"w"), *header=fopen(argv[3],"w"), *data=fopen(argv[4],"w"); assert(source && header && data);
	emitting=1;
	for (size_t i=0; i<8; ++i) assert(!pg_c_indexed_view_read(&storage,types[i],&entries[i].view));
	assert(!pg_c_indexed_emit(data,&storage,8,entries));
	assert(!pg_c_acc_fold_emit(source,header,&storage,&p->typing,sort,8,entries));
	FILE *a=tmpfile(), *b=tmpfile(); assert(a && b);
	assert(pg_c_acc_fold_emit(a,b,&storage,&p->typing,other,8,entries));
	assert(pg_c_acc_fold_emit(a,b,&storage,&p->typing,natural,8,entries));
	assert(pg_c_acc_fold_emit(a,b,&storage,&p->typing,partition,8,entries));
	assert(pg_c_acc_fold_emit(a,b,&storage,&p->typing,sort,7,entries));
	assert(!fclose(a) && !fclose(b)); emitting=0;
	assert(p->graph.terms.count==terms && p->graph.objects.count==objects);
	assert(p->typing.proofs.count==proofs && p->typing.occurrences.count==occurrences);
	assert(!fclose(source) && !fclose(header) && !fclose(data));
	pg_graph_destroy(&storage); pg_program_destroy(p);
	puts("Actual quickSortAcc Fold/IH native adapter emitted inertly; supplied clause bodies remain manual"); return 0;
}
