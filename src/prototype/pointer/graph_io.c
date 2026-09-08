#include "graph_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const unsigned char magic[8] = {'A', 'P', 'G', 'C', 'O', 'R', 'E', 0};

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

int pg_graph_write(FILE *file, size_t count, const struct pg_term *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *context)
{
	if (!file || (count && !roots)) return -1;
	struct pg_dag terms = {0}, objects = {0};
	int status = -1;
	if (pg_dag_init(&objects, NULL, NULL) || pg_dag_init(&terms, dependency, &objects)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&terms, roots[i])) goto done;
	if (fwrite(magic, 1, 8, file) != 8) goto done;
	if (pg_wire_write_u64(file, objects.count) || pg_wire_write_u64(file, terms.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *r = objects.first; r; r = r->next) {
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

static int read_objects(FILE *file, struct pg_graph *graph, size_t count, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *context,
	const struct pg_object **objects)
{
	struct pg_dag seen = {0};
	int status = -1;
	if (pg_dag_init(&seen, NULL, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) {
		int tag = fgetc(file);
		if (tag == 0) objects[i] = pg_binder(graph);
		else if (tag == 1 || tag == 2) {
			uint64_t length;
			if (!resolve || pg_wire_read_u64(file, &length) || !length || length > name_limit || length >= SIZE_MAX) goto done;
			char *label = pg_alloc(graph, (size_t)length + 1);
			if (!label || fread(label, 1, (size_t)length, file) != length || memchr(label, 0, (size_t)length)) goto done;
			label[length] = 0;
			objects[i] = resolve(context, label);
			if (!objects[i] || objects[i]->kind != (tag == 1 ? PG_BINDER : PG_SEMANTIC_OBJECT)) goto done;
		} else goto done;
		/* Distinct object records must remain distinct after relocation. */
		if (!objects[i] || pg_dag_add(&seen, objects[i]) || seen.count != i + 1) goto done;
	}
	status = 0;
done:
	pg_dag_destroy(&seen);
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
	if (read_objects(file, graph, (size_t)no, name_limit, resolve, context, objects)) return -1;
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
