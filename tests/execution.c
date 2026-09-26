#include "execution.h"
#include "program.h"
#include "host.h"

#include <assert.h>
#include <string.h>

static const struct pg_evidence *entry(struct pg_program *program, const char *source)
{
	program->root = pg_program_source(program, program->scope, source, strlen(source), &program->parser);
	assert(program->root);
	struct pg_synthesis_job *job = pg_program_select_name(program, program->root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "main", .length = 4});
	assert(job && !pg_synthesis_result(job));
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(program->synthesis.steps < 100000);
		assert(program->synthesis.ready);
		pg_synthesis_advance(&program->synthesis, 1);
	}
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(job);
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE) {
		const struct pg_term *content;
		proof = pg_thunk_type_view(pg_evidence_classifier(proof), &content)
			? pg_prove_force(&program->typing, proof)
			: pg_prove_return(&program->typing, proof);
	}
	assert(proof);
	return proof;
}

static void output_is(FILE *output, const char *expected, size_t count)
{
	assert(!fflush(output));
	assert(ftell(output) == (long)count);
	rewind(output);
	for (size_t i = 0; i < count; ++i) assert(fgetc(output) == (unsigned char)expected[i]);
	assert(fgetc(output) == EOF);
	assert(!fseek(output, 0, SEEK_END));
}

static void value_is(const struct pg_term *result, const char *expected, size_t count)
{
	assert(result && result->kind == PG_REFERENCE);
	const struct pg_object *type;
	const unsigned char *bytes;
	size_t length;
	assert(pg_host_literal_view(result->as.reference, &type, &length, &bytes));
	assert(type == pg_host_type("Text") && length == count);
	assert(!memcmp(bytes, expected, count));
}

static void invocations(void)
{
	const struct {
		const char *source, *output, *result;
	} cases[] = {
		{"main:=#print #\"a\";", "a", "a"},
		{"main:=#print #\"\";", "", ""},
		{"main:={x:=#print #\"a\";#print x;};", "aa", "a"},
		{"second:=\\x:#Text=>\\y:#Text=>y; main:=second (#print #\"a\") (#print #\"b\");", "ab", "b"},
		{"main:=#print (#print #\"a\");", "aa", "a"},
		{"main:=(#print #\"a\") @#print req k=>k #\"b\" @#return x=>x;", "", "b"},
		{"main:=(#print #\"a\") @#print req k=>#\"b\" @#return x=>x;", "", "b"},
		{"main:=(#print #\"a\") @#return x=>x;", "a", "a"},
		{"main:=(#print #\"a\") @#print req k=>#print #\"b\" @#return x=>x;", "b", "b"},
		{"main:=({x:=#print #\"a\";#print x;}) @#print req k=>{#print req;k req;} @#return x=>x;", "aa", "a"},
		{"main:=({x:=#print #\"a\";x;}) @#print req k=>{k #\"b\";k #\"c\";} @#return x=>#print x;", "bc", "c"},
		{"main:=({x:=#print #\"a\";#print x;}) @#print req k=>#\"b\" @#return x=>x;", "", "b"}
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		struct pg_program *program = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
		assert(program);
		const struct pg_evidence *proof = entry(program, cases[i].source);
		size_t proofs = program->typing.proofs.count, memo = program->evaluation.jobs.count;
		uint64_t total = 0;
		/* Repeat one accepted term in fresh invocations, splitting at every
		 * transition, including before/after the external request callback. */
		for (uint64_t cut = 0; cut <= total; ++cut) {
			FILE *output = tmpfile();
			assert(output);
			struct pg_execution execution;
			assert(!pg_execution_init(&execution, &program->typing, proof, output));
			assert(!ftell(output));
			pg_execution_advance(&execution, cut);
			if (!cut) assert(!ftell(output) && !execution.steps);
			while (execution.status == PG_EXECUTION_PENDING) {
				assert(execution.steps < 10000);
				pg_execution_advance(&execution, 64);
			}
			assert(execution.status == PG_EXECUTION_DONE);
			if (!cut) total = execution.steps;
			assert(execution.steps == total);
			value_is(execution.result, cases[i].result, strlen(cases[i].result));
			pg_execution_advance(&execution, 1000);
			assert(execution.steps == total);
			pg_execution_destroy(&execution);
			output_is(output, cases[i].output, strlen(cases[i].output));
			fclose(output);
			assert(program->typing.proofs.count == proofs && program->evaluation.jobs.count == memo);
		}
		/* Cancellation at every cut must not execute any remaining callback. */
		for (uint64_t cut = 0; cut < total; ++cut) {
			FILE *output = tmpfile();
			assert(output);
			struct pg_execution execution;
			assert(!pg_execution_init(&execution, &program->typing, proof, output));
			pg_execution_advance(&execution, cut);
			long before = ftell(output);
			pg_execution_destroy(&execution);
			assert(ftell(output) == before);
			fclose(output);
		}
		pg_program_destroy(program);
	}
}

static void admission_and_unknown(void)
{
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	struct pg_program *foreign = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p && foreign);
	const struct pg_evidence *context = pg_prove_empty_context(&p->typing);
	const struct pg_evidence *text = pg_prove_host_type(&p->typing, context, pg_host_type("Text"));
	const struct pg_operation_declaration *op = pg_operation_declaration(&p->typing, text, text);
	assert(op);
	p->scope = pg_synthesis_name_job(&p->synthesis, p->scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Other", .length = 5},
		pg_synthesis_operation(&p->synthesis, op));
	const struct pg_evidence *proof = entry(p, "main:=Other #\"a\";");
	FILE *output = tmpfile();
	assert(output);
	struct pg_execution execution;
	assert(pg_execution_init(&execution, &foreign->typing, proof, output));
	pg_execution_destroy(&execution);
	assert(pg_execution_init(&execution, &p->typing, text, output));
	pg_execution_destroy(&execution);
	assert(pg_execution_init(&execution, &p->typing, NULL, output));
	pg_execution_destroy(&execution);
	const struct pg_object *x = pg_binder(&p->graph);
	const struct pg_evidence *extended = pg_prove_context_extension(&p->typing, context, x, text);
	const struct pg_evidence *variable = pg_prove_variable(&p->typing, extended, x);
	const struct pg_evidence *open = pg_prove_return(&p->typing, variable);
	assert(open && pg_execution_init(&execution, &p->typing, open, output));
	pg_execution_destroy(&execution);
	assert(!pg_execution_init(&execution, &p->typing, proof, output));
	assert(pg_execution_advance(&execution, 10000) == PG_EXECUTION_UNHANDLED);
	pg_execution_destroy(&execution);
	output_is(output, "", 0);
	fclose(output);
	pg_program_destroy(foreign);
	pg_program_destroy(p);
}

static void output_failure(void)
{
	FILE *output = fopen("/dev/full", "wb");
	if (!output) return;
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	const struct pg_evidence *proof = entry(p, "main:={#print #\"a\";#print #\"b\";};");
	struct pg_execution execution;
	assert(!pg_execution_init(&execution, &p->typing, proof, output));
	assert(pg_execution_advance(&execution, 10000) == PG_EXECUTION_IO_ERROR);
	uint64_t steps = execution.steps;
	clearerr(output);
	assert(pg_execution_advance(&execution, 10000) == PG_EXECUTION_IO_ERROR);
	assert(execution.steps == steps && !execution.result && !ferror(output));
	pg_execution_destroy(&execution);
	fclose(output);
	pg_program_destroy(p);
}

int main(void)
{
	invocations();
	admission_and_unknown();
	output_failure();
	puts("execution: checked entry, handlers, repeated/split invocations and cancellation passed");
	return 0;
}
