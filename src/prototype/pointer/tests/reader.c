#include "reader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void tokens(void)
{
	const char input[] = "// comment\nVec := \\A : @ => @\\n:Nat => { nil:* Nat.zero; cons:(k:Nat)->A->* k; }; main :: T; &{x:=#.get; !x;}.x @#.return v => v;";
	const int expected[] = {
		PG_TOKEN_IDENT, PG_TOKEN_ASSIGN, '\\', PG_TOKEN_IDENT, ':', '@', PG_TOKEN_LAMBDA_ARROW,
		'@', '\\', PG_TOKEN_IDENT, ':', PG_TOKEN_IDENT, PG_TOKEN_LAMBDA_ARROW, '{',
		PG_TOKEN_IDENT, ':', '*', PG_TOKEN_IDENT, '.', PG_TOKEN_IDENT, ';',
		PG_TOKEN_IDENT, ':', '(', PG_TOKEN_IDENT, ':', PG_TOKEN_IDENT, ')', PG_TOKEN_ARROW,
		PG_TOKEN_IDENT, PG_TOKEN_ARROW, '*', PG_TOKEN_IDENT, ';', '}', ';',
		PG_TOKEN_IDENT, PG_TOKEN_EXPECT, PG_TOKEN_IDENT, ';', '&', '{', PG_TOKEN_IDENT,
		PG_TOKEN_ASSIGN, '#', '.', PG_TOKEN_IDENT, ';', '!', PG_TOKEN_IDENT, ';', '}', '.',
		PG_TOKEN_IDENT, '@', '#', '.', PG_TOKEN_IDENT, PG_TOKEN_IDENT, PG_TOKEN_LAMBDA_ARROW,
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

int main(void)
{
	tokens();
	literals();
	failures();
	prefixes();
	puts("reader: symbolic syntax, contextual names, literals, comments and bounded input passed");
	return 0;
}
