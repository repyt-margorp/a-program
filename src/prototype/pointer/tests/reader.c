#include "reader.h"
#include "syntax.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void tokens(void)
{
	const char input[] = "// comment\nVec := \\A : @ => @\\n:Nat => { nil:* Nat.zero; cons:(k:Nat)->A->* k; }; main :: T; &{x:=#get; !x;}.x @#return v => v;";
	const int expected[] = {
		PG_TOKEN_IDENT, PG_TOKEN_ASSIGN, '\\', PG_TOKEN_IDENT, ':', '@', PG_TOKEN_LAMBDA_ARROW,
		'@', '\\', PG_TOKEN_IDENT, ':', PG_TOKEN_IDENT, PG_TOKEN_LAMBDA_ARROW, '{',
		PG_TOKEN_IDENT, ':', '*', PG_TOKEN_IDENT, '.', PG_TOKEN_IDENT, ';',
		PG_TOKEN_IDENT, ':', '(', PG_TOKEN_IDENT, ':', PG_TOKEN_IDENT, ')', PG_TOKEN_ARROW,
		PG_TOKEN_IDENT, PG_TOKEN_ARROW, '*', PG_TOKEN_IDENT, ';', '}', ';',
		PG_TOKEN_IDENT, PG_TOKEN_EXPECT, PG_TOKEN_IDENT, ';', '&', '{', PG_TOKEN_IDENT,
		PG_TOKEN_ASSIGN, '#', PG_TOKEN_IDENT, ';', '!', PG_TOKEN_IDENT, ';', '}', '.',
		PG_TOKEN_IDENT, '@', '#', PG_TOKEN_IDENT, PG_TOKEN_IDENT, PG_TOKEN_LAMBDA_ARROW,
		PG_TOKEN_IDENT, ';', PG_TOKEN_EOF
	};
	struct pg_reader reader;
	pg_reader_init(&reader, input, sizeof(input) - 1);
	for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
		assert(pg_reader_next(&reader) == expected[i]);
		if (!i) {
			assert(reader.token.line == 2 && reader.token.column == 1);
			assert(reader.token.text_length == 3);
			assert(memcmp(reader.token.text, "Vec", 3) == 0);
		}
	}
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
	const char words[] = "perform handle with return import let in";
	pg_reader_init(&reader, words, sizeof(words) - 1);
	for (size_t i = 0; i < 7; ++i) assert(pg_reader_next(&reader) == PG_TOKEN_IDENT);
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
}

static void literals(void)
{
	const char input[] = "#0 #-0 #9223372036854775807 #-9223372036854775808 #\"hi\" #\"\" #\"\nline\" #\"END\r\na\"b\nEND\"";
	struct pg_reader reader;
	pg_reader_init(&reader, input, sizeof(input) - 1);
	const int64_t values[] = {0, 0, INT64_MAX, INT64_MIN};
	for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i) {
		assert(pg_reader_next(&reader) == PG_TOKEN_INT);
		assert(reader.token.integer == values[i]);
	}
	const char *texts[] = {"hi", "", "line", "a\"b\n"};
	for (size_t i = 0; i < sizeof(texts) / sizeof(*texts); ++i) {
		assert(pg_reader_next(&reader) == PG_TOKEN_TEXT);
		assert(reader.token.text_length == strlen(texts[i]));
		assert(memcmp(reader.token.text, texts[i], strlen(texts[i])) == 0);
	}
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
}

static void failures(void)
{
	const char *invalid[] = {"/*", "/* unterminated", "#-", "#9223372036854775808",
		"#-9223372036854775809", "#\"abc", "#\"END\nmissing", "=", "-", "/", "["};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		struct pg_reader reader;
		pg_reader_init(&reader, invalid[i], strlen(invalid[i]));
		assert(pg_reader_next(&reader) == PG_TOKEN_ERROR);
		assert(reader.error);
		assert(pg_reader_next(&reader) == PG_TOKEN_ERROR);
	}
	struct pg_reader reader;
	const char bounded[] = {'x', ':', '=', '#', '1'};
	pg_reader_init(&reader, bounded, sizeof(bounded));
	assert(pg_reader_next(&reader) == PG_TOKEN_IDENT);
	assert(pg_reader_next(&reader) == PG_TOKEN_ASSIGN);
	assert(pg_reader_next(&reader) == PG_TOKEN_INT);
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
	pg_reader_init(&reader, "/**/", 4);
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
	pg_reader_init(&reader, NULL, 0);
	assert(pg_reader_next(&reader) == PG_TOKEN_EOF);
	pg_reader_init(&reader, NULL, 1);
	assert(pg_reader_next(&reader) == PG_TOKEN_ERROR);
	const char nul[] = {'\0'};
	pg_reader_init(&reader, nul, sizeof(nul));
	assert(pg_reader_next(&reader) == PG_TOKEN_ERROR);
}

static void prefixes(void)
{
	const char source[] = "/* comment */ main := #\"END\r\nbody\"text\nEND\"; // end\n";
	for (size_t length = 0; length < sizeof(source); ++length) {
		struct pg_reader reader;
		pg_reader_init(&reader, source, length);
		for (;;) {
			size_t before = reader.position;
			int token = pg_reader_next(&reader);
			assert(reader.position <= length);
			if (token == PG_TOKEN_EOF || token == PG_TOKEN_ERROR) break;
			assert(reader.position > before);
		}
	}
}

static void syntax(void)
{
	const char source[] = "id := \\x : A => x; main := id #1; main :: A; f := \\g : (x:A) -> B x => g; checked := (f a) :: B; call := f a b; delayed := &id; text := #\"hello\"; t := #Int; type := @;";
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, source, sizeof(source) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.operation == PG_TOKEN_ASSIGN);
	assert(definition.expression->kind == PG_SYNTAX_LAMBDA);
	assert(definition.expression->token.text_length == 1);
	assert(definition.expression->left->token.text[0] == 'A');
	assert(definition.expression->right->token.text[0] == 'x');
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_APPLICATION);
	assert(definition.expression->right->token.integer == 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.operation == PG_TOKEN_EXPECT);
	assert(definition.expression->kind == PG_SYNTAX_ATOM);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *domain = definition.expression->left;
	assert(domain->kind == PG_SYNTAX_PI);
	assert(domain->left->kind == PG_SYNTAX_BINDER);
	assert(domain->left->token.text[0] == 'x');
	assert(domain->right->kind == PG_SYNTAX_APPLICATION);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_EXPECT);
	assert(definition.expression->left->kind == PG_SYNTAX_APPLICATION);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_APPLICATION);
	assert(definition.expression->left->kind == PG_SYNTAX_APPLICATION);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_QUOTE);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->token.kind == PG_TOKEN_TEXT);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_QUALIFIED);
	assert(definition.expression->left->token.kind == '#');
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->token.kind == '@');
	assert(pg_parser_next(&parser, &definition) == 0);
	const char *invalid[] = {"id := \\x => x;", "x := (x:A);", "x := f (x:A);",
		"x := (f a;", "x := f", "x := b @true => ;", "T := @{c:*};"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		assert(pg_parser_next(&parser, &definition) == -1);
		assert(parser.error);
	}
	pg_graph_destroy(&arena);
	puts("syntax: lambda annotations, dependent Pi, applications and separate expect nodes passed");
}

static void eliminations(void)
{
	const char source[] =
		"add := \\n:Nat => n @zero => \\m:Nat => m @succ k => \\m:Nat => Nat.succ (*k m);"
		"handled := M @#get req k => k default @print_alias req k => k unit @#return result => result;"
		"nested := b @true => (b @true => x @false => y) @false => z;"
		"selected := g @step {arg:=x; proof;} => x;";
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, source, sizeof(source) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *match = definition.expression->right;
	assert(match->kind == PG_SYNTAX_ELIMINATION && match->item_count == 2);
	assert(match->items[0].expression->right->kind == PG_SYNTAX_LAMBDA);
	assert(match->items[1].expression->item_count == 1);
	assert(match->items[1].expression->items[0].name.text[0] == 'k');
	assert(match->items[1].expression->right->kind == PG_SYNTAX_LAMBDA);
	assert(pg_parser_next(&parser, &definition) == 1);
	match = definition.expression;
	assert(match->kind == PG_SYNTAX_ELIMINATION && match->item_count == 3);
	assert(match->items[0].expression->left->kind == PG_SYNTAX_QUALIFIED);
	assert(match->items[0].expression->item_count == 2);
	assert(match->items[1].expression->left->token.text_length == strlen("print_alias"));
	assert(match->items[2].expression->left->right->token.text_length == strlen("return"));
	assert(match->items[2].expression->item_count == 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	match = definition.expression;
	assert(match->item_count == 2);
	assert(match->items[0].expression->right->kind == PG_SYNTAX_ELIMINATION);
	assert(match->items[0].expression->right->item_count == 2);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *clause = definition.expression->items[0].expression;
	assert(clause->item_count == 2);
	assert(clause->items[0].operation == PG_TOKEN_ASSIGN);
	assert(clause->items[0].expression->token.text[0] == 'x');
	assert(pg_parser_next(&parser, &definition) == 0);
	const char *invalid[] = {"m:=x @;", "m:=x @c p {q;} =>x;", "m:=x @c {p:=;} =>x;",
		"m:=x @c p =>;", "m:=x @c =>;"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		assert(pg_parser_next(&parser, &definition) == -1);
	}
	pg_graph_destroy(&arena);
	puts("elimination syntax: clause arrays, alias heads, return labels, nested grouping and selectors passed");
}

static void companions(void)
{
	const char source[] = "import Sigma; f:=\\le:A=>\\@le:G=>\\*le=>@le; p:=m @output=>inspect x output @output;";
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, source, sizeof(source) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.operation == PG_SYNTAX_IMPORT);
	assert(definition.expression->left->token.text_length == 5);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *raw = definition.expression;
	assert(raw->binder_marker == 0 && raw->left);
	const struct pg_syntax *graph = raw->right;
	assert(graph->binder_marker == '@' && graph->left);
	const struct pg_syntax *certified = graph->right;
	assert(certified->binder_marker == '*' && !certified->left);
	assert(certified->right->kind == PG_SYNTAX_GRAPH_REFERENCE);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *body = definition.expression->items[0].expression->right;
	assert(body->kind == PG_SYNTAX_APPLICATION);
	assert(body->right->kind == PG_SYNTAX_GRAPH_REFERENCE);
	assert(pg_parser_next(&parser, &definition) == 0);
	const char root[] = "{{import Sigma; main:=x;}}.main";
	pg_parser_init(&parser, &arena, root, sizeof(root) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->left->items[0].operation == PG_SYNTAX_IMPORT);
	const char *invalid[] = {"import;", "import X.Y;", "f:=\\@x=>x;", "f:=\\*x:A=>x;", "f:=#;"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		assert(pg_parser_next(&parser, &definition) == -1);
	}
	pg_graph_destroy(&arena);
	puts("companions: import syntax, marker retention and graph arguments without type fabrication passed");
}

static void blocks(void)
{
	const char source[] = "f := \\x:A => {a:A:=x; #print #\"hello\"; b:={x;}; !a; later:=x;}.a; delayed:=&{#get;};";
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, source, sizeof(source) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *selection = definition.expression->right;
	assert(selection->kind == PG_SYNTAX_QUALIFIED);
	const struct pg_syntax *body = selection->left;
	assert(body->kind == PG_SYNTAX_BLOCK && body->item_count == 5);
	assert(body->items[0].operation == PG_TOKEN_ASSIGN);
	assert(body->items[0].annotation->token.text[0] == 'A');
	assert(body->items[1].name.kind == 0);
	assert(body->items[1].expression->kind == PG_SYNTAX_APPLICATION);
	assert(body->items[2].expression->kind == PG_SYNTAX_BLOCK);
	assert(body->items[3].expression->kind == PG_SYNTAX_EXIT);
	assert(body->items[4].name.text_length == strlen("later"));
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_QUOTE);
	assert(definition.expression->left->kind == PG_SYNTAX_BLOCK);
	assert(pg_parser_next(&parser, &definition) == 0);
	const char root[] = "{{id:=\\x:A=>x; main:=id #1; main::A;}}.main;";
	pg_parser_init(&parser, &arena, root, sizeof(root) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.operation == '{');
	body = definition.expression->left;
	assert(body->kind == PG_SYNTAX_DEFINITIONS && body->item_count == 3);
	assert(body->items[2].operation == PG_TOKEN_EXPECT);
	assert(pg_parser_next(&parser, &definition) == 0);
	const char *invalid[] = {"f:={};", "{{x:=#1;}}", "{x:=#1;}", "{{x:=#1;}}.x extra;",
		"f:={x:=#1};", "{{!x;}}.x", "{{#get;}}.x", "f:={x;", "x:=#1; {{y:=#2;}}.y"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		int status;
		do status = pg_parser_next(&parser, &definition); while (status == 1);
		assert(status == -1);
	}
	pg_graph_destroy(&arena);
	puts("blocks: ordered named/unnamed items, selected result, exits and root-only definitions passed");
}

static size_t self_count(const struct pg_syntax *syntax)
{
	if (!syntax) return 0;
	size_t count = syntax->kind == PG_SYNTAX_ATOM && syntax->token.kind == '*';
	count += self_count(syntax->left) + self_count(syntax->right);
	for (size_t i = 0; i < syntax->item_count; ++i) count += self_count(syntax->items[i].expression);
	return count;
}

static void declarations(void)
{
	const char source[] =
		"List := \\A:@ => @{nil:*; cons:A->*;};"
		"Vec := \\A:@ => @\\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->A->* k->* (Nat.succ k);};"
		"Acc := \\A:@ => \\R:A->A->@ => @\\subject:A => {acc:(x:A)->((y:A)->R y x->* y)->* x;};"
		"Empty := @{};"
		"Many := @{a:*; b:*; c:*; d:*; e:*; f:*; g:*; h:*; i:*;};";
	struct pg_graph arena = {0};
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &arena, source, sizeof(source) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_LAMBDA);
	const struct pg_syntax *declaration = definition.expression->right;
	assert(declaration->kind == PG_SYNTAX_DECLARATION);
	assert(declaration->left->kind == PG_SYNTAX_CONSTRUCTORS);
	assert(declaration->left->item_count == 2);
	assert(self_count(declaration) == 2);
	assert(pg_parser_next(&parser, &definition) == 1);
	declaration = definition.expression->right;
	assert(declaration->kind == PG_SYNTAX_DECLARATION);
	assert(declaration->left->kind == PG_SYNTAX_LAMBDA);
	assert(declaration->left->token.text[0] == 'n');
	assert(declaration->left->right->item_count == 2);
	assert(self_count(declaration) == 3);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->right->kind == PG_SYNTAX_LAMBDA);
	declaration = definition.expression->right->right;
	assert(declaration->kind == PG_SYNTAX_DECLARATION);
	assert(declaration->left->token.text_length == strlen("subject"));
	assert(declaration->left->right->item_count == 1);
	assert(self_count(declaration) == 2);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->left->item_count == 0);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->left->item_count == 9);
	assert(pg_parser_next(&parser, &definition) == 0);
	const char *invalid[] = {"T := @\\n:A => ;", "T := @{a:*;", "T := @{a:=*;};"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		assert(pg_parser_next(&parser, &definition) == -1);
	}
	const char repeated[] = "Rel:=@\\x:A=>@\\y:A=>{r:(z:A)->* z z;}; id:=\\A:@=>x:A=>x; grouped:=(x:A=>x);";
	pg_parser_init(&parser, &arena, repeated, sizeof(repeated) - 1);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->left->right->kind == PG_SYNTAX_LAMBDA);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->right->kind == PG_SYNTAX_LAMBDA);
	assert(pg_parser_next(&parser, &definition) == 1);
	assert(definition.expression->kind == PG_SYNTAX_LAMBDA);
	assert(pg_parser_next(&parser, &definition) == 0);
	pg_graph_destroy(&arena);
	puts("declarations: parameter/index separation, List/Vec/Acc self markers and constructor arrays passed");
}

static void programs(void)
{
	struct pg_graph arena;
	struct pg_parser parser;
	assert(pg_graph_init(&arena) == 0);
	const char *sources[] = {
		"main := later; main :: A; import Library; later := { x; };",
		"{{main := later; main :: A; import Library; later := { x; };}}.main"
	};
	for (size_t i = 0; i < 2; ++i) {
		pg_parser_init(&parser, &arena, sources[i], strlen(sources[i]));
		const struct pg_syntax *root = pg_parser_program(&parser);
		assert(root);
		if (i) {
			assert(root->kind == PG_SYNTAX_QUALIFIED);
			assert(root->right->token.length == 4);
			assert(memcmp(root->right->token.text, "main", 4) == 0);
			root = root->left;
		}
		assert(root->kind == PG_SYNTAX_DEFINITIONS && root->item_count == 4);
		assert(root->items[0].operation == PG_TOKEN_ASSIGN);
		assert(root->items[0].expression->kind == PG_SYNTAX_ATOM);
		assert(root->items[1].operation == PG_TOKEN_EXPECT);
		assert(root->items[2].operation == PG_SYNTAX_IMPORT);
		assert(root->items[3].expression->kind == PG_SYNTAX_BLOCK);
		assert(parser.reader.token.kind == PG_TOKEN_EOF);
		assert(!pg_parser_program(&parser));
	}
	pg_parser_init(&parser, &arena, "", 0);
	const struct pg_syntax *empty = pg_parser_program(&parser);
	assert(empty && empty->kind == PG_SYNTAX_DEFINITIONS && !empty->item_count);
	pg_parser_init(&parser, &arena, sources[0], strlen(sources[0]));
	struct pg_definition first;
	assert(pg_parser_next(&parser, &first) == 1);
	assert(!pg_parser_program(&parser) && parser.error);
	const char *invalid[] = {"x:=A; broken", "x:=A; {{y:=B;}}.y", "{{x:=A;}}.x trailing"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, &arena, invalid[i], strlen(invalid[i]));
		assert(!pg_parser_program(&parser) && parser.error);
	}
	assert(arena.terms.count == 0);
	pg_graph_destroy(&arena);
	puts("program syntax: flat/explicit definition arrays retain checks, imports and selection without Core construction");
}

static void marked_applications(void)
{
	const char *sources[] = {"r:=f *x;", "r:=f (*x);", "r:=f * x;", "r:=f *(x);"};
	struct pg_graph arena = {0};
	for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &arena, sources[i], strlen(sources[i]));
		assert(pg_parser_next(&parser, &definition) == 1);
		const struct pg_syntax *application = definition.expression;
		assert(application->kind == PG_SYNTAX_APPLICATION && application->left->kind == PG_SYNTAX_ATOM);
		assert(application->left->token.text[0] == 'f');
		assert(application->right->kind == PG_SYNTAX_APPLICATION);
		assert(application->right->left->token.kind == '*');
		assert(application->right->right->token.text[0] == 'x');
	}
	assert(!arena.terms.count);
	pg_graph_destroy(&arena);
}

static void intrinsic_names(void)
{
	const char *spellings[] = {"#Int", "#.Int", "#Int.member", "#.Int.member"};
	for (size_t i = 0; i < sizeof(spellings) / sizeof(*spellings); ++i) {
		char input[80];
		snprintf(input, sizeof(input), "name:=%s;", spellings[i]);
		struct pg_graph arena = {0};
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &arena, input, strlen(input));
		if (i % 2) {
			assert(pg_parser_next(&parser, &definition) == -1);
			assert(strstr(parser.error, "--legacy-intrinsic-dot"));
			pg_parser_init(&parser, &arena, input, strlen(input));
			parser.allow_legacy_intrinsic_dot = 1;
		}
		assert(pg_parser_next(&parser, &definition) == 1);
		const struct pg_syntax *qualified = definition.expression;
		assert(qualified->kind == PG_SYNTAX_QUALIFIED);
		if (i >= 2) {
			assert(qualified->right->token.text_length == 6);
			qualified = qualified->left;
		}
		assert(qualified->kind == PG_SYNTAX_QUALIFIED);
		assert(qualified->left->kind == PG_SYNTAX_ATOM && qualified->left->token.kind == '#');
		assert(qualified->right->token.text_length == 3 && !memcmp(qualified->right->token.text, "Int", 3));
		pg_graph_destroy(&arena);
	}
	puts("intrinsic names: #Name and #.Name use ordinary qualified lookup");
}

int main(void)
{
	intrinsic_names();
	tokens();
	literals();
	failures();
	prefixes();
	syntax();
	declarations();
	blocks();
	eliminations();
	marked_applications();
	companions();
	programs();
	puts("reader: symbolic syntax, contextual names, literals, comments and bounded input passed");
	return 0;
}
