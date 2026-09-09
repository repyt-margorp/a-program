#include "computation_io.h"
#include "computation.h"
#include "eval_internal.h"
#include "dag.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGRCP\1";

struct record {
	struct pg_index_entry index;
	const void *value;
	int phase;
};

struct collection {
	struct pg_graph storage;
	struct pg_index records;
};

static struct record *record(struct collection *collection, const void *value, int phase)
{
	if (!value) return NULL;
	uint64_t hash = (uintptr_t)value;
	for (struct pg_index_entry *p = pg_index_candidates(&collection->records, hash); p; p = p->next) {
		struct record *r = (struct record *)p;
		if (r->value == value) return r->phase == phase ? r : NULL;
	}
	struct record *r = pg_alloc(&collection->storage, sizeof(*r));
	if (!r) return NULL;
	r->value = value;
	r->phase = phase;
	return pg_index_insert(&collection->records, &r->index, hash) ? NULL : r;
}

static int child(void *owner, const void *key, size_t index, const void **output)
{
	const struct record *r = key;
	const void *value;
	int phase;
	if (r->phase) {
		const struct pg_reduction_phase *p = r->value;
		if (index >= 4) return 0;
		phase = index == 0;
		value = index == 0 ? (const void *)p->previous : index == 1 ? (const void *)p->head : p->children[index - 2];
	} else {
		const struct pg_reduction_certificate *c = r->value;
		if (index >= 2) return 0;
		phase = index == 1;
		value = phase ? (const void *)c->phases : c->normality;
	}
	if (!value) return 2;
	*output = record(owner, value, phase);
	return *output ? 1 : -1;
}

static int write_links(FILE *file, struct collection *collection, const struct pg_dag *dag, const struct record *r)
{
	for (size_t i = 0; ; ++i) {
		const void *key = NULL;
		int status = child(collection, r, i, &key);
		if (!status) return 0;
		if (status < 0) return -1;
		const struct pg_dag_node *found = key ? pg_dag_find(dag, key) : NULL;
		if (key && !found) return -1;
		if (pg_wire_write_u64(file, found ? found->id : 0)) return -1;
	}
}

int pg_reduction_records_write(FILE *file, size_t count,
	const struct pg_reduction_certificate *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct collection collection = {0};
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_index_init(&collection.records) || pg_dag_init(&dag, child, &collection)) goto done;
	for (size_t i = 0; i < count; ++i) {
		struct record *root = record(&collection, roots[i], 0);
		if (!root || pg_dag_add(&dag, root)) goto done;
	}
	if (dag.count > SIZE_MAX / (2 * sizeof(const struct pg_term *))) goto done;
	const struct pg_term **terms = pg_alloc(&collection.storage, 2 * dag.count * sizeof(*terms));
	if (!terms || fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count)
		|| pg_wire_write_u64(file, count)) goto done;
	size_t n = 0;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct record *r = node->key;
		if (fputc(r->phase, file) == EOF) goto done;
		if (r->phase) {
			const struct pg_reduction_phase *p = r->value;
			if (!p->head || !p->rebuilt) goto done;
			terms[n++] = p->rebuilt;
		} else {
			const struct pg_reduction_certificate *c = r->value;
			const char *name = pg_computation_policy_name(c->policy);
			if (!name || !c->source || !c->target || c->kind > PG_REDUCTION_NF || c->kind < PG_REDUCTION_WHNF) goto done;
			size_t length = strlen(name);
			if (fputc(c->kind, file) == EOF || pg_wire_write_u64(file, length) || fwrite(name, 1, length, file) != length) goto done;
			terms[n++] = c->source;
			terms[n++] = c->target;
		}
		if (write_links(file, &collection, &dag, r)) goto done;
	}
	for (size_t i = 0; i < count; ++i)
		if (pg_wire_write_u64(file, pg_dag_find(&dag, record(&collection, roots[i], 0))->id)) goto done;
	status = pg_graph_write_descriptors(file, n, terms, codec, owner);
done:
	pg_dag_destroy(&dag);
	pg_index_destroy(&collection.records);
	pg_graph_destroy(&collection.storage);
	return status;
}

static int read_link(FILE *file, size_t available, void *const *records, const unsigned char *kinds, int kind, void **result)
{
	uint64_t id;
	if (pg_wire_read_u64(file, &id) || id > available) return -1;
	if (id && kinds[id - 1] != kind) return -1;
	*result = id ? records[id - 1] : NULL;
	return 0;
}

int pg_reduction_records_read(FILE *file, struct pg_graph *output, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner, const struct pg_reduction_archive **archive)
{
	if (!archive) return -1;
	*archive = NULL;
	if (!file || !output) return -1;
	char header[8];
	uint64_t count, root_count;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)
		|| pg_wire_read_u64(file, &count) || pg_wire_read_u64(file, &root_count)) return -1;
	if (count > limit || root_count > limit || count > SIZE_MAX / sizeof(void *)
		|| root_count > SIZE_MAX / sizeof(const struct pg_reduction_certificate *)) return -1;
	struct pg_graph scratch = {0};
	int status = -1;
	void **records = pg_alloc(&scratch, (size_t)count * sizeof(*records));
	unsigned char *kinds = pg_alloc(&scratch, (size_t)count);
	const struct pg_reduction_certificate **roots = pg_alloc(output, (size_t)root_count * sizeof(*roots));
	if (!records || !kinds || !roots) goto done;
	size_t term_count = 0;
	for (size_t i = 0; i < count; ++i) {
		int kind = fgetc(file);
		if (kind < 0 || kind > 1) goto done;
		kinds[i] = (unsigned char)kind;
		void *links[4];
		if (kind) {
			struct pg_reduction_phase *p = pg_alloc(output, sizeof(*p));
			if (!p) goto done;
			records[i] = p;
			for (size_t j = 0; j < 4; ++j)
				if (read_link(file, i, records, kinds, j == 0, &links[j])) goto done;
			p->previous = links[0]; p->head = links[1];
			p->children[0] = links[2]; p->children[1] = links[3];
			if (!p->head || term_count == SIZE_MAX) goto done;
			++term_count;
		} else {
			struct pg_reduction_certificate *c = pg_alloc(output, sizeof(*c));
			if (!c) goto done;
			records[i] = c;
			int reduction = fgetc(file);
			uint64_t length;
			if (reduction < PG_REDUCTION_WHNF || reduction > PG_REDUCTION_NF || pg_wire_read_u64(file, &length)
				|| !length || length > name_limit || length >= SIZE_MAX) goto done;
			char *name = pg_alloc(&scratch, (size_t)length + 1);
			if (!name || fread(name, 1, (size_t)length, file) != length || memchr(name, 0, (size_t)length)) goto done;
			name[length] = 0;
			c->policy = pg_computation_policy_resolve(name);
			c->kind = (enum pg_reduction_kind)reduction;
			if (!c->policy || read_link(file, i, records, kinds, 0, &links[0])
				|| read_link(file, i, records, kinds, 1, &links[1])) goto done;
			c->normality = links[0]; c->phases = links[1];
			if (term_count > SIZE_MAX - 2) goto done;
			term_count += 2;
		}
	}
	for (size_t i = 0; i < root_count; ++i) {
		void *root;
		if (read_link(file, (size_t)count, records, kinds, 0, &root) || !root) goto done;
		roots[i] = root;
	}
	size_t n;
	const struct pg_term *const *terms;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &n, &terms) || n != term_count) goto done;
	n = 0;
	for (size_t i = 0; i < count; ++i) {
		if (kinds[i]) ((struct pg_reduction_phase *)records[i])->rebuilt = terms[n++];
		else {
			struct pg_reduction_certificate *c = records[i];
			c->source = terms[n++]; c->target = terms[n++];
		}
	}
	struct pg_reduction_archive *result = pg_alloc(output, sizeof(*result));
	if (!result) goto done;
	*result = (struct pg_reduction_archive){(size_t)root_count, roots};
	*archive = result;
	status = 0;
done:
	pg_graph_destroy(&scratch);
	return status;
}

int pg_reduction_archive_write(FILE *file, const struct pg_reduction_archive *archive,
	const struct pg_graph_codec *codec, void *owner)
{
	return archive ? pg_reduction_records_write(file, archive->count, archive->roots, codec, owner) : -1;
}
