#include "syntax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct replacement {
	size_t offset, length, text_length;
	const char *text;
};

struct migration {
	const char *source;
	size_t length, count, capacity, clauses;
	struct replacement *replacements;
};

static int replace(struct migration *migration, size_t offset, size_t length,
	const char *text, size_t text_length)
{
	if (offset > migration->length || length > migration->length - offset) return -1;
	if (migration->count == migration->capacity) {
		size_t capacity = migration->capacity ? 2 * migration->capacity : 16;
		if (capacity < migration->capacity || capacity > SIZE_MAX / sizeof(struct replacement)) return -1;
		void *next = realloc(migration->replacements, capacity * sizeof(struct replacement));
		if (!next) return -1;
		migration->replacements = next;
		migration->capacity = capacity;
	}
	migration->replacements[migration->count++] = (struct replacement){offset, length, text_length, text};
	return 0;
}

/* This tool operates only under an explicit legacy-source contract. It uses
	* parsed clause/token boundaries, never selector lookup or textual name guesses. */
static int collect(struct migration *migration, const struct pg_syntax *syntax)
{
	if (!syntax) return 0;
	if (syntax->kind == PG_SYNTAX_CLAUSE && syntax->clause_bindings) {
		if (syntax->clause_bindings != PG_CLAUSE_ORDERED || !syntax->item_count) return -1;
		size_t first = syntax->items[0].name.offset;
		struct pg_reader reader;
		pg_reader_init(&reader, migration->source, migration->length);
		size_t opening = SIZE_MAX, closing = SIZE_MAX;
		while (pg_reader_next(&reader) != PG_TOKEN_EOF) {
			if (reader.token.kind == PG_TOKEN_ERROR) return -1;
			if (reader.token.offset >= syntax->token.offset && reader.token.offset < first
				&& reader.token.kind == '{') opening = reader.token.offset;
			if (reader.token.offset > syntax->items[syntax->item_count - 1].expression->token.offset
				&& reader.token.kind == '}') { closing = reader.token.offset; break; }
		}
		if (opening == SIZE_MAX || closing == SIZE_MAX) return -1;
		if (replace(migration, opening, 0, "{", 1) || replace(migration, closing, 0, "}", 1)) return -1;
		for (size_t i = 0; i < syntax->item_count; ++i) {
			const struct pg_syntax_item *item = &syntax->items[i];
			struct pg_token source = item->name, local = item->expression->token;
			if (source.offset == local.offset) continue;
			if (replace(migration, source.offset, source.length, local.text, local.text_length)
				|| replace(migration, local.offset, local.length, source.text, source.text_length)) return -1;
		}
		++migration->clauses;
	}
	if (collect(migration, syntax->left) || collect(migration, syntax->right)) return -1;
	for (size_t i = 0; i < syntax->item_count; ++i)
		if (collect(migration, syntax->items[i].expression) || collect(migration, syntax->items[i].annotation)) return -1;
	return 0;
}

static int compare(const void *first, const void *second)
{
	const struct replacement *a = first, *b = second;
	return (a->offset > b->offset) - (a->offset < b->offset);
}

int main(int argc, char **argv)
{
	if (argc != 3 || strcmp(argv[1], "--legacy-source")) {
		fputs("usage: migrate --legacy-source FILE.p > MIGRATED.p\n", stderr);
		return 2;
	}
	FILE *file = fopen(argv[2], "rb");
	if (!file || fseek(file, 0, SEEK_END)) return 2;
	long size = ftell(file);
	if (size < 0 || fseek(file, 0, SEEK_SET)) { fclose(file); return 2; }
	char *source = malloc((size_t)size + 1);
	if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) { free(source); fclose(file); return 2; }
	fclose(file);
	struct pg_graph arena = {0};
	struct pg_parser parser;
	pg_parser_init(&parser, &arena, source, (size_t)size);
	parser.allow_legacy_intrinsic_dot = 1;
	const struct pg_syntax *syntax = pg_parser_program(&parser);
	struct migration migration = {.source = source, .length = (size_t)size};
	int result = 2;
	if (!syntax || parser.error || collect(&migration, syntax)) {
		fputs("cannot migrate parsed legacy source\n", stderr);
		goto done;
	}
	if (migration.count > 1)
		qsort(migration.replacements, migration.count, sizeof(struct replacement), compare);
	size_t offset = 0;
	for (size_t i = 0; i < migration.count; ++i) {
		const struct replacement *replacement = &migration.replacements[i];
		if (replacement->offset < offset) goto done;
		if (fwrite(source + offset, 1, replacement->offset - offset, stdout) != replacement->offset - offset
			|| fwrite(replacement->text, 1, replacement->text_length, stdout) != replacement->text_length) goto done;
		offset = replacement->offset + replacement->length;
	}
	if (fwrite(source + offset, 1, (size_t)size - offset, stdout) != (size_t)size - offset || fflush(stdout)) goto done;
	fprintf(stderr, "migrated %zu named clauses\n", migration.clauses);
	result = 0;
done:
	free(migration.replacements);
	pg_graph_destroy(&arena);
	free(source);
	return result;
}
