#include "graph_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>
#include <limits.h>

static const unsigned char magic[8] = {'A', 'P', 'G', 'C', 'O', 'R', 'E', 1};

static int image_roots_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct pg_dag *table = opaque;
	if (pg_wire_write_u64(file, count)) return -1;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_term *term = roots[i];
		if (!term) return -1;
		/* Nested codecs create temporary reference wrappers. Keep a wrapper
		 * here; source Lambda/Application nodes remain borrowed until writing ends. */
		if (term->kind == PG_REFERENCE) term = pg_reference(&table->storage, term->as.reference);
		if (!term || pg_dag_add(table, term)) return -1;
		if (pg_wire_write_u64(file, pg_dag_find(table, term)->id)) return -1;
	}
	return 0;
}

struct image_roots {
	size_t count;
	const struct pg_term *const *terms;
};

static int image_roots_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	(void)name_limit;
	const struct image_roots *table = opaque;
	*count = 0;
	*roots = NULL;
	uint64_t n;
	if (pg_wire_read_u64(file, &n) || n > limit || n > SIZE_MAX / sizeof(const struct pg_term *)) return -1;
	const struct pg_term **result = pg_alloc(graph, (size_t)n * sizeof(*result));
	if (!result) return -1;
	for (size_t i = 0; i < n; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || !id || id > table->count) return -1;
		result[i] = table->terms[id - 1];
	}
	*count = (size_t)n;
	*roots = result;
	return 0;
}


int pg_graph_image_write(FILE *file, const char version[8],
	const struct pg_graph_codec *codec, void *object_owner,
	int (*payload)(FILE *, const struct pg_graph_codec *, void *), void *state)
{
	if (!file || !version || !payload || (codec && codec->roots)) return -1;
	long start = ftell(file);
	if (start < 0 || start > LONG_MAX - 8) return -1;
	struct pg_dag table = {0};
	int status = -1;
	if (pg_dag_init(&table, NULL, NULL) || pg_graph_init(&table.storage)) goto done;
	struct pg_graph_root_codec root_codec = {.write = image_roots_write, .context = &table};
	struct pg_graph_codec nested = codec ? *codec : (struct pg_graph_codec){0};
	nested.roots = &root_codec;
	if (fwrite(version, 1, 8, file) != 8 || pg_wire_write_u64(file, 0)
		|| payload(file, &nested, state)) goto done;
	long position = ftell(file);
	if (position < 0 || table.count > SIZE_MAX / sizeof(const struct pg_term *)) goto done;
	const struct pg_term **all = pg_alloc(&table.storage, table.count * sizeof(*all));
	if (!all) goto done;
	for (const struct pg_dag_node *node = table.first; node; node = node->next) all[node->id - 1] = node->key;
	if (pg_graph_write_descriptors(file, table.count, all, codec, object_owner)) goto done;
	long end = ftell(file);
	if (end < 0 || fseek(file, start + 8, SEEK_SET) || pg_wire_write_u64(file, (uint64_t)position)
		|| fseek(file, end, SEEK_SET)) goto done;
	status = 0;
done:
	pg_dag_destroy(&table);
	return status;
}

int pg_graph_image_read(FILE *file, const char version[8], struct pg_graph *graph,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *object_owner,
	int (*payload)(FILE *, struct pg_graph *, size_t, size_t, const struct pg_graph_codec *, void *), void *state)
{
	if (!file || !version || !graph || !payload || (codec && codec->roots)) return -1;
	char found[8];
	uint64_t position;
	if (fread(found, 1, 8, file) != 8 || memcmp(found, version, 8)
		|| pg_wire_read_u64(file, &position)) return -1;
	long metadata = ftell(file);
	if (metadata < 0 || position < (uint64_t)metadata || position > LONG_MAX) return -1;
	struct image_roots table;
	if (fseek(file, (long)position, SEEK_SET)
		|| pg_graph_read_descriptors(file, graph, limit, name_limit, codec, object_owner, &table.count, &table.terms)) return -1;
	long end = ftell(file);
	if (end < 0 || fseek(file, metadata, SEEK_SET)) return -1;
	struct pg_graph_root_codec root_codec = {.read = image_roots_read, .context = &table};
	struct pg_graph_codec nested = codec ? *codec : (struct pg_graph_codec){0};
	nested.roots = &root_codec;
	if (payload(file, graph, limit, name_limit, &nested, state)
		|| ftell(file) != (long)position || fseek(file, end, SEEK_SET)) return -1;
	return 0;
}


struct transport {
	struct pg_dag *objects;
	const struct pg_graph_codec *codec;
	void *context;
	struct pg_graph *scratch;
};

static int dependency(void *owner, const void *key, size_t index, const void **child)
{
	struct pg_dag *objects = owner;
	const struct pg_term *term = key;
	switch (term->kind) {
	case PG_APPLICATION:
		if (index == 2) return 0;
		*child = index ? term->as.application.argument : term->as.application.function;
		return 1;
	case PG_LAMBDA:
		if (index) return 0;
		if (pg_dag_add(objects, term->as.lambda.binder)) return -1;
		*child = term->as.lambda.body;
		return 1;
	case PG_REFERENCE:
		return pg_dag_add(objects, term->as.reference);
	default: return -1;
	}
}

static int transport_dependency(void *owner, const void *key, size_t index, const void **child)
{
	struct transport *transport = owner;
	const struct pg_term *term = key;
	if (term->kind == PG_LAMBDA && term->as.lambda.binder->owner && transport->codec && transport->codec->child) {
		if (index == 2) return 0;
		*child = index ? term->as.lambda.body : pg_reference(transport->scratch, term->as.lambda.binder);
		return *child ? 1 : -1;
	}
	if (term->kind != PG_REFERENCE || !transport->codec || !transport->codec->child)
		return dependency(transport->objects, key, index, child);
	if (pg_dag_add(transport->objects, term->as.reference)) return -1;
	const struct pg_term *payload = NULL;
	int status = transport->codec->child(transport->context, transport->scratch, term->as.reference, index, &payload);
	if (status == -2 && !index) return 0;
	if (status == 1) *child = payload;
	return status;
}

int pg_graph_dependencies_init(struct pg_dag *terms, struct pg_dag *objects,
	const struct pg_graph_codec *codec, void *context)
{
	if (!terms || !objects || objects->child) return -1;
	if (pg_dag_init(terms, transport_dependency, NULL) || pg_graph_init(&terms->storage)) return -1;
	struct transport *transport = pg_alloc(&terms->storage, sizeof(*transport));
	if (!transport) return -1;
	*transport = (struct transport){objects, codec, context, &terms->storage};
	terms->context = transport;
	return 0;
}

int pg_graph_collect_objects(struct pg_dag *objects, size_t count,
	const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *context)
{
	if (!objects || objects->child || (count && !roots)) return -1;
	struct pg_dag terms = {0};
	int status = -1;
	if (pg_graph_dependencies_init(&terms, objects, codec, context)) goto done;
	for (size_t i = 0; i < count; ++i)
		if (roots[i] && pg_dag_add(&terms, roots[i])) goto done;
	status = 0;
done:
	pg_dag_destroy(&terms);
	return status;
}

int pg_graph_print(FILE *file, const struct pg_term *root)
{
	if (!file || !root) return -1;
	struct pg_dag terms = {0}, objects = {0};
	int status = -1;
	if (pg_dag_init(&objects, NULL, NULL) || pg_dag_init(&terms, dependency, &objects)) goto done;
	if (pg_dag_add(&terms, root)) goto done;
	for (const struct pg_dag_node *r = objects.first; r; r = r->next) {
		const struct pg_object *object = r->key;
		if (fprintf(file, "o%zu := %s\n", r->id,
			object->kind == PG_BINDER ? "binder" : "semantic-object") < 0) goto done;
	}
	for (const struct pg_dag_node *r = terms.first; r; r = r->next) {
		const struct pg_term *term = r->key;
		int written;
		switch (term->kind) {
		case PG_LAMBDA:
			written = fprintf(file, "n%zu := LAMBDA(o%zu, n%zu)\n", r->id,
				pg_dag_find(&objects, term->as.lambda.binder)->id,
				pg_dag_find(&terms, term->as.lambda.body)->id); break;
		case PG_APPLICATION:
			written = fprintf(file, "n%zu := APP(n%zu, n%zu)\n", r->id,
				pg_dag_find(&terms, term->as.application.function)->id,
				pg_dag_find(&terms, term->as.application.argument)->id); break;
		case PG_REFERENCE:
			written = fprintf(file, "n%zu := REF(o%zu)\n", r->id,
				pg_dag_find(&objects, term->as.reference)->id); break;
		default: goto done;
		}
		if (written < 0) goto done;
	}
	if (fprintf(file, "root := n%zu\n", pg_dag_find(&terms, root)->id) < 0) goto done;
	status = ferror(file) ? -1 : 0;
done:
	pg_dag_destroy(&terms);
	pg_dag_destroy(&objects);
	return status;
}

int pg_graph_write(FILE *file, size_t count, const struct pg_term *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *context)
{
	const struct pg_graph_codec codec = {.name = name};
	return pg_graph_write_descriptors(file, count, roots, &codec, context);
}

int pg_graph_write_descriptors(FILE *file, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *context)
{
	if (!file || (count && !roots)) return -1;
	if (codec && codec->roots) {
		if (!codec->roots->write) return -1;
		return codec->roots->write(file, count, roots, codec->roots->context);
	}
	struct pg_dag terms = {0}, objects = {0};
	int status = -1;
	if (pg_dag_init(&objects, NULL, NULL) || pg_graph_dependencies_init(&terms, &objects, codec, context)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&terms, roots[i])) goto done;
	if (fwrite(magic, 1, 8, file) != 8) goto done;
	if (pg_wire_write_u64(file, objects.count) || pg_wire_write_u64(file, terms.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *r = objects.first; r; r = r->next) {
		const struct pg_object *object = r->key;
		if (object->kind != PG_BINDER && object->kind != PG_SEMANTIC_OBJECT) goto done;
		if (object->kind == PG_BINDER && !object->owner) {
			if (fputc(0, file) == EOF) goto done;
		} else {
			const char *label = codec && codec->name ? codec->name(context, object) : NULL;
			if (!label || !*label) goto done;
			size_t length = strlen(label);
			const struct pg_term *payload = NULL;
			int status = codec->child ? codec->child(context, &terms.storage, object, 0, &payload) : -2;
			if (status != -2 && status != 0 && status != 1) goto done;
			int tag = object->kind == PG_BINDER ? 1 : 2;
			if (status != -2) tag += 2;
			if (fputc(tag, file) == EOF || pg_wire_write_u64(file, length)) goto done;
			if (fwrite(label, 1, length, file) != length) goto done;
			if (status != -2) {
				/* Zero-terminated IDs avoid a separate descriptor-counting walk. */
				for (size_t i = 0; status == 1; ++i) {
					const struct pg_dag_node *node = pg_dag_find(&terms, payload);
					if (!node || pg_wire_write_u64(file, node->id) || i == SIZE_MAX) goto done;
					status = codec->child(context, &terms.storage, object, i + 1, &payload);
				}
				if (status || pg_wire_write_u64(file, 0)) goto done;
				for (size_t i = 0;; ++i) {
					uint64_t value;
					int item = codec->scalar ? codec->scalar(context, object, i, &value) : 0;
					if (!item) { if (fputc(0, file) == EOF) goto done; break; }
					if (item != 1 || i == SIZE_MAX || fputc(1, file) == EOF || pg_wire_write_u64(file, value)) goto done;
				}
			}
		}
	}
	for (const struct pg_dag_node *r = terms.first; r; r = r->next) {
		const struct pg_term *term = r->key;
		const struct pg_dag_node *a, *b = NULL;
		switch (term->kind) {
		case PG_LAMBDA:
			a = pg_dag_find(&objects, term->as.lambda.binder);
			b = pg_dag_find(&terms, term->as.lambda.body); break;
		case PG_APPLICATION:
			a = pg_dag_find(&terms, term->as.application.function);
			b = pg_dag_find(&terms, term->as.application.argument); break;
		case PG_REFERENCE: a = pg_dag_find(&objects, term->as.reference); break;
		default: goto done;
		}
		if (!a || !a->id || (term->kind != PG_REFERENCE && (!b || !b->id))) goto done;
		if (fputc((int)term->kind, file) == EOF || pg_wire_write_u64(file, a->id)) goto done;
		if (b && pg_wire_write_u64(file, b->id)) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&terms, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = ferror(file) ? -1 : 0;
done:
	pg_dag_destroy(&terms);
	pg_dag_destroy(&objects);
	return status;
}

struct payload_edge {
	uint64_t value;
	struct payload_edge *next;
};
struct object_input {
	const struct pg_object *object;
	char *name;
	enum pg_object_kind kind;
	size_t count;
	struct payload_edge *first;
	size_t scalar_count;
	struct payload_edge *scalars;
};

static int read_objects(FILE *file, struct pg_graph *graph, size_t count, size_t name_limit,
	const struct pg_graph_codec *codec, void *context, size_t term_count,
	size_t *available, struct object_input *objects, struct pg_dag *seen)
{
	for (size_t i = 0; i < count; ++i) {
		int tag = fgetc(file);
		struct object_input *record = &objects[i];
		if (tag == 0) record->object = pg_binder(graph);
		else if (tag >= 1 && tag <= 4) {
			uint64_t length;
			if (!codec || pg_wire_read_u64(file, &length) || !length || length > name_limit || length >= SIZE_MAX) return -1;
			char *label = pg_alloc(graph, (size_t)length + 1);
			if (!label || fread(label, 1, (size_t)length, file) != length || memchr(label, 0, (size_t)length)) return -1;
			label[length] = 0;
			record->kind = tag % 2 ? PG_BINDER : PG_SEMANTIC_OBJECT;
			if (tag >= 3) {
				if (!codec->restore) return -1;
				record->name = label;
				struct payload_edge **next = &record->first;
				for (;;) {
					uint64_t id;
					if (pg_wire_read_u64(file, &id) || id > term_count) return -1;
					if (!id) break;
					if (!*available) return -1;
					--*available;
					*next = pg_alloc(&seen->storage, sizeof(**next));
					if (!*next) return -1;
					(*next)->value = id;
					next = &(*next)->next;
					++record->count;
				}
				next = &record->scalars;
				for (;;) {
					int item = fgetc(file);
					if (!item) break;
					if (item != 1 || !*available) return -1;
					--*available;
					*next = pg_alloc(&seen->storage, sizeof(**next));
					if (!*next || pg_wire_read_u64(file, &(*next)->value)) return -1;
					next = &(*next)->next;
					++record->scalar_count;
				}
				continue;
			}
			if (!codec->resolve) return -1;
			record->object = codec->resolve(context, label);
			if (!record->object || record->object->kind != record->kind) return -1;
		} else return -1;
		/* Distinct object records must remain distinct after relocation. */
		if (!record->object || pg_dag_find(seen, record->object) || pg_dag_add(seen, record->object)) return -1;
	}
	return 0;
}

static const struct pg_object *restore_object(struct pg_graph *graph, struct object_input *input,
	const struct pg_graph_codec *codec, void *context, size_t count,
	const struct pg_term *const *terms, struct pg_dag *seen)
{
	if (input->object) return input->object;
	if (!input->name || !codec || !codec->restore) return NULL;
	const struct pg_term **payload = pg_alloc(&seen->storage, input->count * sizeof(*payload));
	if (!payload) return NULL;
	size_t i = 0;
	for (const struct payload_edge *edge = input->first; edge; edge = edge->next) {
		if (edge->value > count) return NULL;
		payload[i++] = terms[edge->value - 1];
	}
	uint64_t *scalars = pg_alloc(&seen->storage, input->scalar_count * sizeof(*scalars));
	if (!scalars) return NULL;
	i = 0;
	for (const struct payload_edge *item = input->scalars; item; item = item->next) scalars[i++] = item->value;
	const struct pg_object *object = codec->restore(context, graph, input->name, input->count, payload, input->scalar_count, scalars);
	if (!object || object->kind != input->kind || pg_dag_find(seen, object) || pg_dag_add(seen, object)) return NULL;
	input->object = object;
	return object;
}

int pg_graph_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *context,
	size_t *count, const struct pg_term *const **roots)
{
	const struct pg_graph_codec codec = {.resolve = resolve};
	return pg_graph_read_descriptors(file, graph, limit, name_limit, &codec, context, count, roots);
}

int pg_graph_read_descriptors(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context,
	size_t *count, const struct pg_term *const **roots)
{
	if (!file || !graph || !count || !roots) return -1;
	if (!graph->terms.capacity) return -1;
	if (codec && codec->roots) {
		if (!codec->roots->read) return -1;
		return codec->roots->read(file, graph, limit, name_limit, count, roots, codec->roots->context);
	}
	unsigned char header[8];
	uint64_t no, nt, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &no) || pg_wire_read_u64(file, &nt) || pg_wire_read_u64(file, &nr)) return -1;
	if (no > limit || nt > limit - no || nr > limit - no - nt) return -1;
	if (limit > SIZE_MAX / sizeof(struct object_input)) return -1;
	struct pg_dag seen = {0};
	int status = -1;
	if (pg_dag_init(&seen, NULL, NULL)) goto done;
	/* Relocation and descriptor assembly are borrowed only during this read. */
	struct object_input *objects = pg_alloc(&seen.storage, (size_t)no * sizeof(*objects));
	const struct pg_term **terms = pg_alloc(&seen.storage, (size_t)nt * sizeof(*terms));
	const struct pg_term **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!objects || !terms || !result) goto done;
	size_t available = limit - (size_t)no - (size_t)nt - (size_t)nr;
	if (read_objects(file, graph, (size_t)no, name_limit, codec, context, (size_t)nt, &available, objects, &seen)) goto done;
	for (size_t i = 0; i < nt; ++i) {
		int tag = fgetc(file);
		uint64_t a, b;
		if (pg_wire_read_u64(file, &a) || !a) goto done;
		const struct pg_object *object;
		switch (tag) {
		case PG_REFERENCE:
			if (a > no) goto done;
			object = restore_object(graph, &objects[a - 1], codec, context, i, terms, &seen);
			terms[i] = pg_reference(graph, object); break;
		case PG_LAMBDA:
			if (a > no || pg_wire_read_u64(file, &b) || !b || b > i) goto done;
			object = restore_object(graph, &objects[a - 1], codec, context, i, terms, &seen);
			terms[i] = pg_lambda(graph, object, terms[b - 1]); break;
		case PG_APPLICATION:
			if (a > i || pg_wire_read_u64(file, &b) || !b || b > i) goto done;
			terms[i] = pg_application(graph, terms[a - 1], terms[b - 1]); break;
		default: goto done;
		}
		if (!terms[i]) goto done;
	}
	for (size_t i = 0; i < no; ++i) if (!objects[i].object) goto done;
	for (size_t i = 0; i < nr; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || !id || id > nt) goto done;
		result[i] = terms[id - 1];
	}
	if (fgetc(file) != EOF || ferror(file)) goto done;
	*count = (size_t)nr;
	*roots = result;
	status = 0;
done:
	pg_dag_destroy(&seen);
	return status;
}
