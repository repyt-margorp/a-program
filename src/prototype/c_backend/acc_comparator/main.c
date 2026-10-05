#define _POSIX_C_SOURCE 200809L
#include "../acc_create/emit.h"
#include "../source_observer/observe.h"
#include "../acc_accessibility/emit.h"
#include "../acc_comparison/emit.h"
#include "../acc_partition/emit.h"
#include "../acc_append/emit.h"
#include "../acc_clause/emit.h"
#include "../acc_measure/emit.h"
#include "artifact/file.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int emitting;
static const char *temporary_directory;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *w, uint64_t n) { assert(!emitting); return __real_pg_eval_advance(w,n); }
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n) { assert(!emitting); return __real_pg_whnf_advance(w,n); }
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *w, uint64_t n) { assert(!emitting); return __real_pg_substitution_advance(w,n); }
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *w, uint64_t n) { assert(!emitting); return __real_pg_typed_query_advance(w,n); }

static char *path_of(const char *directory, const char *name)
{
	size_t a=strlen(directory),b=strlen(name);
	if (a>SIZE_MAX-b-2) { errno=ENAMETOOLONG; return NULL; }
	char *path=malloc(a+b+2);
	if (path) snprintf(path,a+b+2,"%s/%s",directory,name);
	return path;
}

/* Existing emitters use temporary streams. Keep them anonymous in the owned
	* disk staging directory; never depend on or clean shared /tmp storage. */
FILE *__wrap_tmpfile(void)
{
	int previous=errno;
	char *path=path_of(temporary_directory,".stream.XXXXXX"); if (!path) return NULL;
	int fd=mkstemp(path),saved=errno;
	if (fd<0) { free(path); errno=saved; return NULL; }
	if (unlink(path)) { saved=errno; close(fd); free(path); errno=saved; return NULL; }
	free(path); FILE *file=fdopen(fd,"w+");
	if (!file) { saved=errno; close(fd); errno=saved; }
	else errno=previous;
	return file;
}

static const struct pg_occurrence *select_subject(struct pg_program *program,
	struct pg_synthesis_job *root, const char *name, uint64_t budget, int *status)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *selected=pg_program_select_name(program,root,token);
	if (!selected) { *status=4; return NULL; }
	uint64_t used; enum pg_synthesis_status state=pg_artifact_revalidate(program,selected,budget,budget,&used);
	if (state!=PG_SYNTHESIS_DONE) {
		*status=state==PG_SYNTHESIS_PENDING ? 3 : state==PG_SYNTHESIS_REJECTED ? 1
			: state==PG_SYNTHESIS_UNSUPPORTED ? 4 : 2;
		return NULL;
	}
	const struct pg_evidence *proof=pg_synthesis_result(selected);
	if (!pg_evidence_owned_by(proof,&program->typing)) { *status=2; return NULL; }
	return pg_evidence_subject(proof);
}

static int emit_body(size_t part, FILE *out, struct pg_graph *storage,
	const struct pg_typing *typing, const struct pg_occurrence *const *sources,
	const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	switch (part) {
	case 0: return pg_c_acc_successor_emit(out,storage,typing,sources[0],reference,entries);
	case 1: return pg_c_nat_accessibility_emit(out,storage,typing,sources[1],sources[1],sources[0],entries);
	case 2: return pg_c_acc_comparison_emit(out,storage,typing,sources[2],sources[2],entries);
	case 3: return pg_c_acc_partition_emit(out,storage,typing,sources[3],entries);
	case 4: return pg_c_acc_append_emit(out,storage,typing,sources[4],entries);
	case 5: return pg_c_acc_clause_emit(out,storage,typing,sources[5],sources[8]->core,sources[9]->core,entries);
	case 6: {
		struct pg_c_acc_measure_sources view={sources[6],sources[6],sources[7],sources[7],
			sources[10],sources[1],sources[11],sources[2]};
		return pg_c_acc_measure_parameter_emit(out,storage,typing,&view,entries);
	}
	case 7: return pg_c_acc_create_emit(out,storage,typing,sources[0],reference,entries);
	}
	return -1;
}

int main(int argc, char **argv)
{
	if (argc!=6) { fputs("usage: c-acc-compare-image IMAGE EMPTY_STAGE SUCCESSOR OUTER STEPS_PER_SELECTOR\n",stderr); return 2; }
	char *end; errno=0; uintmax_t parsed=strtoumax(argv[5],&end,10);
	if (errno || !*argv[5] || *argv[5]<'0' || *argv[5]>'9' || *end || parsed>UINT64_MAX) return 2;
	uint64_t budget=parsed; temporary_directory=argv[2];
	FILE *file=fopen(argv[1],"rb"); if (!file) { perror(argv[1]); return 2; }
	size_t count=0; struct pg_synthesis_job *const *roots;
	struct pg_program *program=pg_artifact_read_file(file,PG_ARTIFACT_DEFAULT_LIMIT,&count,&roots);
	int input_error=ferror(file),closed=fclose(file),status=2;
	if (!program || input_error || closed || !count) { if (program) pg_program_destroy(program); return 2; }
	const char *type_names[]={"bool_type","nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	const char *source_names[]={argv[3],"nat_access","compare_fn","partition_nat","append_nat","sort_acc",
		"measure_fn",argv[4],"partition_fn","append_fn","measure_generic","sort_generic"};
	const char *output_names[]={"actions.inc","accessibility.inc","comparison.inc","partition.inc","append.inc","clause.inc","measure.inc","transport.inc"};
	const struct pg_occurrence *types[8],*sources[12],*reference; char *paths[8]={0}; unsigned created=0;
	for (size_t i=0; i<8; ++i) if (!(types[i]=select_subject(program,roots[0],type_names[i],budget,&status))) goto done;
	for (size_t i=0; i<12; ++i) if (!(sources[i]=select_subject(program,roots[0],source_names[i],budget,&status))) goto done;
	if (!(reference=select_subject(program,roots[0],"succ_access",budget,&status))) goto done;
	struct pg_c_source_snapshot *snapshot=pg_c_source_snapshot_create(&program->graph,&program->typing);
	if (!snapshot) goto done;
	struct pg_graph storage;
	if (pg_graph_init(&storage)) { pg_c_source_snapshot_destroy(snapshot); goto done; }
	struct pg_c_indexed_entry entries[]={{.name="bool"},{.name="nat"},{.name="lt"},{.name="acc"},
		{.name="sized"},{.name="measured"},{.name="partition"},{.name="list"}}; emitting=1;
	for (size_t i=0; i<8; ++i) if (pg_c_indexed_view_read(&storage,types[i]->core,&entries[i].view)) { status=4; goto emitted; }
	for (size_t i=0; i<8; ++i) {
		paths[i]=path_of(argv[2],output_names[i]); if (!paths[i]) { status=2; goto emitted; }
		FILE *out=fopen(paths[i],"wx"); if (!out) { status=2; goto emitted; }
		created|=1u<<i; errno=0;
		int lowered=emit_body(i,out,&storage,&program->typing,sources,reference,entries);
		int write_error=ferror(out),saved=errno; closed=fclose(out);
		if (write_error || closed || (lowered && saved)) { fprintf(stderr,"cannot write body %s\n",output_names[i]); status=2; goto emitted; }
		if (lowered) { fprintf(stderr,"unsupported body %s\n",output_names[i]); status=4; goto emitted; }
	}
	status=0;
emitted:
	assert(pg_c_source_snapshot_unchanged(snapshot));
	pg_c_source_snapshot_destroy(snapshot);
	emitting=0; pg_graph_destroy(&storage);
done:
	for (size_t i=0; i<8; ++i) { if (status && (created&(1u<<i))) unlink(paths[i]); free(paths[i]); }
	pg_program_destroy(program);
	fprintf(stderr,"actual Acc comparator candidate bodies: status%d; source owners inert during emission\n",status);
	return status;
}
