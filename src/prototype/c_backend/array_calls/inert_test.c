#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
#include "../selection.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
#include <assert.h>
#include <inttypes.h>
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
	struct pg_token token = {.kind = PG_TOKEN_IDENT,.text = name,.length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,1000000,1000000,&steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected); assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
}

static const struct pg_term *app(struct pg_graph *g, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *t = pg_application(g,f,x); assert(t); return t;
}

static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	const struct pg_term *head = subject->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	assert(head->kind == PG_REFERENCE);
	const struct pg_data_declaration *d = pg_data_declaration_view(head->as.reference); assert(d); return pg_data_declaration_layout(d);
}

static const struct pg_term *natural(struct pg_graph *g, const struct pg_data_layout *nat, unsigned n)
{
	const struct pg_term *t = pg_reference(g,pg_data_constructor(nat,0));
	while (n--) t = app(g,pg_reference(g,pg_data_constructor(nat,1)),t);
	return t;
}

/* Existing Core supplies finite fingerprint observations after inert emission.
	* This does not implement a target checker or replace the source sorter. */
static void oracle(struct pg_program *p, const struct pg_c_link_plan *plan,
	const struct pg_occurrence *fingerprint, const char *path)
{
	assert(plan->count == 3 && plan->natural_count == 1 && plan->data_count == 1);
	struct pg_graph *g = &p->graph; const struct pg_data_layout *nat = layout(plan->naturals[0].subject), *list = layout(plan->data[0].subject);
	FILE *out = fopen(path,"w"); assert(out);
	fputs("#include \"component.h\"\n#include <assert.h>\n"
		"static uint32_t fingerprint(const uint32_t *a, size_t n) { uint32_t code = 0; assert(n <= 4);\n"
		"while (n) { assert(a[n-1] < 4); code = code * 5 + a[--n] + 1; } return code; }\nint main(void) {\n",out);
	size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 3; ++length, combinations *= 4)
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t a[3] = {0}; size_t digits = code;
			for (size_t i = 0; i < length; ++i,digits /= 4) a[i] = (uint32_t)(digits%4);
			const struct pg_term *input = pg_reference(g,pg_data_constructor(list,0));
			for (size_t i = length; i; --i) input = app(g,app(g,pg_reference(g,pg_data_constructor(list,1)),natural(g,nat,a[i-1])),input);
			fprintf(out,"{ uint32_t a[3] = {%u,%u,%u}, out[4]; size_t n;\n",a[0],a[1],a[2]);
			for (unsigned form = 0; form < 6; ++form) {
				const struct pg_term *call = plan->exports[form < 2 ? form : 2].subject->core;
				if (form >= 2) call = app(g,call,natural(g,nat,form-2));
				call = app(g,call,input);
				call = app(g,fingerprint->core,app(g,pg_reference(g,&pg_total_result_operation),call));
				struct pg_eval machine; pg_computation_eval_init(&machine,g,app(g,pg_reference(g,&pg_total_result_operation),call));
				assert(pg_eval_advance(&machine,100000) == PG_EVAL_WHNF);
				const struct pg_term *result = pg_eval_readback(&machine,g); int64_t value;
				assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference,&value)); pg_eval_destroy(&machine);
				if (form < 2) fprintf(out,"assert(!ap_buffer_%s(a,%zu,out,4,&n));\n",plan->exports[form].alias,length);
				else fprintf(out,"assert(!ap_buffer_insert(%u,a,%zu,out,4,&n));\n",form-2,length);
				fprintf(out,"assert(n == %zu && fingerprint(out,n) == UINT32_C(%" PRId64 "));\n",length+(form>=2),value); ++cases;
			}
			fputs("}\n",out);
		}
	assert(cases == 510); fputs("return 0; }\n",out); assert(!fclose(out));
}

int main(int argc, char **argv)
{
	assert(argc == 4 || argc == 5); struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan,argv[1],&line,&error) && plan.lowering == PG_C_NATIVE_ARRAY_CALLS);
	FILE *file = fopen(plan.artifact,"rb"); assert(file); size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(p,roots[0],plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(p,roots[0],plan.enum_names[i]);
	for (size_t i = 0; i < plan.natural_count; ++i) plan.naturals[i].subject = select_subject(p,roots[0],plan.natural_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) {
		plan.data[i].subject = select_subject(p,roots[0],plan.data_names[i]);
		if (plan.data_from_value[i]) plan.data[i].subject = pg_c_value_classifier(plan.data[i].subject);
		assert(plan.data[i].subject);
	}
	const struct pg_occurrence *fingerprint = argc == 5 ? select_subject(p,roots[0],"fingerprint") : NULL;
	FILE *source = fopen(argv[2],"w"), *header = fopen(argv[3],"w"); assert(source && header);
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count, proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_native_array_calls(source,header,plan.count,plan.exports,SIZE_MAX,
		plan.enum_count,plan.enums,plan.natural_count,plan.naturals,plan.data_count,plan.data,&contract,&error)); emitting = 0;
	assert(contract.recursive && contract.copy_out);
	assert(p->graph.terms.count == terms && p->graph.objects.count == objects && p->typing.proofs.count == proofs && p->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header));
	if (fingerprint) oracle(p,&plan,fingerprint,argv[4]);
	pg_program_destroy(p); pg_c_link_destroy(&plan);
	puts("Array wrapper emission inert; source-sort mode separately supplies510 finite Core fingerprint comparisons"); return 0;
}
