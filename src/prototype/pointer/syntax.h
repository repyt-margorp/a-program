#ifndef A_PROGRAM_POINTER_SYNTAX_H
#define A_PROGRAM_POINTER_SYNTAX_H

#include "graph.h"
#include "reader.h"

enum pg_syntax_kind {
	PG_SYNTAX_ATOM,
	PG_SYNTAX_QUALIFIED,
	PG_SYNTAX_APPLICATION,
	PG_SYNTAX_LAMBDA,
	PG_SYNTAX_PI,
	PG_SYNTAX_BINDER,
	PG_SYNTAX_EXPECT,
	PG_SYNTAX_QUOTE,
	PG_SYNTAX_DECLARATION,
	PG_SYNTAX_CONSTRUCTORS,
	PG_SYNTAX_BLOCK,
	PG_SYNTAX_DEFINITIONS,
	PG_SYNTAX_EXIT,
	PG_SYNTAX_ELIMINATION,
	PG_SYNTAX_CLAUSE,
	PG_SYNTAX_GRAPH_REFERENCE,
	PG_SYNTAX_IMPORT
};

struct pg_syntax_item {
	struct pg_token name;
	const struct pg_syntax *expression;
	const struct pg_syntax *annotation;
	int operation;
};

/* Source syntax is not executable Core or accepted typing evidence. Token
 * text borrows the source buffer; nodes belong to the supplied arena. */
struct pg_syntax {
	enum pg_syntax_kind kind;
	struct pg_token token;
	const struct pg_syntax *left;
	const struct pg_syntax *right;
	size_t item_count;
	const struct pg_syntax_item *items;
	int binder_marker;
};

struct pg_definition {
	struct pg_token name;
	int operation;
	const struct pg_syntax *expression;
};

struct pg_parser {
	struct pg_reader reader;
	struct pg_graph *arena;
	const char *error;
	struct pg_token error_token;
	size_t entries;
};
void pg_parser_init(struct pg_parser *parser, struct pg_graph *arena,
	const char *input, size_t length);
/* 1 entry, 0 end, -1 error. operation '{' denotes a root definition-block
 * selection; PG_SYNTAX_IMPORT an import; otherwise ':=' or '::'.
 * No name resolution or type synthesis. */
int pg_parser_next(struct pg_parser *parser, struct pg_definition *definition);

#endif
