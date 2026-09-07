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

static struct pg_syntax *node(struct pg_parser *parser, enum pg_syntax_kind kind,
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
static const struct pg_syntax *expression_mode(struct pg_parser *parser, int eliminate);

static int starts_annotated_binder(const struct pg_parser *parser)
{
	if (parser->reader.token.kind != PG_TOKEN_IDENT) return 0;
	struct pg_reader lookahead = parser->reader;
	return pg_reader_next(&lookahead) == ':';
}

static int is_import(const struct pg_token *token)
{
	return token->kind == PG_TOKEN_IDENT && token->text_length == 6 && memcmp(token->text, "import", 6) == 0;
}

static const struct pg_syntax *import_entry(struct pg_parser *parser)
{
	struct pg_token token = parser->reader.token;
	advance(parser);
	struct pg_token name = parser->reader.token;
	if (require(parser, PG_TOKEN_IDENT, "expected imported artifact name") != 0) return NULL;
	const struct pg_syntax *target = node(parser, PG_SYNTAX_ATOM, name, NULL, NULL);
	return node(parser, PG_SYNTAX_IMPORT, token, target, NULL);
}

struct item_buffer {
	struct pg_syntax_item *items;
	size_t count, capacity;
};

static int append_item(struct pg_parser *parser, struct item_buffer *buffer, struct pg_syntax_item item)
{
	if (buffer->count == buffer->capacity) {
		size_t next = buffer->capacity ? buffer->capacity * 2 : 8;
		if (next < buffer->capacity || next > SIZE_MAX / sizeof(*buffer->items)) {
			error(parser, "syntax item storage overflow");
			return -1;
		}
		struct pg_syntax_item *grown = realloc(buffer->items, next * sizeof(*grown));
		if (!grown) {
			error(parser, "syntax item allocation failed");
			return -1;
		}
		buffer->items = grown;
		buffer->capacity = next;
	}
	buffer->items[buffer->count++] = item;
	return 0;
}

static struct pg_syntax *item_node(struct pg_parser *parser, enum pg_syntax_kind kind,
	struct pg_token token, const struct item_buffer *buffer)
{
	struct pg_syntax_item *stored = pg_alloc(parser->arena, buffer->count * sizeof(*stored));
	struct pg_syntax *result = pg_alloc(parser->arena, sizeof(*result));
	if (!stored || !result) {
		error(parser, "syntax list allocation failed");
		return NULL;
	}
	if (buffer->count) memcpy(stored, buffer->items, buffer->count * sizeof(*stored));
	*result = (struct pg_syntax){.kind = kind, .token = token,
		.item_count = buffer->count, .items = stored};
	return result;
}

/* Index binders occur inside the declaration marker. Outer lambdas remain
 * parameters; the parser does not substitute the declaration's source name. */
static const struct pg_syntax *declaration_body(struct pg_parser *parser)
{
	if (parser->reader.token.kind == '@') {
		advance(parser);
		if (parser->reader.token.kind != '\\') {
			error(parser, "expected index lambda after '@'");
			return NULL;
		}
	}
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
	struct item_buffer buffer = {0};
	const struct pg_syntax *result = NULL;
	while (!parser->error && parser->reader.token.kind != '}') {
		struct pg_token name = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected constructor name") != 0) goto done;
		if (require(parser, ':', "expected ':' after constructor name") != 0) goto done;
		const struct pg_syntax *type = expression(parser);
		if (!type) goto done;
		if (require(parser, ';', "expected ';' after constructor classifier") != 0) goto done;
		if (append_item(parser, &buffer, (struct pg_syntax_item){.name = name, .expression = type, .operation = ':'}) != 0) goto done;
	}
	if (require(parser, '}', "expected '}' after constructors") != 0) goto done;
	result = item_node(parser, PG_SYNTAX_CONSTRUCTORS, opening, &buffer);
done:
	free(buffer.items);
	return result;
}

/* The opening brace has already been consumed. Definition blocks are parsed
 * only at the program root; ordinary blocks preserve source-order statements. */
static const struct pg_syntax *block(struct pg_parser *parser, struct pg_token opening, int definitions)
{
	struct item_buffer buffer = {0};
	const struct pg_syntax *result = NULL;
	while (!parser->error && parser->reader.token.kind != '}') {
		struct pg_syntax_item item = {0};
		struct pg_token token = parser->reader.token;
		struct pg_reader lookahead = parser->reader;
		int next = pg_reader_next(&lookahead);
		int named = token.kind == PG_TOKEN_IDENT && (next == PG_TOKEN_ASSIGN || next == ':');
		if (definitions) named = 1;
		if (definitions && is_import(&token)) {
			item.expression = import_entry(parser);
			item.operation = PG_SYNTAX_IMPORT;
			if (item.expression) item.name = item.expression->left->token;
		} else if (named) {
			item.name = token;
			if (require(parser, PG_TOKEN_IDENT, "expected block binding name") != 0) goto done;
			if (!definitions && parser->reader.token.kind == ':') {
				advance(parser);
				item.annotation = expression(parser);
			}
			item.operation = parser->reader.token.kind;
			if (item.operation != PG_TOKEN_ASSIGN && !(definitions && item.operation == PG_TOKEN_EXPECT)) {
				error(parser, "expected block assignment or definition check");
				goto done;
			}
			advance(parser);
			item.expression = expression(parser);
		} else if (token.kind == '!') {
			advance(parser);
			const struct pg_syntax *value = expression(parser);
			item.expression = node(parser, PG_SYNTAX_EXIT, token, value, NULL);
		} else {
			item.expression = expression(parser);
		}
		if (!item.expression) goto done;
		if (require(parser, ';', "expected ';' after block item") != 0) goto done;
		if (append_item(parser, &buffer, item) != 0) goto done;
	}
	if (!definitions && !buffer.count) {
		error(parser, "computation block requires an item");
		goto done;
	}
	if (require(parser, '}', "expected '}' after block") != 0) goto done;
	if (definitions && require(parser, '}', "expected second '}' after definitions") != 0) goto done;
	result = item_node(parser, definitions ? PG_SYNTAX_DEFINITIONS : PG_SYNTAX_BLOCK, opening, &buffer);
done:
	free(buffer.items);
	return result;
}

static int atom_start(int kind)
{
	switch (kind) {
	case PG_TOKEN_IDENT: case PG_TOKEN_TEXT: case PG_TOKEN_INT:
	case '(': case '@': case '#': case '*': case '&': case '{':
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
	if (token.kind == '{') {
		result = block(parser, token, 0);
	} else if (token.kind == '(') {
		if (starts_annotated_binder(parser)) {
			struct pg_token binder = parser->reader.token;
			advance(parser);
			advance(parser);
			const struct pg_syntax *domain = expression(parser);
			if (parser->reader.token.kind == PG_TOKEN_LAMBDA_ARROW) {
				advance(parser);
				const struct pg_syntax *body = expression(parser);
				result = node(parser, PG_SYNTAX_LAMBDA, binder, domain, body);
			} else result = node(parser, PG_SYNTAX_BINDER, binder, domain, NULL);
		} else result = expression(parser);
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
				struct pg_token name = parser->reader.token;
				advance(parser);
				const struct pg_syntax *target = node(parser, PG_SYNTAX_ATOM, name, NULL, NULL);
				return node(parser, PG_SYNTAX_GRAPH_REFERENCE, token, target, NULL);
			}
		}
		if (token.kind == '#' && parser->reader.token.kind != '.') {
			error(parser, "expected '.' after intrinsic namespace");
			return NULL;
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

static int starts_clause(const struct pg_parser *parser)
{
	struct pg_reader lookahead = parser->reader;
	if (pg_reader_next(&lookahead) != PG_TOKEN_IDENT) return 1;
	int kind;
	do kind = pg_reader_next(&lookahead); while (kind == PG_TOKEN_IDENT);
	return kind == PG_TOKEN_LAMBDA_ARROW || kind == '{' || kind == '.';
}

static const struct pg_syntax *application(struct pg_parser *parser)
{
	const struct pg_syntax *result = atom(parser);
	while (!parser->error && atom_start(parser->reader.token.kind)) {
		struct pg_token token = parser->reader.token;
		if (token.kind == '@' && starts_clause(parser)) {
			break;
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

static const struct pg_syntax *arrow(struct pg_parser *parser, int eliminate)
{
	if (parser->reader.token.kind == '\\' || starts_annotated_binder(parser)) {
		if (parser->reader.token.kind == '\\') advance(parser);
		int marker = 0;
		if (parser->reader.token.kind == '@' || parser->reader.token.kind == '*') {
			marker = parser->reader.token.kind;
			advance(parser);
		}
		struct pg_token binder = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected lambda binder") != 0) return NULL;
		const struct pg_syntax *domain = NULL;
		if (marker != '*') {
			if (require(parser, ':', "lambda requires a domain annotation") != 0) return NULL;
			domain = expression(parser);
		}
		if (require(parser, PG_TOKEN_LAMBDA_ARROW, "expected '=>' after lambda domain") != 0) return NULL;
		const struct pg_syntax *body = expression_mode(parser, eliminate);
		struct pg_syntax *lambda = node(parser, PG_SYNTAX_LAMBDA, binder, domain, body);
		if (lambda) lambda->binder_marker = marker;
		return lambda;
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
	const struct pg_syntax *codomain = arrow(parser, eliminate);
	return node(parser, PG_SYNTAX_PI, token, domain, codomain);
}

static const struct pg_syntax *elimination(struct pg_parser *parser, const struct pg_syntax *scrutinee)
{
	struct pg_token opening = parser->reader.token;
	struct item_buffer clauses = {0};
	struct item_buffer binders = {0};
	const struct pg_syntax *result = NULL;
	while (!parser->error && parser->reader.token.kind == '@') {
		struct pg_token marker = parser->reader.token;
		advance(parser);
		int kind = parser->reader.token.kind;
		if (kind != PG_TOKEN_IDENT && kind != '#') {
			error(parser, "expected elimination label");
			goto done;
		}
		const struct pg_syntax *head = atom(parser);
		if (!head) goto done;
		binders.count = 0;
		while (parser->reader.token.kind == PG_TOKEN_IDENT) {
			struct pg_syntax_item binder = {.name = parser->reader.token};
			advance(parser);
			if (append_item(parser, &binders, binder) != 0) goto done;
		}
		if (parser->reader.token.kind == '{') {
			if (binders.count) {
				error(parser, "named selectors cannot follow positional binders");
				goto done;
			}
			advance(parser);
			while (!parser->error && parser->reader.token.kind != '}') {
				struct pg_token name = parser->reader.token;
				if (require(parser, PG_TOKEN_IDENT, "expected named selector") != 0) goto done;
				struct pg_token alias = name;
				if (parser->reader.token.kind == PG_TOKEN_ASSIGN) {
					advance(parser);
					alias = parser->reader.token;
					if (require(parser, PG_TOKEN_IDENT, "expected selector alias") != 0) goto done;
				}
				const struct pg_syntax *local = node(parser, PG_SYNTAX_ATOM, alias, NULL, NULL);
				if (require(parser, ';', "expected ';' after selector") != 0) goto done;
				if (append_item(parser, &binders, (struct pg_syntax_item){.name = name,
					.expression = local, .operation = PG_TOKEN_ASSIGN}) != 0) goto done;
			}
			if (require(parser, '}', "expected '}' after selectors") != 0) goto done;
		}
		if (require(parser, PG_TOKEN_LAMBDA_ARROW, "expected '=>' after clause binders") != 0) goto done;
		const struct pg_syntax *body = expression_mode(parser, 0);
		if (!body) goto done;
		struct pg_syntax *clause = item_node(parser, PG_SYNTAX_CLAUSE, marker, &binders);
		if (!clause) goto done;
		clause->left = head;
		clause->right = body;
		if (append_item(parser, &clauses, (struct pg_syntax_item){.expression = clause}) != 0) goto done;
	}
	struct pg_syntax *match = item_node(parser, PG_SYNTAX_ELIMINATION, opening, &clauses);
	if (match) match->left = scrutinee;
	result = match;
done:
	free(clauses.items);
	free(binders.items);
	return result;
}

static const struct pg_syntax *expression_mode(struct pg_parser *parser, int eliminate)
{
	const struct pg_syntax *result = arrow(parser, eliminate);
	if (!result) return NULL;
	if (eliminate && parser->reader.token.kind == '@') result = elimination(parser, result);
	while (!parser->error && parser->reader.token.kind == PG_TOKEN_EXPECT) {
		struct pg_token token = parser->reader.token;
		advance(parser);
		const struct pg_syntax *expected = arrow(parser, eliminate);
		result = node(parser, PG_SYNTAX_EXPECT, token, result, expected);
	}
	return result;
}

static const struct pg_syntax *expression(struct pg_parser *parser)
{
	return expression_mode(parser, 1);
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
	if (is_import(&parser->reader.token)) {
		const struct pg_syntax *import = import_entry(parser);
		if (require(parser, ';', "expected ';' after import") != 0) return -1;
		if (!import) return -1;
		*definition = (struct pg_definition){import->left->token, PG_SYNTAX_IMPORT, import};
		++parser->entries;
		return 1;
	}
	if (parser->reader.token.kind == '{') {
		if (parser->entries) {
			error(parser, "definition block must be the whole program root");
			return -1;
		}
		struct pg_token opening = parser->reader.token;
		advance(parser);
		if (require(parser, '{', "program root requires '{{'") != 0) return -1;
		const struct pg_syntax *definitions = block(parser, opening, 1);
		struct pg_token dot = parser->reader.token;
		if (require(parser, '.', "definition block requires '.name'") != 0) return -1;
		struct pg_token selected = parser->reader.token;
		if (require(parser, PG_TOKEN_IDENT, "expected selected definition") != 0) return -1;
		const struct pg_syntax *member = node(parser, PG_SYNTAX_ATOM, selected, NULL, NULL);
		const struct pg_syntax *selection = node(parser, PG_SYNTAX_QUALIFIED, dot, definitions, member);
		if (parser->reader.token.kind == ';') advance(parser);
		if (parser->reader.token.kind != PG_TOKEN_EOF) error(parser, "unexpected input after program root");
		if (parser->error) return -1;
		*definition = (struct pg_definition){selected, '{', selection};
		++parser->entries;
		return 1;
	}
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
	++parser->entries;
	return 1;
}

const struct pg_syntax *pg_parser_program(struct pg_parser *parser)
{
	if (parser->entries) {
		error(parser, "program parsing requires an unread source");
		return NULL;
	}
	struct item_buffer buffer = {0};
	struct pg_token opening = parser->reader.token;
	const struct pg_syntax *result = NULL;
	struct pg_definition definition;
	int status;
	while ((status = pg_parser_next(parser, &definition)) > 0) {
		if (definition.operation == '{') {
			result = definition.expression;
			goto done;
		}
		struct pg_syntax_item item = {.name = definition.name,
			.expression = definition.expression, .operation = definition.operation};
		if (append_item(parser, &buffer, item) != 0) goto done;
	}
	if (status == 0) result = item_node(parser, PG_SYNTAX_DEFINITIONS, opening, &buffer);
done:
	free(buffer.items);
	return result;
}
