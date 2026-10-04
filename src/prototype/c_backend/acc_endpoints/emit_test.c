#define main c44_emitter_main
#include "../acc_capture/emit_test.c"
#undef main
#include "emit.h"

int main(int argc, char **argv)
{
	assert(argc==4 || argc==5); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const char *names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_c_indexed_entry entries[8]={0}; const struct pg_term *types[8];
	for (size_t i=0; i<8; ++i) types[i]=select_subject(p,roots[0],names[i])->core;
	const struct pg_occurrence *root=select_subject(p,roots[0],argv[3]),*reference=select_subject(p,roots[0],"succ_access");
	size_t terms=p->graph.terms.count,objects=p->graph.objects.count;
	size_t proofs=p->typing.proofs.count,occurrences=p->typing.occurrences.count;
	size_t maps=p->typing.context_maps.count,contexts=p->typing.contexts.count;
	struct pg_graph storage; assert(!pg_graph_init(&storage)); FILE *out=fopen(argv[2],"w"); assert(out);
	int prior=argc==5 && !strcmp(argv[4],"prior"); if (prior) assert(fputs("KEPT\n",out)>=0);
	emitting=1;
	for (size_t i=0; i<8; ++i) assert(!pg_c_indexed_view_read(&storage,types[i],&entries[i].view));
	if (argc==5 && !strcmp(argv[4],"wrong_family")) entries[1].view=entries[0].view;
	if (argc==5 && !strcmp(argv[4],"wrong_acc")) entries[3].view=entries[1].view;
	if (argc==5 && !strcmp(argv[4],"wrong_lt")) entries[2].view=entries[1].view;
	int status=pg_c_acc_endpoints_emit(out,&p->typing,root,reference,entries); emitting=0;
	if (prior) assert(status && ftell(out)==5);
	if (status) assert(ftell(out)==(prior ? 5 : 0));
	assert(!fclose(out) && p->graph.terms.count==terms && p->graph.objects.count==objects);
	assert(p->typing.proofs.count==proofs && p->typing.occurrences.count==occurrences);
	assert(p->typing.context_maps.count==maps && p->typing.contexts.count==contexts);
	pg_graph_destroy(&storage); pg_program_destroy(p); return status ? 4 : 0;
}
