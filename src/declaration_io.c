#include "declaration_io.h"
#include "context_payload.h"
#include "classifier.h"
#include "iadt.h"
#include <string.h>

struct payload {
	struct pg_index_entry index;
	const struct pg_object *object;
	size_t term_count, scalar_count;
	const struct pg_term *const *terms;
	const uint64_t *scalars;
};

int pg_declaration_io_init(struct pg_declaration_io *io,
	struct pg_typing *typing)
{
	if (!io || !typing) return -1;
	*io = (struct pg_declaration_io){.typing = typing};
	if (!pg_graph_init(&io->storage) && !pg_index_init(&io->payloads)) return 0;
	pg_declaration_io_destroy(io);
	return -1;
}

void pg_declaration_io_destroy(struct pg_declaration_io *io)
{
	pg_index_destroy(&io->payloads);
	pg_graph_destroy(&io->storage);
}

static const struct payload *payload(struct pg_declaration_io *io, const struct pg_object *object)
{
	uint64_t hash = (uintptr_t)object * UINT64_C(1099511628211);
	for (struct pg_index_entry *e = pg_index_candidates(&io->payloads, hash); e; e = e->next) {
		const struct payload *p = (const struct payload *)e;
		if (p->object == object) return p;
	}
	const struct pg_data_declaration *declaration = pg_data_declaration_view(object);
	if (!declaration) return NULL;
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	if (pg_data_declaration_pack(declaration, &io->storage, &nc, &contexts, &nt, &terms)) return NULL;
	struct payload *p = pg_alloc(&io->storage, sizeof(*p));
	if (!p || pg_contexts_pack(&io->storage, nc, contexts, nt, terms,
		&p->scalar_count, &p->scalars, &p->term_count, &p->terms)) return NULL;
	p->object = object;
	return pg_index_insert(&io->payloads, &p->index, hash) ? NULL : p;
}

static const char *name(void *owner, const struct pg_object *object)
{
	struct pg_declaration_io *io = owner;
	return pg_data_declaration_view(object) ? "data-declaration/v3" : pg_builtin_graph_codec.name(io->typing->graph, object);
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	struct pg_declaration_io *io = owner;
	return pg_builtin_graph_codec.resolve(io->typing->graph, label);
}

static int child(void *owner, struct pg_graph *scratch, const struct pg_object *object,
	size_t index, const struct pg_term **term)
{
	struct pg_declaration_io *io = owner;
	if (!pg_data_declaration_view(object)) return pg_builtin_graph_codec.child(io->typing->graph, scratch, object, index, term);
	const struct payload *p = payload(io, object);
	if (!p) return -1;
	if (index >= p->term_count) return 0;
	*term = p->terms[index];
	return 1;
}

static int scalar(void *owner, const struct pg_object *object, size_t index, uint64_t *value)
{
	struct pg_declaration_io *io = owner;
	if (!pg_data_declaration_view(object)) return pg_builtin_graph_codec.scalar(io->typing->graph, object, index, value);
	const struct payload *p = payload(io, object);
	if (!p) return -1;
	if (index >= p->scalar_count) return 0;
	*value = p->scalars[index];
	return 1;
}

static const struct pg_object *restore(void *owner, struct pg_graph *graph, const char *label,
	size_t count, const struct pg_term *const *terms, size_t scalar_count, const uint64_t *scalars)
{
	struct pg_declaration_io *io = owner;
	if (graph != io->typing->graph) return NULL;
	if (strcmp(label, "data-declaration/v3"))
		return pg_builtin_graph_codec.restore(io->typing->graph, graph, label, count, terms, scalar_count, scalars);
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *roots;
	if (pg_contexts_unpack(io->typing, scalar_count, scalars, count, terms, &nc, &contexts, &nt, &roots)) return NULL;
	return pg_data_declaration_family(pg_data_declaration_unpack(graph, nc, contexts, nt, roots));
}

const struct pg_graph_codec pg_declaration_graph_codec = {
	.name = name, .resolve = resolve, .child = child, .scalar = scalar, .restore = restore
};
