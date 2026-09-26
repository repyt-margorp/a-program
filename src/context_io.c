#include "context_io.h"
#include "context_payload.h"
#include "wire.h"
#include <stdlib.h>
#include <string.h>

static const char magic[8] = "APGCTX\3";

int pg_contexts_write(FILE *file, size_t count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	const struct pg_graph_codec codec = {.name = name};
	return pg_contexts_write_descriptors(file, count, contexts, term_count, terms, &codec, owner);
}

int pg_contexts_write_descriptors(FILE *file, size_t count,
	const struct pg_context *const *contexts, size_t term_count,
	const struct pg_term *const *terms, const struct pg_graph_codec *codec, void *owner)
{
	if (!file) return -1;
	struct pg_graph storage;
	if (pg_graph_init(&storage)) return -1;
	size_t nm, nr;
	const uint64_t *metadata;
	const struct pg_term *const *roots;
	int status = -1;
	if (pg_contexts_pack(&storage, count, contexts, term_count, terms, &nm, &metadata, &nr, &roots)) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, nm)) goto done;
	for (size_t i = 0; i < nm; ++i) if (pg_wire_write_u64(file, metadata[i])) goto done;
	status = pg_graph_write_descriptors(file, nr, roots, codec, owner);
done:
	pg_graph_destroy(&storage);
	return status;
}

int pg_contexts_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms)
{
	const struct pg_graph_codec codec = {.resolve = resolve};
	return pg_contexts_read_descriptors(file, typing, limit, name_limit, &codec, owner, count, contexts, term_count, terms);
}

int pg_contexts_read_descriptors(FILE *file, struct pg_typing *typing,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms)
{
	if (!file || !typing || !typing->contexts.capacity || !count || !contexts || !term_count || !terms) return -1;
	char header[8];
	uint64_t nm;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &nm) || nm < 2 || nm - 2 > limit || nm > SIZE_MAX / sizeof(uint64_t)) return -1;
	uint64_t *metadata = malloc((size_t)nm * sizeof(*metadata));
	if (!metadata) return -1;
	int status = -1;
	for (size_t i = 0; i < nm; ++i) if (pg_wire_read_u64(file, &metadata[i])) goto done;
	size_t nr;
	const struct pg_term *const *roots;
	if (pg_graph_read_descriptors(file, typing->graph, limit, name_limit, codec, owner, &nr, &roots)) goto done;
	if (metadata[0] > nr / 2 || nr - 2 * (size_t)metadata[0] > limit - (size_t)(nm - 2)) goto done;
	status = pg_contexts_unpack(typing, (size_t)nm, metadata, nr, roots, count, contexts, term_count, terms);
done:
	free(metadata);
	return status;
}
