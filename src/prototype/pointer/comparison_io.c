#include "graph_io.h"
#include "graph_internal.h"
#include "dag.h"
#include "wire.h"

#include <stdlib.h>
#include <string.h>

static const char magic[8] = "APGCMP\1";

static int scope_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	if (index) return 0;
	*child = ((const struct binder_pair *)key)->parent;
	return *child ? 1 : 2;
}

static int entry_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	if (index) return 0;
	*child = ((const struct alpha_entry *)key)->next;
	return *child ? 1 : 2;
}

static int reference(FILE *file, const struct pg_dag *dag, const void *key)
{
	if (!key) return pg_wire_write_u64(file, 0);
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? pg_wire_write_u64(file, node->id) : -1;
}

int pg_comparison_write(FILE *file, const struct pg_comparison *work,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || !work->state) return -1;
	const struct pg_comparison_state *state = work->state;
	if (state->normalize || state->status == PG_COMPARISON_ERROR) return -1;
	struct pg_dag entries = {0}, scopes = {0};
	int status = -1;
	if (pg_dag_init(&entries, entry_child, NULL) || pg_dag_init(&scopes, scope_child, NULL)
		|| pg_graph_init(&scopes.storage)) goto done;
	for (size_t i = 0; i < state->seen.capacity; ++i)
		for (const struct pg_index_entry *entry = state->seen.buckets[i]; entry; entry = entry->next)
			if (pg_dag_add(&entries, entry)) goto done;
	for (const struct pg_dag_node *node = entries.first; node; node = node->next) {
		const struct alpha_entry *entry = node->key;
		if (entry->scope && pg_dag_add(&scopes, entry->scope)) goto done;
		if (entry->cursor && pg_dag_add(&scopes, entry->cursor)) goto done;
	}
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *) / 2;
	if (entries.count > maximum || scopes.count > maximum - entries.count) goto done;
	size_t count = 2 * (entries.count + scopes.count);
	const struct pg_term **roots = pg_alloc(&scopes.storage, count * sizeof(*roots));
	if (!roots) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, entries.count)
		|| pg_wire_write_u64(file, scopes.count) || reference(file, &entries, state->pending)
		|| pg_wire_write_u64(file, state->status) || pg_wire_write_u64(file, state->steps)) goto done;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		const struct binder_pair *scope = node->key;
		size_t offset = 2 * (entries.count + node->id - 1);
		roots[offset] = pg_reference(&scopes.storage, scope->left);
		roots[offset + 1] = scope->right ? pg_reference(&scopes.storage, scope->right) : roots[offset];
		if (!roots[offset] || !roots[offset + 1] || reference(file, &scopes, scope->parent)
			|| pg_wire_write_u64(file, scope->right != NULL)) goto done;
	}
	for (const struct pg_dag_node *node = entries.first; node; node = node->next) {
		const struct alpha_entry *entry = node->key;
		roots[2 * (node->id - 1)] = entry->left;
		roots[2 * (node->id - 1) + 1] = entry->right;
		if (reference(file, &scopes, entry->scope) || reference(file, &scopes, entry->cursor)
			|| reference(file, &entries, entry->next) || pg_wire_write_u64(file, entry->stage)) goto done;
	}
	status = pg_graph_write_descriptors(file, count, roots, codec, owner);
done:
	pg_dag_destroy(&entries);
	pg_dag_destroy(&scopes);
	return status;
}

int pg_comparison_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner, struct pg_comparison *work)
{
	if (!work) return -1;
	work->state = NULL;
	if (!file || !graph) return -1;
	char header[8];
	uint64_t n, s, pending, status, steps;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &s)
		|| pg_wire_read_u64(file, &pending) || pg_wire_read_u64(file, &status)
		|| pg_wire_read_u64(file, &steps)) return -1;
	if (n > limit / 2 || s > limit / 2 - n || pending > n || status >= PG_COMPARISON_ERROR) return -1;
	if ((status == PG_COMPARISON_EQUAL) != (pending == 0)) return -1;
	if (n > SIZE_MAX / sizeof(struct alpha_entry) || s > SIZE_MAX / sizeof(struct binder_pair)) return -1;
	if (n > SIZE_MAX / sizeof(uint64_t) / 4 || s > SIZE_MAX / sizeof(uint64_t) / 2) return -1;
	struct pg_comparison candidate = {calloc(1, sizeof(*candidate.state))};
	struct pg_graph scratch = {0};
	int result = -1;
	if (!candidate.state || pg_index_init(&candidate.state->seen)) goto done;
	uint64_t *scope_records = pg_alloc(&scratch, (size_t)s * 2 * sizeof(uint64_t));
	uint64_t *records = pg_alloc(&scratch, (size_t)n * 4 * sizeof(uint64_t));
	if (!scope_records || !records) goto done;
	for (size_t i = 0; i < s; ++i) {
		uint64_t *r = &scope_records[2 * i];
		if (pg_wire_read_u64(file, &r[0]) || pg_wire_read_u64(file, &r[1])) goto done;
		if (r[0] > i || r[1] > 1) goto done;
	}
	for (size_t i = 0; i < n; ++i) {
		uint64_t *r = &records[4 * i];
		for (size_t j = 0; j < 4; ++j) if (pg_wire_read_u64(file, &r[j])) goto done;
		if (r[0] > s || r[1] > s || r[2] > i || r[3] > 2) goto done;
		if (r[1] > r[0] || (r[3] < 2 && r[1] != r[0])) goto done;
	}
	size_t count;
	const struct pg_term *const *roots;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, owner, &count, &roots)
		|| count != 2 * (n + s)) goto done;
	struct binder_pair *scopes = pg_alloc(&candidate.state->arena, (size_t)s * sizeof(*scopes));
	struct alpha_entry *entries = pg_alloc(&candidate.state->arena, (size_t)n * sizeof(*entries));
	unsigned char *queued = pg_alloc(&scratch, (size_t)n);
	if (!scopes || !entries || !queued) goto done;
	for (size_t i = 0; i < s; ++i) {
		const struct pg_term *left = roots[2 * (n + i)], *right = roots[2 * (n + i) + 1];
		uint64_t *r = &scope_records[2 * i];
		if (left->kind != PG_REFERENCE || left->as.reference->kind != PG_BINDER) goto done;
		if (right->kind != PG_REFERENCE || right->as.reference->kind != PG_BINDER) goto done;
		if (!r[1] && right != left) goto done;
		scopes[i] = (struct binder_pair){left->as.reference, r[1] ? right->as.reference : NULL,
			r[0] ? &scopes[r[0] - 1] : NULL};
	}
	for (uint64_t id = pending; id; id = records[4 * (id - 1) + 2]) queued[id - 1] = 1;
	if (status == PG_COMPARISON_DIFFERENT && records[4 * (pending - 1) + 3] != 2) goto done;
	for (size_t i = 0; i < n; ++i) {
		uint64_t *r = &records[4 * i];
		if (!queued[i] && r[3] != 2) goto done;
		struct alpha_entry *entry = &entries[i];
		entry->left = roots[2 * i];
		entry->right = roots[2 * i + 1];
		entry->scope = r[0] ? &scopes[r[0] - 1] : NULL;
		entry->cursor = r[1] ? &scopes[r[1] - 1] : NULL;
		entry->next = r[2] ? &entries[r[2] - 1] : NULL;
		entry->stage = (unsigned)r[3];
		/* Structural comparison's normalized endpoints are determined by stage. */
		entry->normalized[0] = r[3] > 0 ? entry->left : NULL;
		entry->normalized[1] = r[3] > 1 ? entry->right : NULL;
		if (pg_comparison_index(candidate.state, entry)) goto done;
	}
	candidate.state->pending = pending ? &entries[pending - 1] : NULL;
	candidate.state->status = (enum pg_comparison_status)status;
	candidate.state->steps = steps;
	*work = candidate;
	candidate.state = NULL;
	result = 0;
done:
	pg_comparison_destroy(&candidate);
	pg_graph_destroy(&scratch);
	return result;
}
