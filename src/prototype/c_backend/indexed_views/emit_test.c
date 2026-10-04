#include "emit.h"
#include "artifact/file.h"
#include "host.h"
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

static const struct pg_term *select_type(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT,.text = name,.length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof)->core;
}

static void refusals(struct pg_graph *storage, const struct pg_term *nat)
{
	struct pg_c_indexed_view view;
	const struct pg_object *binder = pg_binder(storage);
	const struct pg_term *ref = pg_reference(storage,binder);
	assert(!pg_c_indexed_view_read(storage,nat,&view));
	assert(pg_c_indexed_view_read(storage,ref,&view));
	assert(pg_c_indexed_view_read(storage,pg_lambda(storage,binder,ref),&view));
	assert(pg_c_indexed_view_read(storage,pg_application(storage,nat,nat),&view));
	const struct pg_term *stuck = pg_application(storage,ref,nat);
	assert(pg_c_indexed_view_read(storage,stuck,&view));
	/* A known factory's argument remains in its original lexical environment. */
	const struct pg_term *identity = pg_lambda(storage,binder,ref);
	assert(!pg_c_indexed_view_read(storage,pg_application(storage,identity,nat),&view));
	assert(view.declaration == pg_data_declaration_view(nat->as.reference));
	const struct pg_term *unused = pg_lambda(storage,binder,nat);
	assert(!pg_c_indexed_view_read(storage,pg_application(storage,unused,stuck),&view));
	const struct pg_object *second = pg_binder(storage);
	const struct pg_term *keep_first = pg_lambda(storage,binder,pg_lambda(storage,second,ref));
	const struct pg_term *two = pg_application(storage,pg_application(storage,keep_first,nat),stuck);
	assert(!pg_c_indexed_view_read(storage,two,&view));
	const struct pg_term *keep_second = pg_lambda(storage,binder,pg_lambda(storage,second,pg_reference(storage,second)));
	two = pg_application(storage,pg_application(storage,keep_second,stuck),nat);
	assert(!pg_c_indexed_view_read(storage,two,&view));
	/* No selected expression is made valid by treating its expected type as
		* a producer. The caller supplies already-admitted subject cores only. */
	puts("Static descriptor heads: known lexical factories; unknown/unsaturated/oversaturated heads refuse");
}

int main(int argc, char **argv)
{
	assert(argc == 3); FILE *image = fopen(argv[1],"rb"); assert(image);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(image,SIZE_MAX,&count,&roots); assert(!fclose(image) && p && count);
	const char *names[] = {"nat_type","lt_family","acc_family","sized_family","measured_type","partition_type","list_type"};
	struct pg_c_indexed_entry entries[] = {{.name="nat"},{.name="lt"},{.name="acc"},{.name="sized"},{.name="measured"},{.name="partition"},{.name="list"}};
	const struct pg_term *types[7];
	for (size_t i = 0; i < 7; ++i) types[i] = select_type(p,roots[0],names[i]);
	const struct pg_term *other_measured = select_type(p,roots[0],"other_measured_type");
	struct pg_graph storage; assert(!pg_graph_init(&storage));
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count;
	size_t proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	FILE *out = fopen(argv[2],"w"); assert(out); emitting = 1;
	for (size_t i = 0; i < 7; ++i) assert(!pg_c_indexed_view_read(&storage,types[i],&entries[i].view));
	assert(!pg_c_indexed_emit(out,&storage,7,entries)); assert(!fclose(out));
	FILE *sink = tmpfile(); assert(sink);
	assert(pg_c_indexed_emit(sink,&storage,0,entries));
	struct pg_c_indexed_entry duplicate[] = {entries[0],entries[0]};
	assert(pg_c_indexed_emit(sink,&storage,2,duplicate));
	struct pg_c_indexed_entry invalid = entries[0]; invalid.name = "injected*/";
	assert(pg_c_indexed_emit(sink,&storage,1,&invalid));
	/* Missing relation representation cannot silently become an opaque proof. */
	struct pg_c_indexed_entry missing[] = {entries[0],entries[2]};
	assert(pg_c_indexed_emit(sink,&storage,2,missing));
	/* The same declaration identity at Bool cannot reuse SizedList Nat. */
	struct pg_c_indexed_entry other[7]; memcpy(other,entries,sizeof(other));
	assert(!pg_c_indexed_view_read(&storage,other_measured,&other[4].view));
	assert(pg_c_indexed_emit(sink,&storage,7,other));
	assert(!fclose(sink)); refusals(&storage,types[0]); emitting = 0;
	assert(p->graph.terms.count == terms && p->graph.objects.count == objects);
	assert(p->typing.proofs.count == proofs && p->typing.occurrences.count == occurrences);
	pg_graph_destroy(&storage); pg_program_destroy(p);
	puts("Seven actual indexed/callable declarations extracted inertly; no expression/Fold body emitted"); return 0;
}
