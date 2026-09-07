#include "reader.h"

#include <string.h>

static int peek(const struct pg_reader *reader, size_t ahead)
{
	if (ahead >= reader->length - reader->position) return -1;
	return (unsigned char)reader->input[reader->position + ahead];
}

static void take(struct pg_reader *reader)
{
	if (peek(reader, 0) == '\n') {
		++reader->line;
		reader->column = 1;
	} else {
		++reader->column;
	}
	++reader->position;
}

static int letter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int digit(int c)
{
	return c >= '0' && c <= '9';
}

static int name_char(int c)
{
	return letter(c) || digit(c);
}

static int fail(struct pg_reader *reader, const char *message)
{
	reader->error = message;
	reader->token.kind = PG_TOKEN_ERROR;
	reader->token.length = reader->position - reader->token.offset;
	return PG_TOKEN_ERROR;
}

static int finish(struct pg_reader *reader, int kind)
{
	reader->token.kind = kind;
	reader->token.length = reader->position - reader->token.offset;
	return kind;
}

static void start(struct pg_reader *reader)
{
	reader->token = (struct pg_token){.offset = reader->position,
		.line = reader->line, .column = reader->column};
}

void pg_reader_init(struct pg_reader *reader, const char *input, size_t length)
{
	*reader = (struct pg_reader){.input = input, .length = length, .line = 1, .column = 1};
	if (!input && length) fail(reader, "missing input buffer");
}

static int skip(struct pg_reader *reader)
{
	for (;;) {
		int c = peek(reader, 0);
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			take(reader);
			continue;
		}
		if (c != '/') return 0;
		int next = peek(reader, 1);
		if (next == '/') {
			while (peek(reader, 0) != -1 && peek(reader, 0) != '\n') take(reader);
			continue;
		}
		if (next != '*') return 0;
		start(reader);
		take(reader);
		take(reader);
		for (;;) {
			if (peek(reader, 0) == -1) return fail(reader, "unterminated block comment");
			if (peek(reader, 0) == '*' && peek(reader, 1) == '/') {
				take(reader);
				take(reader);
				break;
			}
			take(reader);
		}
	}
}

static int text_token(struct pg_reader *reader)
{
	take(reader);
	take(reader);
	size_t delimiter_start = reader->position;
	size_t delimiter_length = 0;
	if (letter(peek(reader, 0))) {
		size_t n = 0;
		while (name_char(peek(reader, n))) ++n;
		int next = peek(reader, n);
		if (next == '\n' || (next == '\r' && peek(reader, n + 1) == '\n')) {
			delimiter_length = n;
			for (size_t i = 0; i < n; ++i) take(reader);
		}
	}
	if (peek(reader, 0) == '\r' && peek(reader, 1) == '\n') take(reader);
	if (peek(reader, 0) == '\n') take(reader);
	size_t body = reader->position;
	for (;;) {
		if (peek(reader, 0) == -1) return fail(reader, "unterminated text literal");
		if (peek(reader, delimiter_length) == '"' &&
			memcmp(reader->input + reader->position, reader->input + delimiter_start, delimiter_length) == 0) {
			reader->token.text = reader->input + body;
			reader->token.text_length = reader->position - body;
			for (size_t i = 0; i <= delimiter_length; ++i) take(reader);
			return finish(reader, PG_TOKEN_TEXT);
		}
		take(reader);
	}
}

static int int_token(struct pg_reader *reader)
{
	take(reader);
	int negative = peek(reader, 0) == '-';
	if (negative) take(reader);
	if (!digit(peek(reader, 0))) return fail(reader, "expected integer digits");
	uint64_t limit = (uint64_t)INT64_MAX + (unsigned)negative;
	uint64_t magnitude = 0;
	while (digit(peek(reader, 0))) {
		unsigned value = (unsigned)(peek(reader, 0) - '0');
		if (magnitude > (limit - value) / 10) return fail(reader, "integer literal outside Int64 range");
		magnitude = magnitude * 10 + value;
		take(reader);
	}
	if (!negative) reader->token.integer = (int64_t)magnitude;
	else if (magnitude == (uint64_t)INT64_MAX + 1) reader->token.integer = INT64_MIN;
	else reader->token.integer = -(int64_t)magnitude;
	return finish(reader, PG_TOKEN_INT);
}

int pg_reader_next(struct pg_reader *reader)
{
	if (reader->error) return PG_TOKEN_ERROR;
	if (skip(reader) != 0) return PG_TOKEN_ERROR;
	start(reader);
	int c = peek(reader, 0);
	if (c == -1) return finish(reader, PG_TOKEN_EOF);
	if (letter(c)) {
		reader->token.text = reader->input + reader->position;
		while (name_char(peek(reader, 0))) take(reader);
		reader->token.text_length = reader->position - reader->token.offset;
		return finish(reader, PG_TOKEN_IDENT);
	}
	if (c == '#') {
		if (peek(reader, 1) == '"') return text_token(reader);
		if (digit(peek(reader, 1)) || peek(reader, 1) == '-') return int_token(reader);
	}
	take(reader);
	struct punctuation { int first, second, kind; };
	static const struct punctuation pairs[] = {
		{':', '=', PG_TOKEN_ASSIGN}, {':', ':', PG_TOKEN_EXPECT},
		{'=', '>', PG_TOKEN_LAMBDA_ARROW}, {'-', '>', PG_TOKEN_ARROW}
	};
	for (size_t i = 0; i < sizeof(pairs) / sizeof(*pairs); ++i) {
		if (pairs[i].first != c) continue;
		if (pairs[i].second != peek(reader, 0)) continue;
		take(reader);
		return finish(reader, pairs[i].kind);
	}
	if (c != 0 && strchr("@*:;{}().\\#&!", c)) return finish(reader, c);
	return fail(reader, "unexpected character");
}
