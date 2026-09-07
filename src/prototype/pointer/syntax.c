#include "syntax.h"

#include <stdlib.h>
#include <string.h>

static void error(struct pg_parser *parser, const char *message)
{
	if (parser->error) return;
	parser->error = message;
	parser->error_token = parser->reader.token;
}

static int advance(struct pg_parser *parser)
{
	int kind = pg_reader_next(&parser->reader);
	if (kind == PG_TOKEN_ERROR) error(parser, parser->reader.error);
	return kind;
}

static int require(struct pg_parser *parser, int kind, const char *message)
{
	if (parser->reader.token.kind != kind) {
		error(parser, message);
		return -1;
	}
	return advance(parser) == PG_TOKEN_ERROR ? -1 : 0;
}

static const struct pg_syntax *node(struct pg_parser *parser, enum pg_syntax_kind kind,
	struct pg_token token, const struct pg_syntax *left, const struct pg_syntax *right)
{
	if (parser->error) return NULL;
	struct pg_syntax *syntax = pg_alloc(parser->arena, sizeof(*syntax));
	if (!syntax) {
		error(parser, "syntax allocation failed");
		return NULL;
	}
	*syntax = (struct pg_syntax){.kind = kind, .token = token, .left = left, .right = right};
	return syntax;
}

static const struct pg_syntax *expression(struct pg_parser *parser);

/* Index binders occur inside the declaration marker. Outer lambdas remain
 * parameters; the parser does not substitute the declaration's source name. */
static const struct pg_syntax *declaration_body(struct pg_parser *parser)
{
	if (parser->reader.token.kind == '\\') {
		advance(parser);
		struct pg_token binder = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected index binder") != 0) return NULL;
		if (require(parser, ':', "expected index domain") != 0) return NULL;
		const struct pg_syntax *domain = expression(parser);
		if (require(parser, PG_TOKEN_LAMBDA_ARROW, "expected '=>' after index domain") != 0) return NULL;
		const struct pg_syntax *body = declaration_body(parser);
		return node(parser, PG_SYNTAX_LAMBDA, binder, domain, body);
	}
	struct pg_token opening = parser->reader.token;
	if (require(parser, '{', "expected constructor block") != 0) return NULL;
	struct pg_syntax_item *items = NULL;
	size_t count = 0, capacity = 0;
	const struct pg_syntax *result = NULL;
	while (!parser->error && parser->reader.token.kind != '}') {
		struct pg_token name = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected constructor name") != 0) goto done;
		if (require(parser, ':', "expected ':' after constructor name") != 0) goto done;
		const struct pg_syntax *type = expression(parser);
		if (!type) goto done;
		if (require(parser, ';', "expected ';' after constructor classifier") != 0) goto done;
		if (count == capacity) {
			size_t next = capacity ? capacity * 2 : 8;
			if (next < capacity || next > SIZE_MAX / sizeof(*items)) {
				error(parser, "constructor storage overflow");
				goto done;
			}
			struct pg_syntax_item *grown = realloc(items, next * sizeof(*items));
			if (!grown) {
				error(parser, "constructor storage allocation failed");
				goto done;
			}
			items = grown;
			capacity = next;
		}
		items[count++] = (struct pg_syntax_item){name, type};
	}
	if (require(parser, '}', "expected '}' after constructors") != 0) goto done;
	struct pg_syntax_item *stored = pg_alloc(parser->arena, count * sizeof(*stored));
	struct pg_syntax *block = pg_alloc(parser->arena, sizeof(*block));
	if (!stored || !block) {
		error(parser, "constructor syntax allocation failed");
		goto done;
	}
	if (count) memcpy(stored, items, count * sizeof(*stored));
	*block = (struct pg_syntax){.kind = PG_SYNTAX_CONSTRUCTORS, .token = opening,
		.item_count = count, .items = stored};
	result = block;
done:
	free(items);
	return result;
}

static int atom_start(int kind)
{
	switch (kind) {
	case PG_TOKEN_IDENT: case PG_TOKEN_TEXT: case PG_TOKEN_INT:
	case '(': case '@': case '#': case '*': case '&':
		return 1;
	default: return 0;
	}
}

static const struct pg_syntax *atom(struct pg_parser *parser)
{
	struct pg_token token = parser->reader.token;
	if (!atom_start(token.kind)) {
		error(parser, "expected expression atom");
		return NULL;
	}
	advance(parser);
	const struct pg_syntax *result = NULL;
	if (token.kind == '(') {
		result = expression(parser);
		if (parser->reader.token.kind == ':') {
			if (!result) return NULL;
			if (result->kind != PG_SYNTAX_ATOM || result->token.kind != PG_TOKEN_IDENT) {
				error(parser, "dependent binder requires a name");
				return NULL;
			}
			advance(parser);
			const struct pg_syntax *domain = expression(parser);
			result = node(parser, PG_SYNTAX_BINDER, result->token, domain, NULL);
		}
		if (require(parser, ')', "expected ')' after expression") != 0) return NULL;
	} else if (token.kind == '&') {
		result = node(parser, PG_SYNTAX_QUOTE, token, atom(parser), NULL);
	} else {
		if (token.kind == '@') {
			int next = parser->reader.token.kind;
			if (next == '{' || next == '\\') {
				const struct pg_syntax *body = declaration_body(parser);
				return node(parser, PG_SYNTAX_DECLARATION, token, body, NULL);
			}
			if (next == PG_TOKEN_IDENT) {
				error(parser, "graph-selector grammar not implemented yet");
				return NULL;
			}
		}
		result = node(parser, PG_SYNTAX_ATOM, token, NULL, NULL);
	}
	while (!parser->error && parser->reader.token.kind == '.') {
		struct pg_token dot = parser->reader.token;
		advance(parser);
		struct pg_token name = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected name after '.'") != 0) return NULL;
		const struct pg_syntax *member = node(parser, PG_SYNTAX_ATOM, name, NULL, NULL);
		result = node(parser, PG_SYNTAX_QUALIFIED, dot, result, member);
	}
	return result;
}

static const struct pg_syntax *application(struct pg_parser *parser)
{
	const struct pg_syntax *result = atom(parser);
	while (!parser->error && atom_start(parser->reader.token.kind)) {
		struct pg_token token = parser->reader.token;
		if (token.kind == '@') {
			error(parser, "elimination grammar not implemented yet");
			return NULL;
		}
		const struct pg_syntax *argument = atom(parser);
		if (!result || !argument) return NULL;
		if (result->kind == PG_SYNTAX_BINDER || argument->kind == PG_SYNTAX_BINDER) {
			error(parser, "dependent binder is only valid before '->'");
			return NULL;
		}
		result = node(parser, PG_SYNTAX_APPLICATION, token, result, argument);
	}
	return result;
}

static const struct pg_syntax *arrow(struct pg_parser *parser)
{
	if (parser->reader.token.kind == '\\') {
		advance(parser);
		struct pg_token binder = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected lambda binder") != 0) return NULL;
		if (require(parser, ':', "lambda requires a domain annotation") != 0) return NULL;
		const struct pg_syntax *domain = expression(parser);
		if (require(parser, PG_TOKEN_LAMBDA_ARROW, "expected '=>' after lambda domain") != 0) return NULL;
		const struct pg_syntax *body = expression(parser);
		return node(parser, PG_SYNTAX_LAMBDA, binder, domain, body);
	}
	const struct pg_syntax *domain = application(parser);
	if (parser->error) return NULL;
	if (parser->reader.token.kind != PG_TOKEN_ARROW) {
		if (domain->kind == PG_SYNTAX_BINDER) {
			error(parser, "dependent binder requires '->'");
			return NULL;
		}
		return domain;
	}
	struct pg_token token = parser->reader.token;
	advance(parser);
	const struct pg_syntax *codomain = arrow(parser);
	return node(parser, PG_SYNTAX_PI, token, domain, codomain);
}

static const struct pg_syntax *expression(struct pg_parser *parser)
{
	const struct pg_syntax *result = arrow(parser);
	while (!parser->error && parser->reader.token.kind == PG_TOKEN_EXPECT) {
		struct pg_token token = parser->reader.token;
		advance(parser);
		const struct pg_syntax *expected = arrow(parser);
		result = node(parser, PG_SYNTAX_EXPECT, token, result, expected);
	}
	return result;
}

void pg_parser_init(struct pg_parser *parser, struct pg_graph *arena,
	const char *input, size_t length)
{
	*parser = (struct pg_parser){.arena = arena};
	pg_reader_init(&parser->reader, input, length);
	advance(parser);
}

int pg_parser_next(struct pg_parser *parser, struct pg_definition *definition)
{
	if (parser->error) return -1;
	if (parser->reader.token.kind == PG_TOKEN_EOF) return 0;
	struct pg_token name = parser->reader.token;
	if (require(parser, PG_TOKEN_IDENT, "expected top-level name") != 0) return -1;
	int operation = parser->reader.token.kind;
	if (operation != PG_TOKEN_ASSIGN && operation != PG_TOKEN_EXPECT) {
		error(parser, "expected ':=' or '::' after top-level name");
		return -1;
	}
	advance(parser);
	const struct pg_syntax *value = expression(parser);
	if (require(parser, ';', "expected ';' after top-level entry") != 0) return -1;
	if (!value) return -1;
	*definition = (struct pg_definition){name, operation, value};
	return 1;
}
