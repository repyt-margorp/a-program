#include "artifact/file.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
#include "../selection.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

/* Existing Core evaluates the actual admitted quickSort; this fixture emits
	* finite expected observations, not a new checker or a replacement sorter. */
static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT,.text = name,.length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p,root,token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p,selected,5000000,5000000,&steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof,&p->typing)); return pg_evidence_subject(proof);
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
	const struct pg_data_declaration *declaration = pg_data_declaration_view(head->as.reference); assert(declaration);
	return pg_data_declaration_layout(declaration);
}

static const struct pg_term *natural(struct pg_graph *g, const struct pg_data_layout *nat, uint32_t n)
{
	const struct pg_term *t = pg_reference(g,pg_data_constructor(nat,0));
	while (n--) t = app(g,pg_reference(g,pg_data_constructor(nat,1)),t);
	return t;
}

int main(int argc, char **argv)
{
	assert(argc == 3); FILE *image = fopen(argv[1],"rb"); assert(image);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(image,SIZE_MAX,&count,&roots); assert(!fclose(image) && p && count);
	const struct pg_occurrence *sort = select_subject(p,roots[0],"sort"), *fingerprint = select_subject(p,roots[0],"fingerprint");
	const struct pg_data_layout *nat = layout(select_subject(p,roots[0],"nat_type"));
	const struct pg_occurrence *list_type = pg_c_value_classifier(select_subject(p,roots[0],"empty")); assert(list_type);
	const struct pg_data_layout *list = layout(list_type); struct pg_graph *g = &p->graph;
	FILE *out = fopen(argv[2],"w"); assert(out);
	fputs("#include \"mockup.h\"\n#include <assert.h>\n"
		"static uint32_t fingerprint(const uint32_t *a, size_t n) { uint32_t code = 0; assert(n <= 4);\n"
		"while (n) { assert(a[n-1] < 4); code = code * 5 + a[--n] + 1; } return code; }\nint main(void) {\n",out);
	size_t cases = 0;
	for (size_t length = 0, combinations = 1; length <= 4; ++length, combinations *= 4)
		for (size_t code = 0; code < combinations; ++code) {
			uint32_t values[4] = {0}; size_t digits = code;
			for (size_t i = 0; i < length; ++i,digits /= 4) values[i] = (uint32_t)(digits%4);
			const struct pg_term *input = pg_reference(g,pg_data_constructor(list,0));
			for (size_t i = length; i; --i) input = app(g,app(g,pg_reference(g,pg_data_constructor(list,1)),natural(g,nat,values[i-1])),input);
			const struct pg_term *call = app(g,sort->core,input);
			call = app(g,fingerprint->core,app(g,pg_reference(g,&pg_total_result_operation),call));
			struct pg_eval machine; pg_computation_eval_init(&machine,g,app(g,pg_reference(g,&pg_total_result_operation),call));
			assert(pg_eval_advance(&machine,1000000) == PG_EVAL_WHNF);
			const struct pg_term *result = pg_eval_readback(&machine,g); int64_t value;
			assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference,&value)); pg_eval_destroy(&machine);
			fprintf(out,"{ uint32_t a[4] = {%u,%u,%u,%u}, out[4]; size_t n;\n"
				"assert(!qs_mockup_sort(a,%zu,out,4,&n,0) && n == %zu && fingerprint(out,n) == UINT32_C(%" PRId64 ")); }\n",
				values[0],values[1],values[2],values[3],length,length,value); ++cases;
		}
	assert(cases == 341); fputs("return 0; }\n",out); assert(!fclose(out)); pg_program_destroy(p);
	puts("Actual admitted Acc quickSort:341 finite existing-Core observations emitted for a labeled manual C candidate"); return 0;
}
