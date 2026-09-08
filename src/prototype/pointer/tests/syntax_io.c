#include "syntax_io.h"
#include "synthesis.h"
#include <assert.h>
#include <string.h>

static void compare_files(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int x, y;
	do { x = fgetc(a); y = fgetc(b); assert(x == y); } while (x != EOF);
	assert(!ferror(a) && !ferror(b));
}

static void run(struct pg_graph *graph, const struct pg_syntax *syntax)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_typing_init(&typing, graph) && !pg_classifiers_init(&classifiers, graph));
	assert(!pg_whnf_work_init(&work, graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *job = pg_synthesis_request(&synthesis, pg_synthesis_root(&synthesis), syntax);
	assert(job && !synthesis.steps && !pg_synthesis_result(job));
	while (synthesis.ready) { assert(synthesis.steps < 10000); pg_synthesis_advance(&synthesis, 1); }
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
}

int main(void)
{
	struct pg_graph source, destination;
	assert(!pg_graph_init(&source) && !pg_graph_init(&destination));
	const char text[] = "{{ id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; }}.id";
	struct pg_parser parser;
	pg_parser_init(&parser, &source, text, strlen(text));
	const struct pg_syntax *program = pg_parser_program(&parser);
	assert(program && !parser.error);
	struct pg_syntax nodes[PG_SYNTAX_IMPORT + 1] = {0};
	const struct pg_syntax_item item = {.name = {.kind = PG_TOKEN_IDENT, .text = "x", .text_length = 1},
		.expression = program, .annotation = program, .operation = PG_TOKEN_EXPECT};
	for (size_t i = 0; i <= PG_SYNTAX_IMPORT; ++i) {
		nodes[i] = (struct pg_syntax){.kind = i, .left = i ? &nodes[i - 1] : NULL,
			.right = program, .items = &item, .item_count = 1, .binder_marker = '*',
			.token = {.kind = PG_TOKEN_TEXT, .text = "a\0b", .text_length = 3,
				.offset = 2, .length = 5, .line = 3, .column = 4, .integer = INT64_MIN}};
	}
	const struct pg_syntax *roots[] = {program, &nodes[PG_SYNTAX_IMPORT], program};
	FILE *file = tmpfile(), *copy = tmpfile();
	assert(file && copy && !pg_syntax_write(file, 3, roots));
	rewind(file);
	size_t count;
	const struct pg_syntax *const *loaded;
	assert(!pg_syntax_read(file, &destination, 10000, &count, &loaded));
	assert(count == 3 && loaded[0] == loaded[2] && loaded[0] != program);
	assert(loaded[1]->right == loaded[0] && loaded[1]->items[0].expression == loaded[0]);
	assert(loaded[1]->items[0].annotation == loaded[0]);
	assert(loaded[1]->token.integer == INT64_MIN && !memcmp(loaded[1]->token.text, "a\0b", 3));
	assert(!source.terms.count && !destination.terms.count);
	assert(!pg_syntax_write(copy, count, loaded));
	compare_files(file, copy);
	run(&destination, loaded[0]);
	/* Every truncated prefix leaves the output parameters untouched. */
	rewind(file);
	FILE *prefix = tmpfile();
	assert(prefix);
	int byte;
	while ((byte = fgetc(file)) != EOF) {
		struct pg_graph incomplete;
		assert(!pg_graph_init(&incomplete));
		rewind(prefix);
		size_t sentinel = 123;
		const struct pg_syntax *const *unchanged = roots;
		assert(pg_syntax_read(prefix, &incomplete, 10000, &sentinel, &unchanged) == -1);
		assert(sentinel == 123 && unchanged == roots);
		pg_graph_destroy(&incomplete);
		assert(!fseek(prefix, 0, SEEK_END) && fputc(byte, prefix) != EOF && !fflush(prefix));
	}
	assert(!fclose(prefix));
	/* Shared deep syntax is transported iteratively; cycles remain invalid. */
	const struct pg_syntax *chain = program;
	for (size_t i = 0; i < 40000; ++i) {
		struct pg_syntax *s = pg_alloc(&source, sizeof(*s));
		assert(s);
		*s = (struct pg_syntax){.kind = PG_SYNTAX_APPLICATION, .left = chain, .right = chain};
		chain = s;
	}
	FILE *deep = tmpfile();
	assert(deep && !pg_syntax_write(deep, 1, &chain));
	rewind(deep);
	assert(!pg_syntax_read(deep, &destination, 50000, &count, &loaded));
	assert(count == 1 && loaded[0]->left == loaded[0]->right);
	assert(!fclose(deep));
	nodes[0].left = &nodes[PG_SYNTAX_IMPORT];
	assert(pg_syntax_write(copy, 3, roots) == -1);
	assert(!fclose(file) && !fclose(copy));
	pg_graph_destroy(&destination); pg_graph_destroy(&source);
	puts("syntax image: exact DAG/token transport, ordinary Solve, sparse edges, truncation and deep sharing passed");
	return 0;
}
