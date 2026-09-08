#include "graph_io.h"
#include "wire.h"

#include <string.h>

static const unsigned char magic[8] = {'A', 'P', 'G', 'C', 'O', 'R', 'E', 0};

struct record {
	struct pg_index_entry index;
	const void *key;
	size_t id;
	unsigned stage;
	struct record *next, *pending;
};

struct records {
	struct pg_index index;
	struct record *first, *last;
	size_t written;
};

static struct record *record(struct pg_graph *arena, struct records *records, const void *key)
{
	if (!key) return NULL;
	uint64_t hash = (uintptr_t)key;
	for (struct pg_index_entry *p = pg_index_candidates(&records->index, hash); p; p = p->next) {
		struct record *r = (struct record *)p;
		if (r->key == key) return r;
	}
	struct record *r = pg_alloc(arena, sizeof(*r));
	if (!r) return NULL;
	r->key = key;
	if (pg_index_insert(&records->index, &r->index, hash) != 0) return NULL;
	return r;
}

static void finish(struct records *records, struct record *r)
{
	r->id = ++records->written;
	if (records->last) records->last->next = r;
	else records->first = r;
	records->last = r;
}

static int collect(struct pg_graph *arena, struct records *terms, struct records *objects,
	const struct pg_term *root)
{
	struct record *pending = record(arena, terms, root);
	if (!pending) return -1;
	if (pending->id) return 0;
	while (pending) {
		const struct pg_term *term = pending->key;
		const struct pg_term *child = NULL;
		const struct pg_object *object = NULL;
		switch (term->kind) {
		case PG_APPLICATION:
			if (pending->stage == 0) child = term->as.application.function;
			else if (pending->stage == 1) child = term->as.application.argument;
			break;
		case PG_LAMBDA:
			if (!pending->stage) { object = term->as.lambda.binder; child = term->as.lambda.body; }
			break;
		case PG_REFERENCE: object = term->as.reference; break;
		default: return -1;
		}
		if (object) {
			struct record *r = record(arena, objects, object);
			if (!r) return -1;
			if (!r->id) finish(objects, r);
		}
		++pending->stage;
		if (child) {
			struct record *r = record(arena, terms, child);
			if (!r) return -1;
			if (r->id) continue;
			if (r->stage) return -1;
			r->pending = pending;
			pending = r;
		} else {
			finish(terms, pending);
			pending = pending->pending;
		}
	}
	return 0;
}

int pg_graph_write(FILE *file, size_t count, const struct pg_term *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *context)
{
	if (!file || (count && !roots)) return -1;
	struct pg_graph arena = {0};
	struct records terms = {0}, objects = {0};
	int status = -1;
	if (pg_index_init(&terms.index) != 0 || pg_index_init(&objects.index) != 0) goto done;
	for (size_t i = 0; i < count; ++i) if (collect(&arena, &terms, &objects, roots[i]) != 0) goto done;
	if (fwrite(magic, 1, 8, file) != 8) goto done;
	if (pg_wire_write_u64(file, objects.written) || pg_wire_write_u64(file, terms.written) || pg_wire_write_u64(file, count)) goto done;
	for (struct record *r = objects.first; r; r = r->next) {
		const struct pg_object *object = r->key;
		if (object->kind != PG_BINDER && object->kind != PG_SEMANTIC_OBJECT) goto done;
		if (object->kind == PG_BINDER && !object->owner) {
			if (fputc(0, file) == EOF) goto done;
		} else {
			const char *label = name ? name(context, object) : NULL;
			if (!label || !*label) goto done;
			size_t length = strlen(label);
			if (fputc(object->kind == PG_BINDER ? 1 : 2, file) == EOF || pg_wire_write_u64(file, length)) goto done;
			if (fwrite(label, 1, length, file) != length) goto done;
		}
	}
	for (struct record *r = terms.first; r; r = r->next) {
		const struct pg_term *term = r->key;
		struct record *a, *b = NULL;
		switch (term->kind) {
		case PG_LAMBDA:
			a = record(&arena, &objects, term->as.lambda.binder);
			b = record(&arena, &terms, term->as.lambda.body); break;
		case PG_APPLICATION:
			a = record(&arena, &terms, term->as.application.function);
			b = record(&arena, &terms, term->as.application.argument); break;
		case PG_REFERENCE: a = record(&arena, &objects, term->as.reference); break;
		default: goto done;
		}
		if (!a || !a->id || (term->kind != PG_REFERENCE && (!b || !b->id))) goto done;
		if (fputc((int)term->kind, file) == EOF || pg_wire_write_u64(file, a->id)) goto done;
		if (b && pg_wire_write_u64(file, b->id)) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		struct record *r = record(&arena, &terms, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = ferror(file) ? -1 : 0;
done:
	pg_index_destroy(&terms.index);
	pg_index_destroy(&objects.index);
	pg_graph_destroy(&arena);
	return status;
}

int pg_graph_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *context,
	size_t *count, const struct pg_term *const **roots)
{
	if (!file || !graph || !count || !roots) return -1;
	if (!graph->terms.capacity) return -1;
	unsigned char header[8];
	uint64_t no, nt, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &no) || pg_wire_read_u64(file, &nt) || pg_wire_read_u64(file, &nr)) return -1;
	if (no > limit || nt > limit - no || nr > limit - no - nt) return -1;
	if (limit > SIZE_MAX / sizeof(void *)) return -1;
	const struct pg_object **objects = pg_alloc(graph, (size_t)no * sizeof(*objects));
	const struct pg_term **terms = pg_alloc(graph, (size_t)nt * sizeof(*terms));
	const struct pg_term **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!objects || !terms || !result) return -1;
	for (size_t i = 0; i < no; ++i) {
		int tag = fgetc(file);
		if (tag == 0) objects[i] = pg_binder(graph);
		else if (tag == 1 || tag == 2) {
			uint64_t length;
			if (!resolve || pg_wire_read_u64(file, &length) || !length || length > name_limit || length >= SIZE_MAX) return -1;
			char *label = pg_alloc(graph, (size_t)length + 1);
			if (!label || fread(label, 1, (size_t)length, file) != length || memchr(label, 0, (size_t)length)) return -1;
			label[length] = 0;
			objects[i] = resolve(context, label);
			if (!objects[i] || objects[i]->kind != (tag == 1 ? PG_BINDER : PG_SEMANTIC_OBJECT)) return -1;
		} else return -1;
		if (!objects[i]) return -1;
	}
	for (size_t i = 0; i < nt; ++i) {
		int tag = fgetc(file);
		uint64_t a, b;
		if (pg_wire_read_u64(file, &a) || !a) return -1;
		switch (tag) {
		case PG_REFERENCE:
			if (a > no) return -1;
			terms[i] = pg_reference(graph, objects[a - 1]); break;
		case PG_LAMBDA:
			if (a > no || pg_wire_read_u64(file, &b) || !b || b > i) return -1;
			terms[i] = pg_lambda(graph, objects[a - 1], terms[b - 1]); break;
		case PG_APPLICATION:
			if (a > i || pg_wire_read_u64(file, &b) || !b || b > i) return -1;
			terms[i] = pg_application(graph, terms[a - 1], terms[b - 1]); break;
		default: return -1;
		}
		if (!terms[i]) return -1;
	}
	for (size_t i = 0; i < nr; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || !id || id > nt) return -1;
		result[i] = terms[id - 1];
	}
	if (fgetc(file) != EOF || ferror(file)) return -1;
	*count = (size_t)nr;
	*roots = result;
	return 0;
}
