#include "a_program/frontend/reader.h"

#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/universe_collection.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSER_BLOCK_ITEM_CAPACITY 512

enum token_kind {
	TOKEN_EOF = 0,
	TOKEN_IDENT,
	TOKEN_ASSIGN,
	TOKEN_AT,
	TOKEN_STAR,
	TOKEN_COLON,
	TOKEN_DOUBLE_COLON,
	TOKEN_SEMI,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_DOT,
	TOKEN_BACKSLASH,
	TOKEN_FATARROW,
	TOKEN_ARROW,
	TOKEN_HASH,
	TOKEN_AMPERSAND,
	TOKEN_BANG,
	TOKEN_TEXT_LITERAL,
	TOKEN_INT_LITERAL
};

struct token {
	int kind;
	const char* start;
	size_t len;
	unsigned line;
	unsigned column;
	int symbol_id;
	int64_t int_value;
};

struct local_binder {
	int symbol_id;
	uint32_t ast_binder_id;
	int induction_allowed;
	struct local_binder* next;
};

struct parser {
	const char* filename;
	char* input;
	size_t input_len;
	size_t pos;
	unsigned line;
	unsigned column;
	struct token current;
	struct prototype_program* program;
	struct prototype_read_error* error;
	struct local_binder* binders;
	/* Case binders closed while a postfix elimination suffix is still being
	 * parsed. They distinguish a missing pair of grouping parentheses from a
	 * genuinely nonlocal induction-hypothesis name. */
	int closed_case_binder_symbols[256];
	uint32_t closed_case_binder_count;
	struct prototype_read_options options;
	unsigned type_definition_depth;
};

static void set_error_code(
	struct parser* parser,
	int diagnostic_code,
	const char* message
) {
	if (!parser->error || parser->error->message[0] != '\0') {
		return;
	}

	parser->error->filename = parser->filename;
	parser->error->line = parser->current.line;
	parser->error->column = parser->current.column;
	parser->error->diagnostic_code = diagnostic_code;
	snprintf(parser->error->message, sizeof(parser->error->message), "%s", message);
}

static void set_error(struct parser* parser, const char* message) {
	set_error_code(parser, PROTOTYPE_READ_DIAGNOSTIC_SYNTAX, message);
}

static char peek_char(const struct parser* parser) {
	return parser->input[parser->pos];
}

static char peek_next_char(const struct parser* parser) {
	char c = parser->input[parser->pos];
	if (c == '\0') {
		return '\0';
	}
	return parser->input[parser->pos + 1];
}

static char advance_char(struct parser* parser) {
	char c = parser->input[parser->pos++];
	if (c == '\n') {
		parser->line++;
		parser->column = 1;
	} else if (c != '\0') {
		parser->column++;
	}
	return c;
}

static int is_ident_start(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_continue(char c) {
	return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int input_starts_with(const struct parser* parser, size_t pos, const char* text, size_t len) {
	if (!parser || !text) {
		return 0;
	}
	if (pos > parser->input_len || len > parser->input_len - pos) {
		return 0;
	}
	for (size_t i = 0; i < len; ++i) {
		if (parser->input[pos + i] != text[i]) {
			return 0;
		}
	}
	return 1;
}

static int read_text_literal_token(struct parser* parser) {
	size_t delimiter_start;
	size_t delimiter_len = 0;
	const char* delimiter = NULL;
	size_t body_start;
	size_t body_end;
	int symbol_id;

	advance_char(parser);
	advance_char(parser);

	delimiter_start = parser->pos;
	if (is_ident_start(peek_char(parser))) {
		size_t cursor = parser->pos;
		while (is_ident_continue(parser->input[cursor])) {
			cursor++;
		}
		if (parser->input[cursor] == '\n' ||
			(parser->input[cursor] == '\r' && parser->input[cursor + 1] == '\n')) {
			delimiter = &parser->input[delimiter_start];
			delimiter_len = cursor - delimiter_start;
			while (parser->pos < cursor) {
				advance_char(parser);
			}
			if (peek_char(parser) == '\r') {
				advance_char(parser);
			}
			advance_char(parser);
		}
	}

	body_start = parser->pos;
	if (delimiter_len == 0) {
		if (peek_char(parser) == '\r' && parser->input[parser->pos + 1] == '\n') {
			advance_char(parser);
			advance_char(parser);
			body_start = parser->pos;
		} else if (peek_char(parser) == '\n') {
			advance_char(parser);
			body_start = parser->pos;
		}
		while (peek_char(parser) != '\0' && peek_char(parser) != '"') {
			advance_char(parser);
		}
		if (peek_char(parser) != '"') {
			set_error(parser, "unterminated text literal");
			return -1;
		}
		body_end = parser->pos;
		symbol_id = symbol_intern(
			parser->program->symbols,
			&parser->input[body_start],
			body_end - body_start
		);
		if (symbol_id < 0) {
			set_error(parser, "symbol table is full");
			return -1;
		}
		advance_char(parser);
		parser->current.kind = TOKEN_TEXT_LITERAL;
		parser->current.symbol_id = symbol_id;
		return 0;
	}

	for (;;) {
		if (peek_char(parser) == '\0') {
			set_error(parser, "unterminated delimited text literal");
			return -1;
		}
		if (input_starts_with(parser, parser->pos, delimiter, delimiter_len) &&
			parser->input[parser->pos + delimiter_len] == '"') {
			body_end = parser->pos;
			symbol_id = symbol_intern(
				parser->program->symbols,
				&parser->input[body_start],
				body_end - body_start
			);
			if (symbol_id < 0) {
				set_error(parser, "symbol table is full");
				return -1;
			}
			for (size_t i = 0; i < delimiter_len; ++i) {
				advance_char(parser);
			}
			advance_char(parser);
			parser->current.kind = TOKEN_TEXT_LITERAL;
			parser->current.symbol_id = symbol_id;
			return 0;
		}
		advance_char(parser);
	}
}

static int read_int_literal_token(struct parser* parser) {
	int negative = 0;
	uint64_t magnitude = 0;
	int saw_digit = 0;
	uint64_t limit;

	advance_char(parser);
	if (peek_char(parser) == '-') {
		negative = 1;
		advance_char(parser);
	}
	limit = negative ? ((uint64_t)INT64_MAX + 1ull) : (uint64_t)INT64_MAX;
	while (peek_char(parser) >= '0' && peek_char(parser) <= '9') {
		int digit = peek_char(parser) - '0';
		saw_digit = 1;
		if (magnitude > (limit - (uint64_t)digit) / 10ull) {
			set_error(parser, "integer literal is outside Int64 range");
			return -1;
		}
		magnitude = magnitude * 10ull + (uint64_t)digit;
		advance_char(parser);
	}
	if (!saw_digit) {
		set_error(parser, "expected digits after '#'");
		return -1;
	}
	parser->current.kind = TOKEN_INT_LITERAL;
	if (negative) {
		if (magnitude == (uint64_t)INT64_MAX + 1ull) {
			parser->current.int_value = INT64_MIN;
		} else {
			parser->current.int_value = -(int64_t)magnitude;
		}
	} else {
		parser->current.int_value = (int64_t)magnitude;
	}
	return 0;
}

static int skip_space_and_comments(struct parser* parser) {
	for (;;) {
		char c = peek_char(parser);
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			advance_char(parser);
			continue;
		}
		if (c == '/' && peek_next_char(parser) == '/') {
			while (peek_char(parser) != '\0' && peek_char(parser) != '\n') {
				advance_char(parser);
			}
			continue;
		}
		if (c == '/' && peek_next_char(parser) == '*') {
			advance_char(parser);
			advance_char(parser);
			while (peek_char(parser) != '\0') {
				if (peek_char(parser) == '*' && peek_next_char(parser) == '/') {
					advance_char(parser);
					advance_char(parser);
					break;
				}
				advance_char(parser);
			}
			if (peek_char(parser) == '\0' && !(parser->input[parser->pos - 2] == '*' && parser->input[parser->pos - 1] == '/')) {
				set_error(parser, "unterminated block comment");
				return -1;
			}
			continue;
		}
		return 0;
	}
}

static int read_token(struct parser* parser) {
	if (skip_space_and_comments(parser) != 0) {
		return -1;
	}

	memset(&parser->current, 0, sizeof(parser->current));
	parser->current.start = &parser->input[parser->pos];
	parser->current.line = parser->line;
	parser->current.column = parser->column;

	char c = peek_char(parser);
	if (c == '\0') {
		parser->current.kind = TOKEN_EOF;
		return 0;
	}

	if (c == '#' && peek_next_char(parser) == '"') {
		return read_text_literal_token(parser);
	}
	if (c == '#' &&
		((peek_next_char(parser) >= '0' && peek_next_char(parser) <= '9') ||
			peek_next_char(parser) == '-')) {
		return read_int_literal_token(parser);
	}

	if (is_ident_start(c)) {
		size_t start = parser->pos;
		while (is_ident_continue(peek_char(parser))) {
			advance_char(parser);
		}
		parser->current.kind = TOKEN_IDENT;
		parser->current.start = &parser->input[start];
		parser->current.len = parser->pos - start;
		parser->current.symbol_id = symbol_intern(parser->program->symbols, parser->current.start, parser->current.len);
		if (parser->current.symbol_id < 0) {
			set_error(parser, "symbol table is full");
			return -1;
		}
		return 0;
	}

	advance_char(parser);
	switch (c) {
		case ':':
			if (peek_char(parser) == ':') {
				advance_char(parser);
				parser->current.kind = TOKEN_DOUBLE_COLON;
			} else if (peek_char(parser) == '=') {
				advance_char(parser);
				parser->current.kind = TOKEN_ASSIGN;
			} else {
				parser->current.kind = TOKEN_COLON;
			}
			return 0;
		case '=':
			if (peek_char(parser) == '>') {
				advance_char(parser);
				parser->current.kind = TOKEN_FATARROW;
				return 0;
			}
			break;
		case '-':
			if (peek_char(parser) == '>') {
				advance_char(parser);
				parser->current.kind = TOKEN_ARROW;
				return 0;
			}
			break;
		case '@':
			parser->current.kind = TOKEN_AT;
			return 0;
		case '*':
			parser->current.kind = TOKEN_STAR;
			return 0;
		case ';':
			parser->current.kind = TOKEN_SEMI;
			return 0;
		case '{':
			parser->current.kind = TOKEN_LBRACE;
			return 0;
		case '}':
			parser->current.kind = TOKEN_RBRACE;
			return 0;
		case '(':
			parser->current.kind = TOKEN_LPAREN;
			return 0;
		case ')':
			parser->current.kind = TOKEN_RPAREN;
			return 0;
		case '.':
			parser->current.kind = TOKEN_DOT;
			return 0;
		case '\\':
			parser->current.kind = TOKEN_BACKSLASH;
			return 0;
		case '#':
			parser->current.kind = TOKEN_HASH;
			return 0;
		case '&':
			parser->current.kind = TOKEN_AMPERSAND;
			return 0;
		case '!':
			parser->current.kind = TOKEN_BANG;
			return 0;
		default:
			break;
	}

	set_error(parser, "unexpected character");
	return -1;
}

static int accept(struct parser* parser, int kind) {
	if (parser->current.kind != kind) {
		return 0;
	}
	return read_token(parser) == 0;
}

static int expect(struct parser* parser, int kind, const char* message) {
	if (parser->current.kind != kind) {
		set_error(parser, message);
		return -1;
	}
	return read_token(parser);
}

static struct prototype_source_span current_span(const struct parser* parser) {
	struct prototype_source_span span;
	memset(&span, 0, sizeof(span));
	if (parser) {
		span.line = parser->current.line;
		span.column = parser->current.column;
	}
	return span;
}

static int parse_type_expr(struct parser* parser, uint32_t* p_ret);
static int parse_term(struct parser* parser, uint32_t* p_ret);
static int parse_case_body(struct parser* parser, uint32_t* p_ret);

static const struct local_binder* lookup_binder(const struct parser* parser, int symbol_id) {
	for (const struct local_binder* binder = parser->binders; binder; binder = binder->next) {
		if (binder->symbol_id == symbol_id) {
			return binder;
		}
	}
	return NULL;
}

static int lookup_closed_case_binder(
	const struct parser* parser,
	int symbol_id
) {
	if (!parser) {
		return 0;
	}
	for (uint32_t i = parser->closed_case_binder_count; i > 0; --i) {
		if (parser->closed_case_binder_symbols[i - 1] == symbol_id) {
			return 1;
		}
	}
	return 0;
}

static int parse_type_atom(struct parser* parser, uint32_t* p_ret) {
	struct prototype_source_span span = current_span(parser);
	if (accept(parser, TOKEN_AMPERSAND)) {
		uint32_t result;
		if (parse_type_atom(parser, &result) != 0 ||
			prototype_ast_type_expr_computation_reference(
				parser->program->asts, result, span, p_ret
			) != 0) {
			set_error(parser, "expected result type after '&'");
			return -1;
		}
		return 0;
	}
	if (parser->current.kind == TOKEN_HASH) {
		int symbol_id;
		const char* name;
		if (read_token(parser) != 0) {
			return -1;
		}
		if (expect(parser, TOKEN_DOT, "expected '.' after system namespace") != 0) {
			return -1;
		}
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected system type name after '#.'");
			return -1;
		}
		name = symbol_to_string(parser->program->symbols, parser->current.symbol_id);
		if (name && strcmp(name, "Terminates") == 0) {
			uint32_t computation;
			if (read_token(parser) != 0 || expect(
					parser, TOKEN_LPAREN, "expected '(' before suspended computation"
				) != 0 || parse_term(parser, &computation) != 0 || expect(
					parser, TOKEN_RPAREN, "expected ')' after suspended computation"
				) != 0) {
				return -1;
			}
			return prototype_ast_type_expr_terminates(
				parser->program->asts, computation, span, p_ret
			);
		}
		int host_type;
		int host_status = prototype_term_host_type_from_source_name(
			parser->program->intrinsic_environment, name, &host_type
		);
		if (host_status < 0) {
			return -1;
		}
		if (host_status == 0) {
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_type_expr_host_type(
				parser->program->asts,
				host_type,
				span,
				p_ret
			);
		}
		if (!name || strcmp(name, "Nat") != 0) {
			set_error(parser, "unknown system type name");
			return -1;
		}
		symbol_id = symbol_intern(parser->program->symbols, "#.Nat", 5);
		if (symbol_id < 0) {
			set_error(parser, "symbol table is full");
			return -1;
		}
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_type_expr_name(parser->program->asts, symbol_id, span, p_ret);
	}
	if (parser->current.kind == TOKEN_STAR) {
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_type_expr_self(parser->program->asts, span, p_ret);
	}
	if (parser->current.kind == TOKEN_AT) {
		if (read_token(parser) != 0) {
			return -1;
		}
		if (parser->current.kind == TOKEN_IDENT) {
			int owner_symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_type_expr_function_graph_reference(
				parser->program->asts, owner_symbol_id, span, p_ret
			);
		}
		return prototype_ast_type_expr_fresh_universe(parser->program->asts, span, p_ret);
	}
	if (parser->current.kind == TOKEN_IDENT) {
		int symbol_id = parser->current.symbol_id;
		const struct local_binder* binder;
		if (read_token(parser) != 0) {
			return -1;
		}
		binder = lookup_binder(parser, symbol_id);
		if (binder) {
			return prototype_ast_type_expr_var(parser->program->asts, binder->ast_binder_id, symbol_id, span, p_ret);
		}
		if (accept(parser, TOKEN_DOT)) {
			int member_symbol_id;
			if (parser->current.kind != TOKEN_IDENT) {
				set_error(parser, "expected member name after type namespace dot");
				return -1;
			}
			member_symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_type_expr_name_in_namespace(
				parser->program->asts,
				symbol_id,
				member_symbol_id,
				span,
				p_ret
			);
		}
		return prototype_ast_type_expr_name(parser->program->asts, symbol_id, span, p_ret);
	}
	if (accept(parser, TOKEN_LPAREN)) {
		if (parse_type_expr(parser, p_ret) != 0) {
			return -1;
		}
		if (parser->current.kind != TOKEN_RPAREN) {
			set_error_code(
				parser,
				parser->type_definition_depth > 0 ?
					PROTOTYPE_READ_DIAGNOSTIC_UNSUPPORTED_INDEXED_FAMILY :
					PROTOTYPE_READ_DIAGNOSTIC_SYNTAX,
				"expected ')' after type expression"
			);
			return -1;
		}
		return read_token(parser);
	}

	set_error(parser, "expected type expression");
	return -1;
}

static int token_starts_type_atom(int kind) {
	return kind == TOKEN_AMPERSAND || kind == TOKEN_HASH || kind == TOKEN_STAR ||
		kind == TOKEN_AT || kind == TOKEN_IDENT || kind == TOKEN_LPAREN;
}

static int parse_type_app(struct parser* parser, uint32_t* p_ret) {
	uint32_t lhs;
	struct prototype_source_span span = current_span(parser);
	if (parse_type_atom(parser, &lhs) != 0) {
		return -1;
	}

	while (token_starts_type_atom(parser->current.kind)) {
		uint32_t argument;
		uint32_t app;
		if (parse_type_atom(parser, &argument) != 0) {
			return -1;
		}
		if (prototype_ast_type_expr_app(parser->program->asts, lhs, argument, span, &app) != 0) {
			set_error(parser, "type expression table is full");
			return -1;
		}
		lhs = app;
	}

	*p_ret = lhs;
	return 0;
}

struct parser_snapshot {
	size_t pos;
	unsigned line;
	unsigned column;
	struct token current;
	struct local_binder* binders;
};

static void snapshot_parser(const struct parser* parser, struct parser_snapshot* snapshot) {
	snapshot->pos = parser->pos;
	snapshot->line = parser->line;
	snapshot->column = parser->column;
	snapshot->current = parser->current;
	snapshot->binders = parser->binders;
}

static void restore_parser(struct parser* parser, const struct parser_snapshot* snapshot) {
	parser->pos = snapshot->pos;
	parser->line = snapshot->line;
	parser->column = snapshot->column;
	parser->current = snapshot->current;
	parser->binders = snapshot->binders;
}

static int parse_type_expr(struct parser* parser, uint32_t* p_ret) {
	if (parser->current.kind == TOKEN_LPAREN) {
		struct parser_snapshot snapshot;
		snapshot_parser(parser, &snapshot);
		struct prototype_source_span span = current_span(parser);
		if (read_token(parser) != 0) {
			return -1;
		}
		if (parser->current.kind == TOKEN_IDENT) {
			int symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			if (parser->current.kind == TOKEN_COLON) {
				uint32_t ast_binder_id = prototype_ast_new_binder(parser->program->asts);
				uint32_t domain;
				uint32_t codomain;
				struct local_binder binder;
				if (ast_binder_id == PROTOTYPE_INVALID_ID || read_token(parser) != 0 ||
					parse_type_expr(parser, &domain) != 0 ||
					expect(parser, TOKEN_RPAREN, "expected ')' after Pi binder") != 0 ||
					expect(parser, TOKEN_ARROW, "expected '->' after Pi binder") != 0) {
					return -1;
				}
				binder.symbol_id = symbol_id;
				binder.ast_binder_id = ast_binder_id;
				binder.induction_allowed = 0;
				binder.next = parser->binders;
				parser->binders = &binder;
				if (parse_type_expr(parser, &codomain) != 0) {
					parser->binders = binder.next;
					return -1;
				}
				parser->binders = binder.next;
				if (prototype_ast_type_expr_pi(
						parser->program->asts,
						ast_binder_id,
						symbol_id,
						domain,
						codomain,
						span,
						p_ret
					) != 0) {
					set_error(parser, "type expression table is full");
					return -1;
				}
				return 0;
			}
		}
		restore_parser(parser, &snapshot);
	}

	uint32_t lhs;
	if (parse_type_app(parser, &lhs) != 0) {
		return -1;
	}

	if (parser->current.kind == TOKEN_ARROW) {
		struct prototype_source_span span = current_span(parser);
		if (accept(parser, TOKEN_ARROW) == 0) {
			return -1;
		}
		uint32_t rhs;
		if (parse_type_expr(parser, &rhs) != 0) {
			return -1;
		}
		return prototype_ast_type_expr_arrow(parser->program->asts, lhs, rhs, span, p_ret);
	}

	*p_ret = lhs;
	return 0;
}

static int try_parse_constructor_field_binder(
	struct parser* parser,
	uint32_t* p_ast_binder_id,
	int* p_symbol_id,
	uint32_t* p_type_expr
) {
	if (!parser || !p_ast_binder_id || !p_symbol_id || !p_type_expr ||
		parser->current.kind != TOKEN_LPAREN) {
		return 1;
	}

	struct parser_snapshot snapshot;
	snapshot_parser(parser, &snapshot);
	if (read_token(parser) != 0) {
		return -1;
	}
	if (parser->current.kind != TOKEN_IDENT) {
		restore_parser(parser, &snapshot);
		return 1;
	}
	int symbol_id = parser->current.symbol_id;
	uint32_t ast_binder_id = prototype_ast_new_binder(parser->program->asts);
	if (ast_binder_id == PROTOTYPE_INVALID_ID) {
		set_error(parser, "binder table is full");
		return -1;
	}
	if (read_token(parser) != 0) {
		return -1;
	}
	if (parser->current.kind != TOKEN_COLON) {
		restore_parser(parser, &snapshot);
		return 1;
	}
	if (read_token(parser) != 0 ||
		parse_type_expr(parser, p_type_expr) != 0 ||
		expect(parser, TOKEN_RPAREN, "expected ')' after constructor field binder") != 0) {
		return -1;
	}
	*p_ast_binder_id = ast_binder_id;
	*p_symbol_id = symbol_id;
	return 0;
}

static int parse_constructor_type(
	struct parser* parser,
	uint32_t* field_types,
	uint32_t* field_binder_ids,
	int* field_name_symbol_ids,
	uint32_t field_capacity,
	uint32_t* p_field_count,
	uint32_t* p_result_type
) {
	uint32_t field_count = 0;
	struct local_binder field_binders[64];
	struct local_binder* initial_binders = parser->binders;

	for (;;) {
		if (field_count >= field_capacity) {
			parser->binders = initial_binders;
			set_error(parser, "too many constructor fields");
			return -1;
		}

		uint32_t field_type;
		uint32_t field_binder_id = PROTOTYPE_INVALID_ID;
		int field_symbol_id = -1;
		int binder_status = try_parse_constructor_field_binder(
			parser,
			&field_binder_id,
			&field_symbol_id,
			&field_type
		);
		if (binder_status < 0) {
			parser->binders = initial_binders;
			return -1;
		}
		if (binder_status > 0 && parse_type_app(parser, &field_type) != 0) {
			parser->binders = initial_binders;
			return -1;
		}

		if (parser->current.kind != TOKEN_ARROW) {
			if (binder_status == 0) {
				parser->binders = initial_binders;
				set_error(parser, "expected '->' after constructor field binder");
				return -1;
			}
			*p_field_count = field_count;
			*p_result_type = field_type;
			parser->binders = initial_binders;
			return 0;
		}
		if (read_token(parser) != 0) {
			parser->binders = initial_binders;
			return -1;
		}

		field_types[field_count] = field_type;
		field_binder_ids[field_count] = field_binder_id;
		field_name_symbol_ids[field_count] = field_symbol_id;
		if (field_binder_id != PROTOTYPE_INVALID_ID) {
			field_binders[field_count].symbol_id = field_symbol_id;
			field_binders[field_count].ast_binder_id = field_binder_id;
			field_binders[field_count].induction_allowed = 0;
			field_binders[field_count].next = parser->binders;
			parser->binders = &field_binders[field_count];
		}
		field_count++;
	}
}

static int parse_constructor_block(struct parser* parser, uint32_t ast_type_def_id) {
	if (expect(parser, TOKEN_LBRACE, "expected '{' before constructors") != 0) {
		return -1;
	}

	while (parser->current.kind != TOKEN_RBRACE && parser->current.kind != TOKEN_EOF) {
		int constructor_symbol;
		uint32_t constructor_fields[64];
		uint32_t constructor_field_binder_ids[64];
		int constructor_field_symbols[64];
		uint32_t constructor_field_count;
		uint32_t constructor_result_type;

		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected constructor name");
			return -1;
		}
		struct prototype_source_span constructor_name_span = current_span(parser);
		constructor_symbol = parser->current.symbol_id;
		if (read_token(parser) != 0) {
			return -1;
		}
		if (expect(parser, TOKEN_COLON, "expected ':' after constructor name") != 0) {
			return -1;
		}
		if (parse_constructor_type(
			parser,
			constructor_fields,
			constructor_field_binder_ids,
			constructor_field_symbols,
			64,
			&constructor_field_count,
			&constructor_result_type
		) != 0) {
			return -1;
		}
		if (expect(parser, TOKEN_SEMI, "expected ';' after constructor") != 0) {
			return -1;
		}
		if (prototype_ast_type_add_constructor(
			parser->program->asts,
			ast_type_def_id,
			constructor_symbol,
			constructor_name_span,
			constructor_fields,
			constructor_field_binder_ids,
			constructor_field_symbols,
			constructor_field_count,
			constructor_result_type
		) != 0) {
			set_error(parser, "constructor table is full");
			return -1;
		}
	}

	if (expect(parser, TOKEN_RBRACE, "expected '}' after type definition") != 0) {
		return -1;
	}
	return expect(parser, TOKEN_SEMI, "expected ';' after type definition");
}

static int parse_family_body(struct parser* parser, uint32_t ast_type_def_id) {
	struct local_binder index_binders[32];
	struct local_binder* parameter_binders;
	uint32_t index_count = 0;

	if (!parser || ast_type_def_id >= parser->program->asts->type_def_count) {
		return -1;
	}
	parameter_binders = parser->binders;
	if (expect(parser, TOKEN_AT, "expected '@{' or '@\\index' after family parameters") != 0) {
		return -1;
	}
	if (parser->current.kind == TOKEN_LBRACE) {
		return parse_constructor_block(parser, ast_type_def_id);
	}

	for (;;) {
		int symbol_id;
		uint32_t ast_binder_id;
		uint32_t classifier;
		struct prototype_source_span span;

		if (expect(parser, TOKEN_BACKSLASH, "expected '\\' after '@' in index binder") != 0) {
			parser->binders = parameter_binders;
			return -1;
		}
		if (index_count >= 32) {
			parser->binders = parameter_binders;
			set_error(parser, "too many family indices");
			return -1;
		}
		if (parser->current.kind != TOKEN_IDENT) {
			parser->binders = parameter_binders;
			set_error(parser, "expected index binder name");
			return -1;
		}
		span = current_span(parser);
		symbol_id = parser->current.symbol_id;
		ast_binder_id = prototype_ast_new_binder(parser->program->asts);
		if (ast_binder_id == PROTOTYPE_INVALID_ID || read_token(parser) != 0 ||
			expect(parser, TOKEN_COLON, "expected ':' after index binder") != 0 ||
			parse_type_expr(parser, &classifier) != 0 ||
			expect(parser, TOKEN_FATARROW, "expected '=>' after index binder") != 0 ||
			prototype_ast_type_add_family_binder(
				parser->program->asts,
				ast_type_def_id,
				ast_binder_id,
				symbol_id,
				classifier,
				PROTOTYPE_AST_FAMILY_BINDER_INDEX,
				span
			) != 0) {
			parser->binders = parameter_binders;
			if (ast_binder_id == PROTOTYPE_INVALID_ID) {
				set_error(parser, "binder table is full");
			}
			return -1;
		}
		index_binders[index_count].symbol_id = symbol_id;
		index_binders[index_count].ast_binder_id = ast_binder_id;
		index_binders[index_count].induction_allowed = 0;
		index_binders[index_count].next = parser->binders;
		parser->binders = &index_binders[index_count++];

		if (parser->current.kind == TOKEN_LBRACE) {
			break;
		}
		if (expect(parser, TOKEN_AT, "expected another '@\\index' or '{'") != 0) {
			parser->binders = parameter_binders;
			return -1;
		}
	}

	/* Family indices describe the arity. Constructor-local index variables must
	 * be bound explicitly in each constructor telescope. */
	parser->binders = parameter_binders;
	return parse_constructor_block(parser, ast_type_def_id);
}

static int parse_anonymous_type_literal(
	struct parser* parser,
	int namespace_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	uint32_t ast_type_def_id;

	if (prototype_ast_type_add(
			parser->program->asts,
			namespace_symbol_id,
			span,
			span,
			&ast_type_def_id
		) != 0) {
		set_error(parser, "type table is full");
		return -1;
	}
	parser->type_definition_depth++;
	int body_status = parse_family_body(parser, ast_type_def_id);
	parser->type_definition_depth--;
	if (body_status != 0) {
		return -1;
	}
	if (prototype_ast_type_literal(parser->program->asts, ast_type_def_id, span, p_ret) != 0) {
		set_error(parser, "AST table is full");
		return -1;
	}
	return 0;
}

static int parse_anonymous_type_def(
	struct parser* parser,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_assignment_id
) {
	uint32_t term;
	if (parse_anonymous_type_literal(parser, name_symbol_id, body_span, &term) != 0) {
		return -1;
	}
	if (prototype_ast_add_term_assignment(
		parser->program->asts,
		name_symbol_id,
		term,
		source_entry_id,
		name_span,
		body_span,
		p_assignment_id
	) != 0) {
		set_error(parser, "definition table is full");
		return -1;
	}
	return 0;
}

static int parse_parameterized_type_or_lambda_def(
	struct parser* parser,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_assignment_id
) {
	int binder_symbols[32];
	uint32_t ast_binder_ids[32];
	uint32_t binder_types[32];
	struct prototype_source_span binder_spans[32];
	uint32_t binder_count = 0;
	struct local_binder binders[32];
	struct local_binder* outer_binders = parser->binders;

	while (parser->current.kind == TOKEN_BACKSLASH) {
		if (binder_count >= 32) {
			set_error(parser, "too many lambda/type parameters");
			parser->binders = outer_binders;
			return -1;
		}
		if (expect(parser, TOKEN_BACKSLASH, "expected lambda") != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected binder name");
			parser->binders = outer_binders;
			return -1;
		}
			binder_symbols[binder_count] = parser->current.symbol_id;
			binder_spans[binder_count] = current_span(parser);
		ast_binder_ids[binder_count] = prototype_ast_new_binder(parser->program->asts);
		if (ast_binder_ids[binder_count] == PROTOTYPE_INVALID_ID) {
			set_error(parser, "binder table is full");
			parser->binders = outer_binders;
			return -1;
		}
		if (read_token(parser) != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		if (expect(parser, TOKEN_COLON, "expected ':' after binder") != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		if (parse_type_expr(parser, &binder_types[binder_count]) != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		if (expect(parser, TOKEN_FATARROW, "expected '=>' after binder") != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		binders[binder_count].symbol_id = binder_symbols[binder_count];
		binders[binder_count].ast_binder_id = ast_binder_ids[binder_count];
		binders[binder_count].induction_allowed = 0;
		binders[binder_count].next = parser->binders;
		parser->binders = &binders[binder_count];
		binder_count++;
	}

	if (parser->current.kind == TOKEN_AT) {
		uint32_t ast_type_def_id;
		if (prototype_ast_type_add(
				parser->program->asts,
				name_symbol_id,
				name_span,
				body_span,
				&ast_type_def_id
			) != 0) {
			set_error(parser, "type table is full");
			parser->binders = outer_binders;
			return -1;
		}
		for (uint32_t i = 0; i < binder_count; ++i) {
				if (prototype_ast_type_add_family_binder(
					parser->program->asts,
					ast_type_def_id,
					ast_binder_ids[i],
					binder_symbols[i],
					binder_types[i],
					PROTOTYPE_AST_FAMILY_BINDER_PARAMETER,
					binder_spans[i]
					) != 0) {
				set_error(parser, "type parameter table is full");
				parser->binders = outer_binders;
				return -1;
			}
		}
		parser->type_definition_depth++;
			int body_status = parse_family_body(parser, ast_type_def_id);
		parser->type_definition_depth--;
		if (body_status != 0) {
			parser->binders = outer_binders;
			return -1;
		}
		parser->binders = outer_binders;
		uint32_t term;
		if (prototype_ast_type_formation(
			parser->program->asts,
			ast_type_def_id,
			body_span,
			&term
		) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		if (prototype_ast_add_term_assignment(
			parser->program->asts,
			name_symbol_id,
			term,
			source_entry_id,
			name_span,
			body_span,
			p_assignment_id
		) != 0) {
			set_error(parser, "definition table is full");
			return -1;
		}
		return 0;
	}

	uint32_t body;
	if (parse_term(parser, &body) != 0) {
		parser->binders = outer_binders;
		return -1;
	}

	parser->binders = outer_binders;

	uint32_t term = body;
	for (uint32_t i = binder_count; i > 0; --i) {
		uint32_t lambda;
		if (prototype_ast_lambda(
			parser->program->asts,
			ast_binder_ids[i - 1],
			binder_symbols[i - 1],
			binder_types[i - 1],
			term,
			body_span,
			&lambda
		) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		term = lambda;
	}

	if (expect(parser, TOKEN_SEMI, "expected ';' after term definition") != 0) {
		return -1;
	}
	if (prototype_ast_add_term_assignment(
		parser->program->asts,
		name_symbol_id,
		term,
		source_entry_id,
		name_span,
		body_span,
		p_assignment_id
	) != 0) {
		set_error(parser, "definition table is full");
		return -1;
	}
	return 0;
}

static int block_starts_binding(struct parser* parser) {
	if (!parser || parser->current.kind != TOKEN_IDENT) {
		return 0;
	}
	struct parser_snapshot snapshot;
	snapshot_parser(parser, &snapshot);
	if (read_token(parser) != 0) {
		return -1;
	}
	int result = parser->current.kind == TOKEN_ASSIGN ||
		parser->current.kind == TOKEN_COLON;
	restore_parser(parser, &snapshot);
	return result;
}

static int block_has_local_symbol(
	const struct parser* parser,
	const struct local_binder* outer_binders,
	int symbol_id
) {
	if (!parser) {
		return 0;
	}
	for (const struct local_binder* binder = parser->binders;
		binder != outer_binders;
		binder = binder->next) {
		if (!binder || binder->symbol_id == symbol_id) {
			return binder != NULL;
		}
	}
	return 0;
}

static int parse_computation_block(struct parser* parser, uint32_t* p_ret) {
	struct prototype_source_span span = current_span(parser);
	struct local_binder* outer_binders = parser->binders;
	uint32_t item_ids[PARSER_BLOCK_ITEM_CAPACITY];
	struct local_binder local_binders[PARSER_BLOCK_ITEM_CAPACITY];
	uint32_t item_count = 0;
	uint32_t local_binder_count = 0;
	uint32_t result_item_index;
	int result_mode = PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM;
	if (!parser || !p_ret || expect(parser, TOKEN_LBRACE, "expected '{'") != 0) {
		return -1;
	}
	while (parser->current.kind != TOKEN_RBRACE) {
		if (parser->current.kind == TOKEN_EOF) {
			set_error(parser, "expected '}' after computation block");
			goto fail;
		}
		if (item_count >= PARSER_BLOCK_ITEM_CAPACITY) {
			set_error(parser, "computation block has too many items");
			goto fail;
		}
		if (parser->current.kind == TOKEN_BANG) {
			struct prototype_source_span item_span = current_span(parser);
			uint32_t value;
			uint32_t item_id;
			if (read_token(parser) != 0 || parse_term(parser, &value) != 0 ||
				expect(
					parser,
					TOKEN_SEMI,
					"expected ';' after Lambda exit"
				) != 0 || prototype_ast_block_lambda_exit(
					parser->program->asts, value, item_span, &item_id
				) != 0) {
				if (!parser->error || parser->error->message[0] == '\0') {
					set_error(parser, "AST table is full");
				}
				goto fail;
			}
			item_ids[item_count++] = item_id;
			continue;
		}
		int starts_binding = block_starts_binding(parser);
		if (starts_binding < 0) {
			goto fail;
		}
		struct prototype_source_span item_span = current_span(parser);
		uint32_t item_id;
		if (starts_binding) {
			int binder_symbol_id = parser->current.symbol_id;
			uint32_t ast_binder_id;
			uint32_t binder_type = PROTOTYPE_INVALID_ID;
			uint32_t value;
			if (block_has_local_symbol(parser, outer_binders, binder_symbol_id)) {
				set_error(parser, "duplicate computation block binding");
				goto fail;
			}
			ast_binder_id = prototype_ast_new_binder(parser->program->asts);
			if (ast_binder_id == PROTOTYPE_INVALID_ID || read_token(parser) != 0) {
				goto fail;
			}
			if (accept(parser, TOKEN_COLON) &&
				parse_type_expr(parser, &binder_type) != 0) {
				goto fail;
			}
			if (expect(parser, TOKEN_ASSIGN, "expected ':=' in computation block") != 0 ||
				parse_term(parser, &value) != 0 || expect(
					parser,
					TOKEN_SEMI,
					"expected ';' after computation block binding"
				) != 0 || prototype_ast_block_binding(
					parser->program->asts,
					ast_binder_id,
					binder_symbol_id,
					binder_type,
					value,
					item_span,
					&item_id
				) != 0) {
				if (!parser->error || parser->error->message[0] == '\0') {
					set_error(parser, "AST table is full");
				}
				goto fail;
			}
			struct local_binder* binder = &local_binders[local_binder_count++];
			binder->symbol_id = binder_symbol_id;
			binder->ast_binder_id = ast_binder_id;
			binder->induction_allowed = 0;
			binder->next = parser->binders;
			parser->binders = binder;
		} else {
			uint32_t term;
			if (parse_term(parser, &term) != 0 || expect(
					parser,
					TOKEN_SEMI,
					"expected ';' after computation block expression"
				) != 0 || prototype_ast_block_expression(
					parser->program->asts, term, item_span, &item_id
				) != 0) {
				if (!parser->error || parser->error->message[0] == '\0') {
					set_error(parser, "AST table is full");
				}
				goto fail;
			}
		}
		item_ids[item_count++] = item_id;
	}
	if (item_count == 0) {
		set_error(parser, "computation block requires at least one item");
		goto fail;
	}
	if (expect(parser, TOKEN_RBRACE, "expected '}' after computation block") != 0) {
		goto fail;
	}
	result_item_index = item_count - 1;
	if (accept(parser, TOKEN_DOT)) {
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected result binding after computation block dot");
			goto fail;
		}
		int result_symbol_id = parser->current.symbol_id;
		int found = 0;
		for (uint32_t i = 0; i < item_count; ++i) {
			const struct prototype_ast_node* item =
				&parser->program->asts->nodes[item_ids[i]];
			if (item->tag == PROTOTYPE_AST_BLOCK_BINDING &&
				item->as.block_binding.binder_symbol_id == result_symbol_id) {
				result_item_index = i;
				found = 1;
				break;
			}
		}
		if (!found) {
			set_error(parser, "computation block result must name a direct binding");
			goto fail;
		}
		result_mode = PROTOTYPE_AST_BLOCK_RESULT_SELECTED_BINDING;
		if (read_token(parser) != 0) {
			goto fail;
		}
	}
	if (prototype_ast_computation_block(
			parser->program->asts,
			item_ids,
			item_count,
			result_item_index,
			result_mode,
			span,
			p_ret
		) != 0) {
		set_error(parser, "AST table is full");
		goto fail;
	}
	parser->binders = outer_binders;
	return 0;

fail:
	parser->binders = outer_binders;
	return -1;
}

static int parse_term_atom(struct parser* parser, uint32_t* p_ret) {
	if (accept(parser, TOKEN_AMPERSAND)) {
		struct prototype_source_span span = current_span(parser);
		uint32_t computation;
		if (parse_term_atom(parser, &computation) != 0 ||
			prototype_ast_quote(
				parser->program->asts, computation, span, p_ret
			) != 0) {
			set_error(parser, "expected computation after '&'");
			return -1;
		}
		return 0;
	}
	if (parser->current.kind == TOKEN_LBRACE) {
		return parse_computation_block(parser, p_ret);
	}
	if (parser->current.kind == TOKEN_HASH) {
		int namespace_symbol_id;
		int symbol_id;
		int type_symbol_id = -1;
		int system_name_kind = PROTOTYPE_AST_SYSTEM_NAME_UNKNOWN;
		int host_type_id = PROTOTYPE_HOST_TYPE_INVALID;
		int pure_primitive_id = PROTOTYPE_PURE_PRIMITIVE_UNKNOWN;
		int effect_operation_id = PROTOTYPE_EFFECT_OPERATION_UNKNOWN;
		const char* name;
		struct prototype_source_span span = current_span(parser);
		if (read_token(parser) != 0) {
			return -1;
		}
		namespace_symbol_id = symbol_intern(parser->program->symbols, "#", 1);
		if (namespace_symbol_id < 0) {
			set_error(parser, "symbol table is full");
			return -1;
		}
		if (expect(parser, TOKEN_DOT, "expected '.' after system namespace") != 0) {
			return -1;
		}
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected system name after '#.'");
			return -1;
		}
			symbol_id = parser->current.symbol_id;
			name = symbol_to_string(parser->program->symbols, symbol_id);
			if (name && strcmp(name, "terminates") == 0) {
				uint32_t computation;
				if (read_token(parser) != 0 || expect(
						parser, TOKEN_LPAREN,
						"expected '(' before suspended computation"
					) != 0 || parse_term(parser, &computation) != 0 || expect(
						parser, TOKEN_RPAREN,
						"expected ')' after suspended computation"
					) != 0) {
					return -1;
				}
				return prototype_ast_terminates_witness(
					parser->program->asts, computation, span, p_ret
				);
			}
			if (name && strcmp(name, "Nat") == 0) {
				int nat_symbol_id;
				if (read_token(parser) != 0) {
					return -1;
				}
				if (expect(parser, TOKEN_DOT, "expected constructor name after '#.Nat'") != 0) {
				return -1;
			}
			if (parser->current.kind != TOKEN_IDENT) {
				set_error(parser, "expected constructor name after '#.Nat.'");
				return -1;
			}
			nat_symbol_id = symbol_intern(parser->program->symbols, "#.Nat", 5);
			if (nat_symbol_id < 0) {
				set_error(parser, "symbol table is full");
				return -1;
			}
			symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_name_in_namespace(
				parser->program->asts,
				nat_symbol_id,
				symbol_id,
					span,
					p_ret
				);
			}
			struct prototype_intrinsic_namespace_binding binding;
			int binding_status = prototype_intrinsic_namespace_lookup(
				parser->program->intrinsic_environment, name, &binding
			);
			if (binding_status < 0) {
				return -1;
			}
			if (binding_status > 0) {
				set_error(parser, "unknown system name");
				return -1;
			}
			if (binding.kind == PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE) {
				system_name_kind = PROTOTYPE_AST_SYSTEM_NAME_HOST_TYPE;
				host_type_id = binding.target_id;
			} else if (binding.kind == PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE) {
				system_name_kind = PROTOTYPE_AST_SYSTEM_NAME_PURE_PRIMITIVE;
				pure_primitive_id = binding.target_id;
				if (pure_primitive_id == PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT ||
					pure_primitive_id == PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT) {
					type_symbol_id = symbol_intern(parser->program->symbols, "#.Nat", 5);
				}
			} else if (
				binding.kind == PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION
			) {
				system_name_kind = PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION;
				effect_operation_id = binding.target_id;
			} else {
				set_error(parser, "unsupported system namespace binding");
				return -1;
			}
			if (system_name_kind == PROTOTYPE_AST_SYSTEM_NAME_UNKNOWN) {
				set_error(parser, "unknown system name");
				return -1;
			}
				if ((pure_primitive_id == PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT ||
						pure_primitive_id == PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT) &&
					type_symbol_id < 0) {
					set_error(parser, "symbol table is full");
					return -1;
				}
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_system_name(
			parser->program->asts,
			namespace_symbol_id,
			symbol_id,
			type_symbol_id,
			system_name_kind,
			host_type_id,
			pure_primitive_id,
			effect_operation_id,
			span,
			p_ret
		);
	}
	if (parser->current.kind == TOKEN_TEXT_LITERAL) {
		int text_symbol_id = parser->current.symbol_id;
		struct prototype_source_span span = current_span(parser);
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_text_literal(parser->program->asts, text_symbol_id, span, p_ret);
	}
	if (parser->current.kind == TOKEN_INT_LITERAL) {
		int64_t value = parser->current.int_value;
		struct prototype_source_span span = current_span(parser);
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_int_literal(parser->program->asts, value, span, p_ret);
	}
	if (accept(parser, TOKEN_STAR)) {
		struct prototype_source_span span = current_span(parser);
		int symbol_id;
		const struct local_binder* binder;
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected binder name after '*'");
			return -1;
		}
		symbol_id = parser->current.symbol_id;
		binder = lookup_binder(parser, symbol_id);
		if (!binder) {
			if (lookup_closed_case_binder(parser, symbol_id)) {
				set_error_code(
					parser,
					PROTOTYPE_READ_DIAGNOSTIC_NESTED_MATCH_GROUPING,
					"nested elimination body requires parentheses before sibling '@' clauses"
				);
				return -1;
			}
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_function_graph_witness_reference(
				parser->program->asts, symbol_id, span, p_ret
			);
		}
		if (!binder->induction_allowed) {
			set_error(parser, "induction hypothesis must refer to a match-case binder");
			return -1;
		}
		if (read_token(parser) != 0) {
			return -1;
		}
		return prototype_ast_induction_hypothesis(
			parser->program->asts,
			binder->ast_binder_id,
			symbol_id,
			span,
			p_ret
		);
	}
	if (parser->current.kind == TOKEN_IDENT) {
		int symbol_id = parser->current.symbol_id;
		struct prototype_source_span span = current_span(parser);
		const struct local_binder* binder;
		if (read_token(parser) != 0) {
			return -1;
		}
		if (accept(parser, TOKEN_DOT)) {
			int member_symbol_id;

			if (parser->current.kind != TOKEN_IDENT) {
				set_error(parser, "expected member name after namespace dot");
				return -1;
			}
			member_symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_name_in_namespace(
				parser->program->asts,
				symbol_id,
				member_symbol_id,
				span,
				p_ret
			);
		}
		binder = lookup_binder(parser, symbol_id);
		if (binder) {
			return prototype_ast_var(parser->program->asts, binder->ast_binder_id, symbol_id, span, p_ret);
		}
		return prototype_ast_name(parser->program->asts, symbol_id, span, p_ret);
	}
	if (accept(parser, TOKEN_LPAREN)) {
		struct prototype_source_span span = current_span(parser);
		if (parse_term(parser, p_ret) != 0) {
			return -1;
		}
		if (expect(parser, TOKEN_RPAREN, "expected ')' after term") != 0) {
			return -1;
		}
		if (accept(parser, TOKEN_DOT)) {
			int member_symbol_id;
			if (parser->current.kind != TOKEN_IDENT) {
				set_error(parser, "expected member name after namespace dot");
				return -1;
			}
			member_symbol_id = parser->current.symbol_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			return prototype_ast_name_in_ast_namespace(
				parser->program->asts,
				*p_ret,
				member_symbol_id,
				span,
				p_ret
			);
		}
		return 0;
	}

	set_error(parser, "expected term");
	return -1;
}

static int token_starts_term_atom(int kind) {
	return kind == TOKEN_IDENT ||
		kind == TOKEN_LPAREN ||
		kind == TOKEN_LBRACE ||
		kind == TOKEN_AMPERSAND ||
		kind == TOKEN_STAR ||
		kind == TOKEN_HASH ||
		kind == TOKEN_TEXT_LITERAL ||
		kind == TOKEN_INT_LITERAL;
}

static int parse_app_term(struct parser* parser, uint32_t* p_ret) {
	uint32_t term;
	struct prototype_source_span span = current_span(parser);
	if (parse_term_atom(parser, &term) != 0) {
		return -1;
	}

	while (token_starts_term_atom(parser->current.kind)) {
		uint32_t argument;
		uint32_t app;
		if (parse_term_atom(parser, &argument) != 0) {
			return -1;
		}
		if (prototype_ast_app(parser->program->asts, term, argument, span, &app) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		term = app;
	}

	*p_ret = term;
	return 0;
}

enum parsed_elimination_head_kind {
	PARSED_ELIMINATION_HEAD_BARE = 1,
	PARSED_ELIMINATION_HEAD_TERM,
	PARSED_ELIMINATION_HEAD_RETURN
};

struct parsed_elimination_clause {
	int head_kind;
	int head_symbol_id;
	uint32_t head_ast;
	struct prototype_source_span span;
	struct prototype_ast_binder* binders;
	uint32_t binder_count;
	uint32_t body;
};

static int parse_elimination_head(
	struct parser* parser,
	struct parsed_elimination_clause* clause
) {
	if (!parser || !clause) {
		return -1;
	}
	clause->span = current_span(parser);
	clause->head_ast = PROTOTYPE_INVALID_ID;
	clause->head_symbol_id = -1;
	if (parser->current.kind == TOKEN_HASH) {
		int namespace_symbol_id;
		int symbol_id;
		const char* name;
		struct prototype_intrinsic_namespace_binding binding;
		if (read_token(parser) != 0 ||
			expect(parser, TOKEN_DOT, "expected '.' after system namespace") != 0 ||
			parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected intrinsic clause label after '@#.'");
			return -1;
		}
		namespace_symbol_id = symbol_intern(parser->program->symbols, "#", 1);
		symbol_id = parser->current.symbol_id;
		name = symbol_to_string(parser->program->symbols, symbol_id);
		if (namespace_symbol_id < 0 || !name ||
			prototype_intrinsic_namespace_lookup(
				parser->program->intrinsic_environment, name, &binding
			) != 0) {
			set_error(parser, "unknown intrinsic elimination label");
			return -1;
		}
		if (read_token(parser) != 0) {
			return -1;
		}
		if (binding.kind ==
			PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN) {
			clause->head_kind = PARSED_ELIMINATION_HEAD_RETURN;
			clause->head_symbol_id = symbol_id;
			return 0;
		}
		if (binding.kind != PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION ||
			prototype_ast_system_name(
				parser->program->asts,
				namespace_symbol_id,
				symbol_id,
				-1,
				PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION,
				PROTOTYPE_HOST_TYPE_INVALID,
				PROTOTYPE_PURE_PRIMITIVE_UNKNOWN,
				binding.target_id,
				clause->span,
				&clause->head_ast
			) != 0) {
			set_error(parser, "computation-fold clause requires an effect operation");
			return -1;
		}
		clause->head_kind = PARSED_ELIMINATION_HEAD_TERM;
		clause->head_symbol_id = symbol_id;
		return 0;
	}
	if (parser->current.kind != TOKEN_IDENT) {
		set_error(parser, "expected constructor, operation, or '#.return' after '@'");
		return -1;
	}
	int symbol_id = parser->current.symbol_id;
	const struct local_binder* local = lookup_binder(parser, symbol_id);
	clause->head_symbol_id = symbol_id;
	if (read_token(parser) != 0) {
		return -1;
	}
	if (accept(parser, TOKEN_DOT)) {
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected member name after elimination label namespace");
			return -1;
		}
		int member_symbol_id = parser->current.symbol_id;
		if (read_token(parser) != 0 || prototype_ast_name_in_namespace(
				parser->program->asts,
				symbol_id,
				member_symbol_id,
				clause->span,
				&clause->head_ast
			) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		clause->head_kind = PARSED_ELIMINATION_HEAD_TERM;
		return 0;
	}
	if ((local && prototype_ast_var(
			parser->program->asts,
			local->ast_binder_id,
			symbol_id,
			clause->span,
			&clause->head_ast
		) != 0) || (!local && prototype_ast_name(
			parser->program->asts,
			symbol_id,
			clause->span,
			&clause->head_ast
		) != 0)) {
		set_error(parser, "AST table is full");
		return -1;
	}
	clause->head_kind = PARSED_ELIMINATION_HEAD_BARE;
	return 0;
}

static int parse_elimination_suffix(
	struct parser* parser,
	uint32_t scrutinee,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct parsed_elimination_clause clauses[64];
	struct prototype_ast_binder binder_storage[256];
	struct local_binder local_binders[256];
	uint32_t clause_count = 0;
	uint32_t binder_cursor = 0;
	uint32_t return_clause = PROTOTYPE_INVALID_ID;
	uint32_t closed_binder_base = parser->closed_case_binder_count;

	while (accept(parser, TOKEN_AT)) {
		if (clause_count >= 64) {
			set_error(parser, "too many elimination clauses");
			return -1;
		}
		struct parsed_elimination_clause* clause = &clauses[clause_count];
		memset(clause, 0, sizeof(*clause));
		if (parse_elimination_head(parser, clause) != 0) {
			return -1;
		}
		clause->binders = &binder_storage[binder_cursor];
		clause->binder_count = 0;
		while (parser->current.kind == TOKEN_IDENT) {
			if (binder_cursor >= 256) {
				set_error(parser, "too many elimination binders");
				return -1;
			}
			binder_storage[binder_cursor].symbol_id = parser->current.symbol_id;
			binder_storage[binder_cursor].ast_binder_id = prototype_ast_new_binder(parser->program->asts);
			if (binder_storage[binder_cursor].ast_binder_id == PROTOTYPE_INVALID_ID) {
				set_error(parser, "binder table is full");
				return -1;
			}
			local_binders[binder_cursor].symbol_id = binder_storage[binder_cursor].symbol_id;
			local_binders[binder_cursor].ast_binder_id = binder_storage[binder_cursor].ast_binder_id;
			local_binders[binder_cursor].induction_allowed = 1;
			local_binders[binder_cursor].next = parser->binders;
			parser->binders = &local_binders[binder_cursor];
			binder_cursor++;
			clause->binder_count++;
			if (read_token(parser) != 0) {
				return -1;
			}
		}

		if (expect(parser, TOKEN_FATARROW, "expected '=>' after elimination clause") != 0) {
			return -1;
		}
		if (parse_case_body(parser, &clause->body) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < clause->binder_count; ++i) {
			parser->binders = local_binders[binder_cursor - i - 1].next;
		}
		for (uint32_t i = 0; i < clause->binder_count; ++i) {
			if (parser->closed_case_binder_count >= 256) {
				set_error(parser, "too many closed elimination binders");
				return -1;
			}
			parser->closed_case_binder_symbols[
				parser->closed_case_binder_count++
			] = clause->binders[i].symbol_id;
		}
		if (clause->head_kind == PARSED_ELIMINATION_HEAD_RETURN) {
			if (return_clause != PROTOTYPE_INVALID_ID) {
				set_error(parser, "duplicate '#.return' computation-fold clause");
				return -1;
			}
			return_clause = clause_count;
		}
		clause_count++;
	}

	if (clause_count == 0) {
		set_error(parser, "expected elimination clause");
		return -1;
	}
	if (return_clause != PROTOTYPE_INVALID_ID) {
		if (clauses[return_clause].binder_count != 1 || clause_count - 1 > 31) {
			set_error(parser, clauses[return_clause].binder_count != 1 ?
				"'#.return' clause requires exactly one binder" :
				"computation fold supports at most 31 operation clauses");
			return -1;
		}
		struct prototype_ast_computation_fold_clause_input fold_clauses[31];
		uint32_t fold_clause_count = 0;
		for (uint32_t i = 0; i < clause_count; ++i) {
			if (i == return_clause) {
				continue;
			}
			if (clauses[i].binder_count != 2 ||
				clauses[i].head_ast == PROTOTYPE_INVALID_ID) {
				set_error(parser, "operation clause requires an operation and exactly two binders");
				return -1;
			}
			fold_clauses[fold_clause_count].operation = clauses[i].head_ast;
			fold_clauses[fold_clause_count].operation_argument_binder_id =
				clauses[i].binders[0].ast_binder_id;
			fold_clauses[fold_clause_count].operation_argument_symbol_id =
				clauses[i].binders[0].symbol_id;
			fold_clauses[fold_clause_count].operation_continuation_binder_id =
				clauses[i].binders[1].ast_binder_id;
			fold_clauses[fold_clause_count].operation_continuation_symbol_id =
				clauses[i].binders[1].symbol_id;
			fold_clauses[fold_clause_count].body = clauses[i].body;
			fold_clauses[fold_clause_count].span = clauses[i].span;
			fold_clause_count++;
		}
		int status = prototype_ast_computation_fold(
			parser->program->asts,
			scrutinee,
			fold_clauses,
			fold_clause_count,
			clauses[return_clause].binders[0].ast_binder_id,
			clauses[return_clause].binders[0].symbol_id,
			clauses[return_clause].body,
			span,
			p_ret
		);
		parser->closed_case_binder_count = closed_binder_base;
		return status;
	}
	struct prototype_ast_match_case_input match_cases[64];
	for (uint32_t i = 0; i < clause_count; ++i) {
		if (clauses[i].head_kind != PARSED_ELIMINATION_HEAD_BARE) {
			set_error(parser, "ADT match requires a bare constructor label");
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (clauses[j].head_symbol_id == clauses[i].head_symbol_id) {
				set_error(parser, "duplicate match case; wrap nested match bodies in parentheses");
				return -1;
			}
		}
		match_cases[i].constructor_symbol_id = clauses[i].head_symbol_id;
		match_cases[i].binders = clauses[i].binders;
		match_cases[i].binder_count = clauses[i].binder_count;
		match_cases[i].body = clauses[i].body;
		match_cases[i].span = clauses[i].span;
	}
	int status = prototype_ast_match(
		parser->program->asts, scrutinee, match_cases, clause_count, span, p_ret
	);
	parser->closed_case_binder_count = closed_binder_base;
	return status;
}

static int parse_lambda_term(struct parser* parser, uint32_t* p_ret) {
	struct prototype_source_span span = current_span(parser);
	int binder_symbol;
	uint32_t ast_binder_id;
	uint32_t binder_type;
	uint32_t body;
	struct local_binder binder;

	if (expect(parser, TOKEN_BACKSLASH, "expected lambda") != 0) {
		return -1;
	}
	if (parser->current.kind != TOKEN_IDENT) {
		set_error(parser, "expected lambda binder");
		return -1;
	}
	binder_symbol = parser->current.symbol_id;
	ast_binder_id = prototype_ast_new_binder(parser->program->asts);
	if (ast_binder_id == PROTOTYPE_INVALID_ID) {
		set_error(parser, "binder table is full");
		return -1;
	}
	if (read_token(parser) != 0) {
		return -1;
	}
	if (expect(parser, TOKEN_COLON, "expected ':' after lambda binder") != 0) {
		return -1;
	}
	if (parse_type_expr(parser, &binder_type) != 0) {
		return -1;
	}
	if (expect(parser, TOKEN_FATARROW, "expected '=>' after lambda binder") != 0) {
		return -1;
	}

	binder.symbol_id = binder_symbol;
	binder.ast_binder_id = ast_binder_id;
	binder.induction_allowed = 0;
	binder.next = parser->binders;
	parser->binders = &binder;

	if (parse_term(parser, &body) != 0) {
		parser->binders = binder.next;
		return -1;
	}

	parser->binders = binder.next;
	return prototype_ast_lambda(parser->program->asts, ast_binder_id, binder_symbol, binder_type, body, span, p_ret);
}

static int parse_bare_lambda_term(struct parser* parser, uint32_t* p_ret) {
	struct parser saved = *parser;
	struct prototype_source_span span = current_span(parser);
	int binder_symbol;
	uint32_t ast_binder_id;
	uint32_t binder_type;
	uint32_t body;
	struct local_binder binder;

	if (parser->current.kind != TOKEN_IDENT) {
		return 0;
	}

	binder_symbol = parser->current.symbol_id;
	if (read_token(parser) != 0) {
		return -1;
	}
	if (parser->current.kind != TOKEN_COLON) {
		*parser = saved;
		return 0;
	}
	ast_binder_id = prototype_ast_new_binder(parser->program->asts);
	if (ast_binder_id == PROTOTYPE_INVALID_ID) {
		set_error(parser, "binder table is full");
		return -1;
	}
	if (read_token(parser) != 0) {
		return -1;
	}
	if (parse_type_expr(parser, &binder_type) != 0) {
		return -1;
	}
	if (expect(parser, TOKEN_FATARROW, "expected '=>' after binder") != 0) {
		return -1;
	}

	binder.symbol_id = binder_symbol;
	binder.ast_binder_id = ast_binder_id;
	binder.induction_allowed = 0;
	binder.next = parser->binders;
	parser->binders = &binder;

	if (parse_term(parser, &body) != 0) {
		parser->binders = binder.next;
		return -1;
	}

	parser->binders = binder.next;
	if (prototype_ast_lambda(parser->program->asts, ast_binder_id, binder_symbol, binder_type, body, span, p_ret) != 0) {
		set_error(parser, "AST table is full");
		return -1;
	}
	return 1;
}

static int parse_term(struct parser* parser, uint32_t* p_ret) {
	struct prototype_source_span span = current_span(parser);
	if (parser->current.kind == TOKEN_BACKSLASH) {
		if (parse_lambda_term(parser, p_ret) != 0) {
			return -1;
		}
	} else if (parser->current.kind == TOKEN_IDENT) {
		int parsed_lambda = parse_bare_lambda_term(parser, p_ret);
		if (parsed_lambda != 0) {
			if (parsed_lambda < 0) {
				return -1;
			}
		} else {
			uint32_t term;
			if (parse_app_term(parser, &term) != 0) {
				return -1;
			}
			if (parser->current.kind == TOKEN_AT) {
				if (parse_elimination_suffix(parser, term, span, p_ret) != 0) {
					return -1;
				}
			} else {
				*p_ret = term;
			}
		}
	} else {
		uint32_t term;
		if (parse_app_term(parser, &term) != 0) {
			return -1;
		}
		if (parser->current.kind == TOKEN_AT) {
			if (parse_elimination_suffix(parser, term, span, p_ret) != 0) {
				return -1;
			}
		} else {
			*p_ret = term;
		}
	}

	while (accept(parser, TOKEN_DOUBLE_COLON)) {
		uint32_t type_expr;
		uint32_t ascribed;
		if (parse_type_expr(parser, &type_expr) != 0) {
			return -1;
		}
		if (prototype_ast_ascription(parser->program->asts, *p_ret, type_expr, span, &ascribed) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		*p_ret = ascribed;
	}
	return 0;
}

static int parse_case_body(struct parser* parser, uint32_t* p_ret) {
	struct prototype_source_span span = current_span(parser);
	if (parser->current.kind == TOKEN_LBRACE) {
		if (parse_computation_block(parser, p_ret) != 0) {
			return -1;
		}
	} else if (parser->current.kind == TOKEN_AMPERSAND) {
		if (parse_app_term(parser, p_ret) != 0) {
			return -1;
		}
	} else if (parser->current.kind == TOKEN_BACKSLASH) {
		if (parse_lambda_term(parser, p_ret) != 0) {
			return -1;
		}
	} else if (parse_app_term(parser, p_ret) != 0) {
		return -1;
	}

	while (accept(parser, TOKEN_DOUBLE_COLON)) {
		uint32_t type_expr;
		uint32_t ascribed;
		if (parse_type_expr(parser, &type_expr) != 0) {
			return -1;
		}
		if (prototype_ast_ascription(parser->program->asts, *p_ret, type_expr, span, &ascribed) != 0) {
			set_error(parser, "AST table is full");
			return -1;
		}
		*p_ret = ascribed;
	}
	return 0;
}

static int parse_term_def(
	struct parser* parser,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_assignment_id
) {
	uint32_t term;
	if (parse_term(parser, &term) != 0) {
		return -1;
	}
	if (expect(parser, TOKEN_SEMI, "expected ';' after term definition") != 0) {
		return -1;
	}
	if (prototype_ast_add_term_assignment(
		parser->program->asts,
		name_symbol_id,
		term,
		source_entry_id,
		name_span,
		body_span,
		p_assignment_id
	) != 0) {
		set_error(parser, "definition table is full");
		return -1;
	}
	return 0;
}

static int parse_entry(struct parser* parser) {
	int name_symbol_id;
	uint32_t annotation_type_expr = PROTOTYPE_INVALID_ID;
	uint32_t source_entry_id;
	struct prototype_source_span name_span;
	struct prototype_source_span type_span;
	struct prototype_source_span body_span;

	if (parser->current.kind != TOKEN_IDENT) {
		set_error(parser, "expected top-level identifier");
		return -1;
	}
	const char* entry_name = symbol_to_string(
		parser->program->symbols,
		parser->current.symbol_id
	);
	if (entry_name && strcmp(entry_name, "import") == 0) {
		struct prototype_source_span import_span;
		if (read_token(parser) != 0) {
			return -1;
		}
		if (parser->current.kind != TOKEN_IDENT) {
			set_error(parser, "expected imported artifact symbol after 'import'");
			return -1;
		}
		source_entry_id = prototype_ast_new_source_entry(parser->program->asts);
		if (source_entry_id == PROTOTYPE_INVALID_ID) {
			set_error(parser, "source entry table is full");
			return -1;
		}
		name_symbol_id = parser->current.symbol_id;
		import_span = current_span(parser);
		if (read_token(parser) != 0) {
			return -1;
		}
		if (expect(parser, TOKEN_SEMI, "expected ';' after import") != 0) {
			return -1;
		}
		if (prototype_ast_add_import(
				parser->program->asts,
				name_symbol_id,
				source_entry_id,
				import_span
			) != 0) {
			set_error(parser, "import table is full");
			return -1;
		}
		return 0;
	}
	source_entry_id = prototype_ast_new_source_entry(parser->program->asts);
	if (source_entry_id == PROTOTYPE_INVALID_ID) {
		set_error(parser, "source entry table is full");
		return -1;
	}
	name_symbol_id = parser->current.symbol_id;
	name_span = current_span(parser);
	if (read_token(parser) != 0) {
		return -1;
	}
	if (accept(parser, TOKEN_DOUBLE_COLON)) {
		uint32_t expectation_id;
		type_span = current_span(parser);
		if (parse_type_expr(parser, &annotation_type_expr) != 0) {
			return -1;
		}
		if (parser->current.kind != TOKEN_SEMI) {
			set_error(parser, "top-level '::' expects a standalone type expectation");
			return -1;
		}
		if (read_token(parser) != 0) {
			return -1;
		}
		if (prototype_ast_add_type_expectation(
			parser->program->asts,
			PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION,
			name_symbol_id,
			annotation_type_expr,
			source_entry_id,
			name_span,
			type_span,
			PROTOTYPE_INVALID_ID,
			&expectation_id
		) != 0) {
			set_error(parser, "expectation table is full");
			return -1;
		}
		return 0;
	}
	if (accept(parser, TOKEN_COLON)) {
		type_span = current_span(parser);
		if (parse_type_expr(parser, &annotation_type_expr) != 0) {
			return -1;
		}
		if (parser->current.kind == TOKEN_SEMI) {
			uint32_t expectation_id;
			if (read_token(parser) != 0) {
				return -1;
			}
			if (prototype_ast_add_type_expectation(
				parser->program->asts,
				PROTOTYPE_AST_TYPE_ENTRY_DECLARATION,
				name_symbol_id,
				annotation_type_expr,
				source_entry_id,
				name_span,
				type_span,
				PROTOTYPE_INVALID_ID,
				&expectation_id
			) != 0) {
				set_error(parser, "expectation table is full");
				return -1;
			}
			return 0;
		}
		set_error(parser, "top-level ':' declares an external name and cannot be combined with ':='; use 'name : Type;' or 'name := body; name :: Type;' instead");
		return -1;
	}
	if (expect(parser, TOKEN_ASSIGN, "expected ':=' after top-level identifier") != 0) {
		return -1;
	}
	body_span = current_span(parser);

	uint32_t assignment_id = PROTOTYPE_INVALID_ID;

	if (parser->current.kind == TOKEN_AT) {
		if (parse_anonymous_type_def(
			parser,
			name_symbol_id,
			source_entry_id,
			name_span,
			body_span,
			&assignment_id
		) != 0) {
			return -1;
		}
	} else if (parser->current.kind == TOKEN_BACKSLASH) {
		if (parse_parameterized_type_or_lambda_def(
			parser,
			name_symbol_id,
			source_entry_id,
			name_span,
			body_span,
			&assignment_id
		) != 0) {
			return -1;
		}
	} else if (parse_term_def(
		parser,
		name_symbol_id,
		source_entry_id,
		name_span,
		body_span,
		&assignment_id
	) != 0) {
		return -1;
	}

	(void)assignment_id;
	return 0;
}

static int finish_definition_block(
	struct parser* parser,
	uint32_t first_assignment,
	struct prototype_source_span block_span,
	int selected_symbol_id,
	struct prototype_source_span select_span
) {
	if (!parser || first_assignment > parser->program->asts->assignment_count) {
		return -1;
	}
	uint32_t assignment_count =
		(uint32_t)parser->program->asts->assignment_count - first_assignment;
	if (assignment_count == 0) {
		set_error(parser, "definition block requires at least one definition");
		return -1;
	}
	if (assignment_count > PARSER_BLOCK_ITEM_CAPACITY) {
		set_error(parser, "definition block is too large");
		return -1;
	}
	uint32_t assignment_ids[PARSER_BLOCK_ITEM_CAPACITY];
	int selected_found = 0;
	for (uint32_t i = 0; i < assignment_count; ++i) {
		uint32_t assignment_id = first_assignment + i;
		assignment_ids[i] = assignment_id;
		struct prototype_ast_term_assignment_def* assignment =
			&parser->program->asts->assignments[assignment_id];
		assignment->definition_value_required = 1;
		if (selected_symbol_id >= 0 &&
			assignment->name_symbol_id == selected_symbol_id) {
			selected_found++;
		}
	}
	if (selected_found != 1) {
		set_error(parser, selected_found == 0 ?
			"selected definition is not a direct member of the definition block" :
			"selected definition is ambiguous");
		return -1;
	}
	uint32_t definition_block;
	if (prototype_ast_definition_block(
			parser->program->asts,
			assignment_ids,
			assignment_count,
			block_span,
			&definition_block
		) != 0) {
		set_error(parser, "definition block table is full");
		return -1;
	}
	parser->program->asts->root_definition_block = definition_block;
	if (selected_found == 1) {
		uint32_t definition_select;
		if (prototype_ast_definition_select(
				parser->program->asts,
				definition_block,
				selected_symbol_id,
				select_span,
				&definition_select
			) != 0) {
			set_error(parser, "definition selection table is full");
			return -1;
		}
		parser->program->asts->root_definition_select = definition_select;
	}
	return 0;
}

static int parse_program(struct parser* parser) {
	if (read_token(parser) != 0) {
		return -1;
	}
	if (parser->current.kind == TOKEN_LBRACE) {
		struct prototype_source_span span = current_span(parser);
		uint32_t first_assignment = (uint32_t)parser->program->asts->assignment_count;
		if (read_token(parser) != 0) {
			return -1;
		}
		if (parser->current.kind == TOKEN_LBRACE) {
			if (read_token(parser) != 0) {
				return -1;
			}
			while (parser->current.kind != TOKEN_RBRACE) {
				if (parser->current.kind == TOKEN_EOF) {
					set_error(parser, "expected '}}' after definition block");
					return -1;
				}
				if (parse_entry(parser) != 0) {
					return -1;
				}
			}
			if (expect(parser, TOKEN_RBRACE, "expected first '}' after definition block") != 0 ||
				expect(parser, TOKEN_RBRACE, "expected second '}' after definition block") != 0 ||
				expect(parser, TOKEN_DOT, "expected '.name' after definition block") != 0 ||
				parser->current.kind != TOKEN_IDENT) {
				if (!parser->error || parser->error->message[0] == '\0') {
					set_error(parser, "expected selected definition name after definition block");
				}
				return -1;
			}
			int selected_symbol_id = parser->current.symbol_id;
			struct prototype_source_span select_span = current_span(parser);
			if (read_token(parser) != 0) {
				return -1;
			}
			(void)accept(parser, TOKEN_SEMI);
			if (finish_definition_block(
					parser,
					first_assignment,
					span,
					selected_symbol_id,
					select_span
				) != 0) {
				return -1;
			}
			if (parser->current.kind != TOKEN_EOF) {
				set_error(parser, "definition block selection must be the program root");
				return -1;
			}
			return 0;
		}
		set_error(parser, "a program-level computation block is not a definition block; use '{{'");
		return -1;
	}
	while (parser->current.kind != TOKEN_EOF) {
		if (parse_entry(parser) != 0) {
			return -1;
		}
	}
	return 0;
}

static char* read_file_bytes(const char* path) {
	FILE* file = fopen(path, "rb");
	if (!file) {
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long len = ftell(file);
	if (len < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	char* bytes = (char*)malloc((size_t)len + 1);
	if (!bytes) {
		fclose(file);
		return NULL;
	}

	size_t read_len = fread(bytes, 1, (size_t)len, file);
	fclose(file);
	if (read_len != (size_t)len) {
		free(bytes);
		return NULL;
	}
	bytes[len] = '\0';
	return bytes;
}

static char* duplicate_input(const char* input) {
	size_t len = strlen(input);
	char* copy = (char*)malloc(len + 1);
	if (!copy) {
		return NULL;
	}
	memcpy(copy, input, len + 1);
	return copy;
}

static int read_from_owned_input(
	const char* name,
	char* input,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
) {
	struct parser parser;
	memset(&parser, 0, sizeof(parser));
	if (error) {
		memset(error, 0, sizeof(*error));
	}

	if (!name || !input || !program || !program->symbols || !program->asts) {
		if (error) {
			error->filename = name;
			snprintf(error->message, sizeof(error->message), "%s", "invalid reader arguments");
		}
		free(input);
		return -1;
	}

	parser.input = input;
	parser.input_len = strlen(input);
	parser.filename = name;
	parser.line = 1;
	parser.column = 1;
	parser.program = program;
	parser.error = error;
	if (options) {
		parser.options = *options;
	}

	int result = parse_program(&parser);
	free(parser.input);
	return result;
}

int prototype_read_ast_file_with_options(
	const char* path,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
) {
	char* input = NULL;
	if (path) {
		input = read_file_bytes(path);
	}
	if (!input) {
		if (error) {
			memset(error, 0, sizeof(*error));
			error->filename = path;
			snprintf(error->message, sizeof(error->message), "%s", "failed to read file");
		}
		return -1;
	}
	return read_from_owned_input(path, input, program, options, error);
}

int prototype_read_ast_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
) {
	return prototype_read_ast_file_with_options(path, program, NULL, error);
}

int prototype_read_ast_string_with_options(
	const char* name,
	const char* input,
	struct prototype_program* program,
	const struct prototype_read_options* options,
	struct prototype_read_error* error
) {
	char* copy = input ? duplicate_input(input) : NULL;
	if (!copy) {
		if (error) {
			memset(error, 0, sizeof(*error));
			error->filename = name;
			snprintf(error->message, sizeof(error->message), "%s", "failed to copy input");
		}
		return -1;
	}
	return read_from_owned_input(name ? name : "<interactive>", copy, program, options, error);
}

int prototype_read_ast_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
) {
	return prototype_read_ast_string_with_options(name, input, program, NULL, error);
}

int prototype_read_file(
	const char* path,
	struct prototype_program* program,
	struct prototype_read_error* error
) {
	if (prototype_read_ast_file(path, program, error) != 0) {
		return -1;
	}
	return prototype_compile_graph(program, error);
}

int prototype_read_string(
	const char* name,
	const char* input,
	struct prototype_program* program,
	struct prototype_read_error* error
) {
	if (prototype_read_ast_string(name, input, program, error) != 0) {
		return -1;
	}
	return prototype_compile_graph(program, error);
}
