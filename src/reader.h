#ifndef A_PROGRAM_POINTER_READER_H
#define A_PROGRAM_POINTER_READER_H

#include <stddef.h>
#include <stdint.h>

/* Single-character punctuation uses its ASCII value. No word is reserved by
 * the lexer; contextual grammar decisions belong to the parser. */
enum pg_token_kind {
	PG_TOKEN_EOF = 0,
	PG_TOKEN_IDENT = 256,
	PG_TOKEN_ASSIGN,
	PG_TOKEN_EXPECT,
	PG_TOKEN_LAMBDA_ARROW,
	PG_TOKEN_ARROW,
	PG_TOKEN_TEXT,
	PG_TOKEN_INT,
	PG_TOKEN_ERROR
};

struct pg_token {
	int kind;
	size_t offset, length, line, column;
	const char *text;
	size_t text_length;
	int64_t integer;
};

/* Tokens borrow input; scanning allocates nothing and never resolves names. */
struct pg_reader {
	const char *input;
	size_t length, position, line, column;
	const char *error;
	struct pg_token token;
};
void pg_reader_init(struct pg_reader *reader, const char *input, size_t length);
int pg_reader_next(struct pg_reader *reader);

#endif
