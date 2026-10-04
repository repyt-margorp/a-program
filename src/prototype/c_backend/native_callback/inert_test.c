#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
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
	const struct pg_evidence *proof = pg_synthesis_result(selected); assert(pg_evidence_owned_by(proof,&p->typing));
	return pg_evidence_subject(proof);
}
static const struct pg_term *app(struct pg_graph *g, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(g,f,x); assert(term); return term;
}
static const struct pg_term *op(struct pg_graph *g, const struct pg_object *operation, const struct pg_term *x)
{
	return app(g,pg_reference(g,operation),x);
}
static const struct pg_term *number(struct pg_graph *g, unsigned wide, int64_t value)
{
	const struct pg_object *object = pg_host_integer(g,pg_host_type(wide ? "Int64" : "Int32"),value); assert(object);
	return pg_reference(g,object);
}
static const struct pg_term *whnf(struct pg_program *p, const struct pg_term *value)
{
	struct pg_eval machine; pg_eval_init(&machine,value);
	assert(pg_eval_advance(&machine,100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine,&p->graph); assert(result); pg_eval_destroy(&machine); return result;
}
static const struct pg_term *evaluate(struct pg_program *p, const struct pg_term *call)
{
	struct pg_eval machine; pg_computation_eval_init(&machine,&p->graph,op(&p->graph,&pg_total_result_operation,call));
	assert(pg_eval_advance(&machine,100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine,&p->graph); assert(result); pg_eval_destroy(&machine); return result;
}
static int64_t integer_result(struct pg_program *p, const struct pg_term *call)
{
	const struct pg_term *result = evaluate(p,call); int64_t value;
	assert(result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference,&value)); return value;
}
static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	assert(subject->core->kind == PG_REFERENCE);
	const struct pg_data_declaration *d = pg_data_declaration_view(subject->core->as.reference); assert(d);
	return pg_data_declaration_layout(d);
}
static void literal(FILE *file, unsigned wide, int64_t n)
{
	if (n == INT64_MIN) fputs("INT64_MIN",file);
	else if (!wide && n == INT32_MIN) fputs("INT32_MIN",file);
	else fprintf(file,"INT%u_C(%" PRId64 ")",wide ? 64 : 32,n);
}

static void list_result(struct pg_program *p, const struct pg_data_layout *list, unsigned wide,
	const struct pg_term *call, size_t count, int64_t *values)
{
	const struct pg_term *result = evaluate(p,call);
	for (size_t i = 0;; ++i) {
		const struct pg_term *head = whnf(p,result), *arguments[2]; size_t arity = 0;
		while (head->kind == PG_APPLICATION) {
			assert(arity < 2); arguments[arity++] = head->as.application.argument; head = head->as.application.function;
		}
		const struct pg_data_layout *owner; size_t position, fields;
		assert(head->kind == PG_REFERENCE && pg_data_constructor_view(head->as.reference,&owner,&position,&fields) && owner == list);
		if (!fields) { assert(!arity && position == (wide ? 1 : 0) && i == count); return; }
		assert(fields == 2 && arity == 2 && position == (wide ? 0 : 1) && i < count);
		/* Applications were peeled from right to left. Int64 has reversed fields. */
		const struct pg_term *item = whnf(p,arguments[wide ? 0 : 1]);
		assert(item->kind == PG_REFERENCE && pg_host_integer_view(item->as.reference,&values[i]));
		result = arguments[wide ? 1 : 0];
	}
}

/* Existing Core runs only after inert emission; full-value observations are
	* descriptive finite comparisons, not a new source/transformation checker. */
static void oracle(struct pg_program *p, const struct pg_c_link_plan *plan,
	const struct pg_occurrence *negate[2], const struct pg_occurrence *subtract[2], const char *path)
{
	assert(plan->count == 13 && plan->data_count == 4 && plan->enum_count == 1 && !plan->natural_count);
	struct pg_graph *g = &p->graph; FILE *file = fopen(path,"w"); assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\n"
		"static int32_t s32(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)(UINT32_MAX-n);}\n"
		"static int64_t s64(uint64_t n){return n<=INT64_MAX?(int64_t)n:-1-(int64_t)(UINT64_MAX-n);}\n"
		"static int32_t n32(void *p,int32_t n){(void)p;return s32(UINT32_C(0)-(uint32_t)n);}\n"
		"static int64_t n64(void *p,int64_t n){(void)p;return s64(UINT64_C(0)-(uint64_t)n);}\n"
		"static int32_t b32(void *p,int32_t x,int32_t y){(void)p;return s32((uint32_t)x-(uint32_t)y);}\n"
		"static int64_t b64(void *p,int64_t x,int64_t y){(void)p;return s64((uint64_t)x-(uint64_t)y);}\n"
		"int main(void){struct ap_c_arena arena={0};struct ap_c_callback_i32 u32={0,n32};struct ap_c_callback_i64 u64={0,n64};\n"
		"struct ap_c_callback2_i32 b32v={0,b32};struct ap_c_callback2_i64 b64v={0,b64};\n",file);
	size_t comparisons = 0;
	for (unsigned wide = 0; wide < 2; ++wide) {
		const struct pg_data_layout *list = layout(plan->data[wide].subject);
		const int64_t choices[] = {wide ? INT64_MIN : INT32_MIN,-1,0,1,wide ? INT64_MAX : INT32_MAX};
		const struct pg_term *unary = op(g,&pg_thunk_operation,negate[wide]->core), *binary = op(g,&pg_thunk_operation,subtract[wide]->core);
		for (size_t count = 0, combinations = 1; count <= 3; ++count, combinations *= 5)
			for (size_t code = 0; code < combinations; ++code) {
				int64_t values[3] = {0}, output[3]; size_t digits = code;
				for (size_t i = 0; i < count; ++i,digits /= 5) values[i] = choices[digits%5];
				const struct pg_term *input = pg_reference(g,pg_data_constructor(list,wide ? 1 : 0));
				for (size_t i = count; i; --i) {
					const struct pg_term *cell = pg_reference(g,pg_data_constructor(list,wide ? 0 : 1));
					input = wide ? app(g,app(g,cell,input),number(g,wide,values[i-1])) : app(g,app(g,cell,number(g,wide,values[i-1])),input);
				}
				fprintf(file,"{int%u_t a[3]={",wide ? 64 : 32);
				for (size_t i = 0; i < 3; ++i) { if (i) fputc(',',file); literal(file,wide,values[i]); }
				fprintf(file,"},copy[3],result;size_t written;const struct ap_data_Numbers%u *input,*out;\nassert(!ap_from_Numbers%u(&arena,a,%zu,&input));\n",wide?64:32,wide?64:32,count);
				for (size_t form = 0; form < 6; ++form) {
					const struct pg_term *call = app(g,app(g,plan->exports[(form ? 2 : 0)+wide].subject->core,form ? binary : unary),input);
					if (form) call = app(g,call,number(g,wide,choices[form-1]));
					list_result(p,list,wide,call,count,output);
					if (form) {
						fprintf(file,"assert(!ap_export_combine%u(&arena,b%uv,input,",wide?64:32,wide?64:32); literal(file,wide,choices[form-1]);
						fputs(",&out));\n",file);
					} else fprintf(file,"assert(!ap_export_map%u(&arena,u%u,input,&out));\n",wide?64:32,wide?64:32);
					fprintf(file,"assert(!ap_copy_Numbers%u(out,copy,3,&written)&&written==%zu);\n",wide?64:32,count);
					for (size_t i = 0; i < count; ++i) { fprintf(file,"assert(copy[%zu]==",i); literal(file,wide,output[i]); fputs(");\n",file); }
					++comparisons;
				}
				for (size_t seed = 0; seed < 5; ++seed) {
					const struct pg_term *call = app(g,app(g,app(g,plan->exports[4+wide].subject->core,binary),input),number(g,wide,choices[seed]));
					int64_t result = integer_result(p,call);
					fprintf(file,"assert(!ap_export_reduce%u(&arena,b%uv,input,",wide?64:32,wide?64:32); literal(file,wide,choices[seed]);
					fputs(",&result)&&result==",file); literal(file,wide,result); fputs(");\n",file); ++comparisons;
				}
				fputs("ap_arena_Numbers32_destroy(&arena);}\n",file);
			}
	}
	assert(comparisons == 3432); fputs("return 0;}\n",file); assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5); struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan,argv[1],&line,&error) && plan.lowering == PG_C_CALLBACK_NATIVE_DIRECT);
	FILE *file = fopen(plan.artifact,"rb"); assert(file); size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(file,SIZE_MAX,&count,&roots); assert(!fclose(file) && p && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(p,roots[0],plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(p,roots[0],plan.enum_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(p,roots[0],plan.data_names[i]);
	const struct pg_occurrence *negate[] = {select_subject(p,roots[0],"negate32"),select_subject(p,roots[0],"negate64")};
	const struct pg_occurrence *subtract[] = {select_subject(p,roots[0],"subtract32"),select_subject(p,roots[0],"subtract64")};
	FILE *source = fopen(argv[2],"w"), *header = fopen(argv[3],"w"); assert(source && header);
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count, proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_callbacks_native(source,header,plan.count,plan.exports,SIZE_MAX,
		plan.enum_count,plan.enums,0,NULL,plan.data_count,plan.data,&contract,&error)); emitting = 0;
	assert(contract.recursive && contract.branching && contract.copy_out && !contract.natural);
	assert(p->graph.terms.count == terms && p->graph.objects.count == objects && p->typing.proofs.count == proofs && p->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header)); oracle(p,&plan,negate,subtract,argv[4]);
	pg_program_destroy(p); pg_c_link_destroy(&plan);
	puts("Native scalar callback emission inert; existing Core supplies3432 full-value map/combine/reduction comparisons"); return 0;
}
