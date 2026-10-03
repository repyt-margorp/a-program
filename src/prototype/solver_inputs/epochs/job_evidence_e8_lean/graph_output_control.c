#include "program.h"
#include "synthesis_source.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	assert(argc == 2);
	FILE *file = fopen(argv[1], "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long length = ftell(file);
	assert(length >= 0 && !fseek(file, 0, SEEK_SET));
	char *source = malloc((size_t)length + 1);
	assert(source && fread(source, 1, (size_t)length, file) == (size_t)length);
	source[length] = 0;
	assert(!fclose(file));
	struct pg_program *program = pg_program_create(source, (size_t)length, PG_DEFINITION_IMPLICIT_THUNK);
	free(source);
	assert(program && program->root && !program->parser.error);
	struct pg_synthesis *synthesis = &program->synthesis;
	size_t borrowed = 0, leaves = 0;
	const uint64_t budgets[] = {0, 1, 32, 100, 1000, 10000000};
	uint64_t previous = 0;
	for (size_t cut = 0; cut < sizeof(budgets) / sizeof(*budgets); ++cut) {
		pg_synthesis_advance(synthesis, budgets[cut] - previous);
		previous = budgets[cut];
		borrowed = leaves = 0;
		for (size_t i = 0; i < synthesis->jobs.capacity; ++i) {
			for (const struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
				const struct pg_synthesis_job *job = (const void *)entry;
				const struct pg_source_scope *scope;
				const struct pg_syntax *syntax;
				if (pg_synthesis_source_input(synthesis, job, &scope, &syntax)
					|| syntax->kind != PG_SYNTAX_GRAPH_REFERENCE) continue;
				struct pg_synthesis_input output = pg_synthesis_work_output(job);
				if (job->status == PG_SYNTHESIS_PENDING) {
					assert(!output.checked && !output.pending && !pg_synthesis_result(job));
				} else if (job->status == PG_SYNTHESIS_DONE) {
					const struct pg_evidence *proof = pg_synthesis_result(job);
					assert(proof);
					if (output.pending) {
						assert(!job->result && proof == pg_synthesis_input_result(output));
						++borrowed;
					} else {
						assert(job->result && pg_evidence_rule(proof) == PG_VARIABLE);
						++leaves;
					}
				}
			}
		}
	}
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE && borrowed);
	const struct pg_source_scope *root = pg_synthesis_root(synthesis);
	const struct pg_evidence *empty = pg_synthesis_scope_context(root);
	const struct pg_evidence *universe = pg_prove_universe(&program->typing, empty, 0);
	const struct pg_object *function = pg_binder(&program->graph), *graph = pg_binder(&program->graph);
	const struct pg_evidence *function_context = pg_prove_context_extension(&program->typing, empty, function, universe);
	const struct pg_evidence *graph_context = pg_prove_context_extension(&program->typing, function_context, graph,
		pg_prove_universe(&program->typing, function_context, 0));
	const struct pg_source_scope *scope = pg_synthesis_scope_bind(synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "f", .length = 1}, function,
		(struct pg_synthesis_input){.checked = function_context}, NULL, NULL, PG_SOURCE_UNASSOCIATED);
	scope = pg_synthesis_scope_bind(synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "fg", .length = 2}, graph,
		(struct pg_synthesis_input){.checked = graph_context}, function, NULL, PG_SOURCE_GRAPH);
	assert(scope);
	struct pg_synthesis_job *leaf_module = pg_program_source(program, scope, "r:=@f;", 6, &program->parser);
	assert(leaf_module && !program->parser.error);
	pg_synthesis_advance(synthesis, 1000);
	assert(pg_synthesis_status(leaf_module) == PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *leaf = pg_synthesis_definition(leaf_module,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "r", .length = 1});
	struct pg_synthesis_job *body;
	assert(!pg_synthesis_definition_body(synthesis, leaf, &body) && body);
	assert(body->result && pg_evidence_rule(pg_synthesis_result(body)) == PG_VARIABLE);
	assert(pg_evidence_subject(pg_synthesis_result(body))->core == pg_reference(&program->graph, graph));
	assert(!pg_synthesis_work_output(body).pending);
	++leaves;
	printf("graph outputs: borrowed=%zu binder_leaves=%zu steps=%llu\n", borrowed, leaves,
		(unsigned long long)synthesis->steps);
	pg_program_destroy(program);
	return 0;
}
