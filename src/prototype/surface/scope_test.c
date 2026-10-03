/* Compile the actual source module once in this test translation unit so the
	* lexical-only dependency scanner is exercised directly. No alternate checker. */
#include "synthesis.c"
#include <assert.h>

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work evaluation;
	struct pg_synthesis synthesis;
	assert(!pg_graph_init(&graph));
	assert(!pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&evaluation, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	const struct pg_evidence *empty = pg_synthesis_scope_context(root);
	const struct pg_object *tail = pg_binder(&graph);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, empty, tail,
		pg_prove_universe(&typing, empty, 0));
	assert(context);
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "tail", .length = 4, .text_length = 4},
		tail, (struct pg_synthesis_input){.checked = context});
	assert(scope);
	const char *texts[] = {
		"m:=g @c {{tail:=origin;}}=>*tail;",
		"m:=g @c {{inner:=tail;}}=>*tail;",
		"m:=g @c {{inner:=tail;}}=>*inner;",
		"m:=\\tail:T=>*tail;",
		"m:={tail:=*tail;tail;};"
	};
	for (size_t i = 0; i < sizeof(texts) / sizeof(*texts); ++i) {
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &graph, texts[i], strlen(texts[i]));
		assert(pg_parser_next(&parser, &definition) == 1);
		int uses_scrutinee = 0;
		assert(branch_dependencies(scope, pg_evidence_context(empty), definition.expression,
			NULL, &uses_scrutinee) == (i == 1 || i == 4));
	}
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&evaluation);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("surface nested scopes: LHS shadows, RHS selector does not shadow, outer IH remains lexical");
	return 0;
}
