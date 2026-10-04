#include "artifact/file.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

/* Existing Core computes finite comparator observations. This is an oracle
	* test, not a new source checker or an emitter-side evaluation path. */
static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token={.kind=PG_TOKEN_IDENT,.text=name,.length=strlen(name)};
	struct pg_synthesis_job *s=pg_program_select_name(p,root,token); uint64_t steps;
	assert(s && pg_artifact_revalidate(p,s,5000000,5000000,&steps)==PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof=pg_synthesis_result(s); assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}
static const struct pg_term *app(struct pg_graph *g, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *t=pg_application(g,f,x); assert(t); return t;
}
static const struct pg_term *natural(struct pg_graph *g, const struct pg_data_layout *nat, uint32_t n)
{
	const struct pg_term *t=pg_reference(g,pg_data_constructor(nat,0));
	while (n--) t=app(g,pg_reference(g,pg_data_constructor(nat,1)),t);
	return t;
}
int main(int argc, char **argv)
{
	assert(argc==3); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p=pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	const struct pg_occurrence *compare=select_subject(p,roots[0],"compare_fn"),*observe=select_subject(p,roots[0],"bool_value"),*nat_type=select_subject(p,roots[0],"nat_type");
	assert(nat_type->core->kind==PG_REFERENCE);
	const struct pg_data_declaration *decl=pg_data_declaration_view(nat_type->core->as.reference); assert(decl);
	const struct pg_data_layout *nat=pg_data_declaration_layout(decl); struct pg_graph *g=&p->graph;
	FILE *out=fopen(argv[2],"w"); assert(out);
	fputs("#include \"support.c\"\n#include <assert.h>\nint main(void) {\n",out); size_t cases=0;
	for (uint32_t left=0; left<16; ++left) for (uint32_t right=0; right<16; ++right) {
		const struct pg_term *call=app(g,app(g,compare->core,natural(g,nat,left)),natural(g,nat,right));
		call=app(g,observe->core,app(g,pg_reference(g,&pg_total_result_operation),call));
		struct pg_eval machine; pg_computation_eval_init(&machine,g,app(g,pg_reference(g,&pg_total_result_operation),call));
		assert(pg_eval_advance(&machine,1000000)==PG_EVAL_WHNF);
		const struct pg_term *result=pg_eval_readback(&machine,g); int64_t value;
		assert(result && result->kind==PG_REFERENCE && pg_host_integer_view(result->as.reference,&value) && (value==0 || value==1)); pg_eval_destroy(&machine);
		fprintf(out,"{ struct qs_arena a = {0}; assert(gc_compare(&a,0,%u,%u) == %" PRId64 " && !a.status && !a.count && !a.depth); }\n",left,right,value); ++cases;
	}
	assert(cases==256); fputs("return 0; }\n",out); assert(!fclose(out)); pg_program_destroy(p);
	puts("Actual admitted natLessOrEqual:256 finite existing-Core comparison observations"); return 0;
}
