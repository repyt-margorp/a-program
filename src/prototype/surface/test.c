#include "program.h"
#include "syntax_io.h"
#include "source_io.h"
#include "dag.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char source[] =
	"Nat:=@{zero:*;succ:*->*;};"
	"List:=@{nil:*;cons:Nat->*->*;};"
	"length:=\\xs:List => xs @nil=>Nat.zero "
	"@cons head tail=>{tailLength:=*tail;Nat.succ tailLength;};"
	"u:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons {{ t:=tail; r:=tailLength; }}=>Nat.succ *r;"
	"v:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons {{ r:=tailLength; t:=tail; }}=>Nat.succ *r;"
	"o:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons { t:=tail; r:=tailLength; }=>Nat.succ *r;"
	"p:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons h t r rg=>Nat.succ *rg;"
	"hidden:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons {{ r:=tailLength; }}=>Nat.succ *r;"
	"shorthand:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons {{tail;tailLength;}}=>Nat.succ *tailLength;"
	"identity:=\\xs:List => \\n:Nat => \\g:@length xs n => g "
	"@nil=>Nat.zero @cons {{tail:=tail;tailLength:=tailLength;}}=>Nat.succ *tailLength;";

static void solve(struct pg_program *program, struct pg_synthesis_job *job, uint64_t chunk)
{
	assert(job);
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(program->synthesis.ready && program->synthesis.steps < 1000000);
		pg_synthesis_advance(&program->synthesis, chunk);
	}
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
}

static const struct pg_evidence *select(struct pg_program *p, const char *name, uint64_t chunk)
{
	struct pg_synthesis_job *job = pg_program_select_name(p, p->root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)});
	solve(p, job, chunk);
	return pg_synthesis_result(job);
}

/* Inspect accepted occurrences before normalization or application to an input.
	* Every cons branch must bind all four fields, including the dependent graph,
	* and the recursive field's IH; one visible selection cannot shrink this scope. */
static void telescope(const struct pg_occurrence *occurrence, size_t *matches)
{
	if (occurrence->origin) telescope(occurrence->origin, matches);
	if (occurrence->induction && !occurrence->origin) {
		const struct pg_induction_allocation *allocation = occurrence->induction;
		assert(allocation->count == 2);
		size_t count;
		assert(!pg_context_extension_size(allocation->clauses[1], allocation->clauses[0], &count));
		assert(count == 5);
		++*matches;
	}
	for (size_t i = 0; i < occurrence->operand_count; ++i)
		telescope(occurrence->operands[i], matches);
}

static void elaboration(struct pg_program *p, uint64_t chunk)
{
	solve(p, p->root, chunk);
	const struct pg_evidence *first = select(p, "u", chunk);
	const char *names[] = {"v", "o", "p", "hidden", "shorthand", "identity"};
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
		const struct pg_evidence *other = select(p, names[i], chunk);
		assert(pg_alpha_equal(pg_evidence_subject(first)->core, pg_evidence_subject(other)->core) == 1);
		assert(pg_alpha_equal(pg_evidence_classifier(first), pg_evidence_classifier(other)) == 1);
		size_t matches = 0;
		telescope(pg_evidence_subject(other), &matches);
		assert(matches == 1);
	}
}

static void roundtrip(uint64_t chunk)
{
	struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(p && p->root && !p->synthesis.steps);
	/* Unsolved and solved source images use ordinary checking after reload. */
	for (size_t phase = 0; phase < 2; ++phase) {
		if (phase) elaboration(p, chunk);
		FILE *file = tmpfile();
		const struct pg_synthesis_input roots[] = {{.pending = pg_synthesis_pending(p->root)}};
		assert(file && !pg_sources_write(file, &p->synthesis, 1, roots));
		pg_program_destroy(p);
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *loaded;
		p = pg_sources_read(file, 1000000, &count, &loaded);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(loaded[0]));
		p->root = loaded[0];
		assert(!fclose(file));
	}
	elaboration(p, chunk);
	pg_program_destroy(p);
}

static int token_is(struct pg_token token, const char *name)
{
	return token.kind == PG_TOKEN_IDENT && token.text_length == strlen(name)
		&& !memcmp(token.text, name, token.text_length);
}

static void syntax_contract(void)
{
	struct pg_graph arena = {0}, loaded_arena = {0};
	struct pg_parser parser;
	const char text[] = "x:=g @cons {{local:=source;}}=>local;";
	pg_parser_init(&parser, &arena, text, strlen(text));
	const struct pg_syntax *root = pg_parser_program(&parser);
	assert(root && !parser.error && !pg_syntax_validate(1, &root));
	const struct pg_syntax *clause = root->items[0].expression->items[0].expression;
	assert(clause->clause_bindings == PG_CLAUSE_UNORDERED);
	assert(token_is(clause->items[0].name, "local"));
	assert(token_is(clause->items[0].expression->token, "source"));
	FILE *file = tmpfile(), *copy = tmpfile();
	assert(file && copy && !pg_syntax_write(file, 1, &root));
	rewind(file);
	size_t count;
	const struct pg_syntax *const *loaded;
	assert(!pg_syntax_read(file, &loaded_arena, 10000, &count, &loaded));
	assert(count == 1 && !pg_syntax_validate(count, loaded));
	assert(!pg_syntax_write(copy, count, loaded));
	rewind(file); rewind(copy);
	int byte;
	do { byte = fgetc(file); assert(byte == fgetc(copy)); } while (byte != EOF);
	assert(!ferror(file) && !ferror(copy));
	const struct pg_syntax *saved_clause = loaded[0]->items[0].expression->items[0].expression;
	assert(saved_clause->clause_bindings == PG_CLAUSE_UNORDERED);
	assert(token_is(saved_clause->items[0].name, "local"));
	assert(token_is(saved_clause->items[0].expression->token, "source"));
	/* The historical version must fail before publishing reconstructed roots. */
	assert(!fseek(file, 6, SEEK_SET) && fputc(1, file) != EOF && !fflush(file));
	rewind(file);
	count = 123;
	loaded = &root;
	assert(pg_syntax_read(file, &loaded_arena, 10000, &count, &loaded) == -1);
	assert(count == 123 && loaded == &root);
	struct pg_syntax bad = *clause;
	const struct pg_syntax *invalid = &bad;
	bad.clause_bindings = PG_CLAUSE_POSITIONAL;
	assert(pg_syntax_validate(1, &invalid) == -1);
	bad.clause_bindings = (enum pg_clause_bindings)99;
	assert(pg_syntax_validate(1, &invalid) == -1);
	bad = *root;
	bad.clause_bindings = PG_CLAUSE_UNORDERED;
	assert(pg_syntax_validate(1, &invalid) == -1);
	assert(!fclose(file) && !fclose(copy));
	pg_graph_destroy(&loaded_arena);
	pg_graph_destroy(&arena);
}

static void scope_scan(void)
{
	const char *texts[] = {
		"D:=@\\local:@ => {c:(x @cons {{local:=source;}}=>local)->*;};",
		"D:=@\\source:@ => {c:(x @cons {{local:=source;}}=>source)->*;};",
		"D:=@\\source:@ => {c:(x @cons {{local:=source;}}=>local)->*;};"
	};
	for (size_t i = 0; i < sizeof(texts) / sizeof(*texts); ++i) {
		struct pg_graph arena = {0};
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &arena, texts[i], strlen(texts[i]));
		assert(pg_parser_next(&parser, &definition) == 1);
		size_t count;
		assert(pg_syntax_constructor_telescope(&arena, definition.expression, 0, &count));
		assert(count == (i == 1));
		pg_graph_destroy(&arena);
	}
}

int main(void)
{
	syntax_contract();
	scope_scan();
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) roundtrip(chunk);
	puts("surface: LHS scope, version rejection, exact syntax transport, canonical typed telescopes and unnormalized alpha equivalence passed");
	return 0;
}
