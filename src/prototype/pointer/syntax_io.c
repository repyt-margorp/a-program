#include "syntax_io.h"
#include "dag.h"
#include "wire.h"
#include <string.h>

static const char magic[8] = "APGSYN\1";

static int child(void *unused, const void *key, size_t index, const void **result)
{
	(void)unused;
	const struct pg_syntax *syntax = key;
	if (syntax->item_count > (SIZE_MAX - 2) / 2 || (syntax->item_count && !syntax->items)) return -1;
	if (index >= 2 + 2 * syntax->item_count) return 0;
	if (index < 2) *result = index ? syntax->right : syntax->left;
	else {
		const struct pg_syntax_item *item = &syntax->items[(index - 2) / 2];
		*result = index % 2 ? item->annotation : item->expression;
	}
	return *result ? 1 : 2;
}

static int named(const struct pg_token *token)
{
	return token->kind == PG_TOKEN_IDENT && token->text && token->text_length
		&& token->length == token->text_length;
}

static int atom_name(const struct pg_syntax *s)
{
	return s && s->kind == PG_SYNTAX_ATOM && named(&s->token);
}

static int shape(const struct pg_syntax *s)
{
	if (s->token.kind < 0 || s->token.kind > PG_TOKEN_ERROR) return 0;
	if (s->token.text_length && !s->token.text) return 0;
	if (s->token.kind == PG_TOKEN_IDENT && !named(&s->token)) return 0;
	if (s->binder_marker && s->kind != PG_SYNTAX_LAMBDA) return 0;
	int left = s->left != NULL, right = s->right != NULL;
	int items = 0;
	switch (s->kind) {
	case PG_SYNTAX_ATOM:
		if (left || right) return 0;
		switch (s->token.kind) {
		case PG_TOKEN_IDENT: case PG_TOKEN_TEXT: case PG_TOKEN_INT: case '@': case '#': case '*': break;
		default: return 0;
		}
		break;
	case PG_SYNTAX_QUALIFIED:
		if (!left || !atom_name(s->right)) return 0;
		break;
	case PG_SYNTAX_APPLICATION: case PG_SYNTAX_PI: case PG_SYNTAX_EXPECT:
		if (!left || !right) return 0;
		break;
	case PG_SYNTAX_LAMBDA:
		if (!right || !named(&s->token)) return 0;
		switch (s->binder_marker) {
		case 0: case '@': if (!left) return 0; break;
		case '*': if (left) return 0; break;
		default: return 0;
		}
		break;
	case PG_SYNTAX_BINDER:
		if (!left || right || !named(&s->token)) return 0;
		break;
	case PG_SYNTAX_QUOTE: case PG_SYNTAX_DECLARATION: case PG_SYNTAX_EXIT:
		if (!left || right) return 0;
		break;
	case PG_SYNTAX_GRAPH_REFERENCE: case PG_SYNTAX_IMPORT:
		if (!atom_name(s->left) || right) return 0;
		break;
	case PG_SYNTAX_BLOCK:
		if (!s->item_count) return 0;
		/* fall through */
	case PG_SYNTAX_CONSTRUCTORS: case PG_SYNTAX_DEFINITIONS:
		if (left || right) return 0;
		items = 1;
		break;
	case PG_SYNTAX_ELIMINATION:
		if (!left || right || !s->item_count) return 0;
		items = 1;
		break;
	case PG_SYNTAX_CLAUSE:
		if (!left || !right) return 0;
		items = 1;
		break;
	default: return 0;
	}
	if (s->item_count && (!items || !s->items)) return 0;
	int selector_mode = s->item_count ? s->items[0].operation : 0;
	for (size_t i = 0; i < s->item_count; ++i) {
		const struct pg_syntax_item *item = &s->items[i];
		if (item->annotation && s->kind != PG_SYNTAX_BLOCK) return 0;
		if (s->kind == PG_SYNTAX_CLAUSE) {
			if (!named(&item->name) || item->operation != selector_mode) return 0;
			if (!item->operation) { if (item->expression) return 0; }
			else if (item->operation != PG_TOKEN_ASSIGN || !atom_name(item->expression)) return 0;
			continue;
		}
		if (!item->expression) return 0;
		if (s->kind == PG_SYNTAX_ELIMINATION) {
			if (item->operation || item->name.kind || item->expression->kind != PG_SYNTAX_CLAUSE) return 0;
			continue;
		}
		if (s->kind == PG_SYNTAX_BLOCK && !item->operation) {
			if (item->name.kind || item->annotation) return 0;
			continue;
		}
		if (!named(&item->name)) return 0;
		if (s->kind == PG_SYNTAX_CONSTRUCTORS) {
			if (item->operation != ':') return 0;
		} else if (s->kind == PG_SYNTAX_BLOCK) {
			if (item->operation != PG_TOKEN_ASSIGN) return 0;
		} else switch (item->operation) {
		case PG_TOKEN_ASSIGN: case PG_TOKEN_EXPECT: break;
		case PG_SYNTAX_IMPORT: if (item->expression->kind != PG_SYNTAX_IMPORT) return 0; break;
		default: return 0;
		}
	}
	return 1;
}

int pg_syntax_validate(size_t count, const struct pg_syntax *const *roots)
{
	if (count && !roots) return -1;
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_dag_init(&dag, child, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&dag, roots[i])) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next)
		if (!shape(node->key)) goto done;
	status = 0;
done:
	pg_dag_destroy(&dag);
	return status;
}

static int write_token(FILE *file, const struct pg_token *token)
{
	if (token->kind < 0 || token->kind > PG_TOKEN_ERROR || (token->text_length && !token->text)) return -1;
	const uint64_t words[] = {token->kind, token->offset, token->length, token->line,
		token->column, token->text_length, (uint64_t)token->integer};
	for (size_t i = 0; i < 7; ++i) if (pg_wire_write_u64(file, words[i])) return -1;
	return token->text_length && fwrite(token->text, 1, token->text_length, file) != token->text_length ? -1 : 0;
}

static int write_ref(FILE *file, const struct pg_dag *dag, const struct pg_syntax *syntax)
{
	const struct pg_dag_node *node = pg_dag_find(dag, syntax);
	if (syntax && !node) return -1;
	return pg_wire_write_u64(file, node ? node->id : 0);
}

int pg_syntax_write(FILE *file, size_t count, const struct pg_syntax *const *roots)
{
	if (!file || (count && !roots)) return -1;
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_dag_init(&dag, child, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&dag, roots[i])) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct pg_syntax *s = node->key;
		if (s->kind > PG_SYNTAX_IMPORT || s->binder_marker < 0 || s->binder_marker > 127) goto done;
		if (pg_wire_write_u64(file, s->kind) || write_token(file, &s->token)
			|| pg_wire_write_u64(file, s->binder_marker) || write_ref(file, &dag, s->left)
			|| write_ref(file, &dag, s->right) || pg_wire_write_u64(file, s->item_count)) goto done;
		for (size_t i = 0; i < s->item_count; ++i) {
			const struct pg_syntax_item *item = &s->items[i];
			if (item->operation < 0 || item->operation > PG_TOKEN_ERROR || write_token(file, &item->name) || pg_wire_write_u64(file, item->operation)
				|| write_ref(file, &dag, item->expression) || write_ref(file, &dag, item->annotation)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) if (write_ref(file, &dag, roots[i])) goto done;
	status = 0;
done:
	pg_dag_destroy(&dag);
	return status;
}

static int read_token(FILE *file, struct pg_graph *graph, size_t *remaining, struct pg_token *token)
{
	uint64_t w[7];
	for (size_t i = 0; i < 7; ++i) if (pg_wire_read_u64(file, &w[i])) return -1;
	if (w[0] > PG_TOKEN_ERROR || w[5] > *remaining) return -1;
	for (size_t i = 1; i < 6; ++i) if (w[i] > SIZE_MAX) return -1;
	char *text = pg_alloc(graph, (size_t)w[5]);
	if (!text || fread(text, 1, (size_t)w[5], file) != w[5]) return -1;
	*remaining -= (size_t)w[5];
	/* Decode two's-complement wire integers without an out-of-range cast. */
	int64_t integer = w[6] <= INT64_MAX ? (int64_t)w[6] : -1 - (int64_t)(UINT64_MAX - w[6]);
	*token = (struct pg_token){.kind = (int)w[0], .offset = w[1], .length = w[2],
		.line = w[3], .column = w[4], .text_length = w[5], .text = text, .integer = integer};
	return 0;
}

static int read_ref(FILE *file, size_t count, const struct pg_syntax *const *nodes, const struct pg_syntax **result)
{
	uint64_t id;
	if (pg_wire_read_u64(file, &id) || id > count) return -1;
	*result = id ? nodes[id - 1] : NULL;
	return 0;
}

int pg_syntax_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t *count, const struct pg_syntax *const **roots)
{
	if (!file || !graph || !count || !roots) return -1;
	char header[8];
	uint64_t n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return -1;
	if (n > limit || nr > limit - n || n > SIZE_MAX / sizeof(void *) || nr > SIZE_MAX / sizeof(void *)) return -1;
	size_t remaining = limit - (size_t)n - (size_t)nr;
	const struct pg_syntax **nodes = pg_alloc(graph, (size_t)n * sizeof(*nodes));
	const struct pg_syntax **selected = pg_alloc(graph, (size_t)nr * sizeof(*selected));
	if (!nodes || !selected) return -1;
	for (size_t i = 0; i < n; ++i) {
		uint64_t kind, marker, items;
		struct pg_syntax *s = pg_alloc(graph, sizeof(*s));
		if (!s || pg_wire_read_u64(file, &kind) || kind > PG_SYNTAX_IMPORT) return -1;
		s->kind = (enum pg_syntax_kind)kind;
		if (read_token(file, graph, &remaining, &s->token) || pg_wire_read_u64(file, &marker) || marker > 127) return -1;
		s->binder_marker = (int)marker;
		if (read_ref(file, i, nodes, &s->left) || read_ref(file, i, nodes, &s->right)
			|| pg_wire_read_u64(file, &items)) return -1;
		if (items > remaining || items > SIZE_MAX / sizeof(struct pg_syntax_item)) return -1;
		remaining -= (size_t)items;
		struct pg_syntax_item *array = pg_alloc(graph, (size_t)items * sizeof(*array));
		if (!array) return -1;
		s->items = array; s->item_count = (size_t)items;
		for (size_t j = 0; j < items; ++j) {
			uint64_t operation;
			if (read_token(file, graph, &remaining, &array[j].name) || pg_wire_read_u64(file, &operation)
				|| operation > PG_TOKEN_ERROR) return -1;
			array[j].operation = (int)operation;
			if (read_ref(file, i, nodes, &array[j].expression) || read_ref(file, i, nodes, &array[j].annotation)) return -1;
		}
		nodes[i] = s;
	}
	for (size_t i = 0; i < nr; ++i)
		if (read_ref(file, (size_t)n, nodes, &selected[i]) || !selected[i]) return -1;
	*count = (size_t)nr; *roots = selected;
	return 0;
}
