#include "computation_io.h"
#include "computation.h"
#include "eval_internal.h"
#include "dag.h"
#include "wire.h"

#include <string.h>
#include <stdlib.h>

static const char magic[8] = "APGRCP\2";

const struct pg_reduction_archive *pg_reduction_archive_snapshot(struct pg_graph *output,
	const struct pg_whnf_work *work, const struct pg_reduction_archive *previous)
{
	if (!output || !work) return NULL;
	struct pg_dag receipts = {0}, phases = {0};
	struct pg_reduction_archive *archive = NULL;
	if (pg_dag_init(&receipts, NULL, NULL) || pg_dag_init(&phases, NULL, NULL)) goto done;
	if (previous) {
		for (size_t i = 0; i < previous->count; ++i) {
			const struct pg_reduction_certificate *raw = previous->roots[i];
			const struct pg_reduction_certificate *local = pg_reduction_find(work, raw->policy, raw->source, raw->kind);
			if (pg_dag_add(&receipts, local ? local : raw)) goto done;
		}
		for (size_t i = 0; i < previous->phase_count; ++i)
			if (pg_dag_add(&phases, previous->phases[i])) goto done;
	}
	for (size_t i = 0; i < work->jobs.capacity; ++i) {
		for (const struct pg_index_entry *entry = work->jobs.buckets[i]; entry; entry = entry->next) {
			const struct pg_whnf_job *job = (const void *)entry;
			const struct pg_reduction_certificate *receipt = pg_whnf_certificate(job);
			if (receipt && pg_dag_add(&receipts, receipt)) goto done;
		}
	}
	for (size_t i = 0; i < work->normal_forms.capacity; ++i) {
		for (const struct pg_index_entry *entry = work->normal_forms.buckets[i]; entry; entry = entry->next) {
			const struct pg_nf_job *job = (const void *)entry;
			if (job->prefix && pg_dag_add(&receipts, job->prefix)) goto done;
			const struct pg_reduction_certificate *receipt = pg_nf_certificate(job);
			if (receipt) {
				if (pg_dag_add(&receipts, receipt)) goto done;
			} else if (job->phases && pg_dag_add(&phases, job->phases)) goto done;
		}
	}
	if (receipts.count > SIZE_MAX / sizeof(void *) || phases.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_reduction_certificate **roots = pg_alloc(output, receipts.count * sizeof(*roots));
	const struct pg_reduction_phase **pending = pg_alloc(output, phases.count * sizeof(*pending));
	if (!roots || !pending) goto done;
	for (const struct pg_dag_node *node = receipts.first; node; node = node->next) roots[node->id - 1] = node->key;
	for (const struct pg_dag_node *node = phases.first; node; node = node->next) pending[node->id - 1] = node->key;
	archive = pg_alloc(output, sizeof(*archive));
	if (archive) *archive = (struct pg_reduction_archive){receipts.count, roots, phases.count, pending};
done:
	pg_dag_destroy(&receipts);
	pg_dag_destroy(&phases);
	return archive;
}

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

static int collect(struct collection *collection, struct pg_dag *dag, size_t count,
	const struct pg_reduction_certificate *const *roots, size_t phase_count,
	const struct pg_reduction_phase *const *phases)
{
	if (pg_index_init(&collection->records) || pg_dag_init(dag, child, collection)) return -1;
	for (size_t i = 0; i < count; ++i) {
		struct record *root = record(collection, roots[i], 0);
		if (!root || pg_dag_add(dag, root)) return -1;
	}
	for (size_t i = 0; i < phase_count; ++i) {
		struct record *root = record(collection, phases[i], 1);
		if (!root || pg_dag_add(dag, root)) return -1;
	}
	return 0;
}

int pg_reduction_records_write(FILE *file, size_t count,
	const struct pg_reduction_certificate *const *roots, size_t phase_count,
	const struct pg_reduction_phase *const *phases, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (count && !roots) || (phase_count && !phases)) return -1;
	struct collection collection = {0};
	struct pg_dag dag = {0};
	int status = -1;
	if (collect(&collection, &dag, count, roots, phase_count, phases)) goto done;
	if (dag.count > SIZE_MAX / (2 * sizeof(const struct pg_term *))) goto done;
	const struct pg_term **terms = pg_alloc(&collection.storage, 2 * dag.count * sizeof(*terms));
	if (!terms || fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count)
		|| pg_wire_write_u64(file, count) || pg_wire_write_u64(file, phase_count)) goto done;
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
			if (!name || !c->source || !c->target || c->kind > PG_REDUCTION_PREFIX || c->kind < PG_REDUCTION_WHNF) goto done;
			size_t length = strlen(name);
			if (fputc(c->kind, file) == EOF || pg_wire_write_u64(file, length) || fwrite(name, 1, length, file) != length) goto done;
			terms[n++] = c->source;
			terms[n++] = c->target;
		}
		if (write_links(file, &collection, &dag, r)) goto done;
	}
	for (size_t i = 0; i < count; ++i)
		if (pg_wire_write_u64(file, pg_dag_find(&dag, record(&collection, roots[i], 0))->id)) goto done;
	for (size_t i = 0; i < phase_count; ++i)
		if (pg_wire_write_u64(file, pg_dag_find(&dag, record(&collection, phases[i], 1))->id)) goto done;
	status = pg_graph_write_descriptors(file, n, terms, codec, owner);
done:
	pg_dag_destroy(&dag);
	pg_index_destroy(&collection.records);
	pg_graph_destroy(&collection.storage);
	return status;
}

static int read_link(FILE *file, size_t available, void *const *records, const unsigned char *kinds,
	int kind, void **result, size_t *ordinal)
{
	uint64_t id;
	if (pg_wire_read_u64(file, &id) || id > available) return -1;
	if (id && kinds[id - 1] != kind) return -1;
	*result = id ? records[id - 1] : NULL;
	if (ordinal) *ordinal = (size_t)id;
	return 0;
}

static int receipt_shape(const struct pg_reduction_certificate *c, const struct pg_reduction_certificate *first)
{
	if (c->normality) {
		const struct pg_reduction_certificate *basis = c->normality;
		enum pg_reduction_kind basis_kind = c->kind == PG_REDUCTION_PREFIX ? PG_REDUCTION_NF : c->kind;
		return !c->phases && c->source == c->target && basis->target == c->source
			&& basis->kind == basis_kind && basis->policy == c->policy;
	}
	if (c->kind == PG_REDUCTION_WHNF) return !c->phases;
	const struct pg_reduction_phase *p = c->phases;
	if (c->kind == PG_REDUCTION_PREFIX) {
		if (!p || p->previous || !first || first->source != c->source) return 0;
		if (!p->children[0] && p->rebuilt->kind != PG_REFERENCE) return 0;
		return p->rebuilt == c->target && p->head->policy == c->policy;
	}
	if (!p || !first || p->children[0] || p->children[1]) return 0;
	return first->source == c->source && p->rebuilt == c->target
		&& p->head->policy == c->policy && pg_reduction_nf_terminal(p->previous, p->head);
}

int pg_reduction_records_read(FILE *file, struct pg_graph *output, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner, const struct pg_reduction_archive **archive)
{
	if (!archive) return -1;
	*archive = NULL;
	if (!file || !output) return -1;
	char header[8];
	uint64_t count, root_count, phase_count;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)
		|| pg_wire_read_u64(file, &count) || pg_wire_read_u64(file, &root_count)
		|| pg_wire_read_u64(file, &phase_count)) return -1;
	if (count > limit || root_count > limit || count > SIZE_MAX / sizeof(void *)
		|| root_count > SIZE_MAX / sizeof(const struct pg_reduction_certificate *)) return -1;
	if (phase_count > limit || phase_count > SIZE_MAX / sizeof(const struct pg_reduction_phase *)) return -1;
	struct pg_graph scratch = {0};
	int status = -1;
	void **records = pg_alloc(&scratch, (size_t)count * sizeof(*records));
	/* Transient chain origins avoid rescanning a shared predecessor chain. */
	const struct pg_reduction_certificate **first = pg_alloc(&scratch, (size_t)count * sizeof(*first));
	unsigned char *kinds = pg_alloc(&scratch, (size_t)count);
	const struct pg_reduction_certificate **roots = pg_alloc(output, (size_t)root_count * sizeof(*roots));
	const struct pg_reduction_phase **phases = pg_alloc(output, (size_t)phase_count * sizeof(*phases));
	if (!records || !first || !kinds || !roots || !phases) goto done;
	size_t term_count = 0;
	for (size_t i = 0; i < count; ++i) {
		int kind = fgetc(file);
		if (kind < 0 || kind > 1) goto done;
		kinds[i] = (unsigned char)kind;
		void *links[4];
		size_t origin = 0;
		if (kind) {
			struct pg_reduction_phase *p = pg_alloc(output, sizeof(*p));
			if (!p) goto done;
			records[i] = p;
			for (size_t j = 0; j < 4; ++j)
				if (read_link(file, i, records, kinds, j == 0, &links[j], j == 0 ? &origin : NULL)) goto done;
			p->previous = links[0]; p->head = links[1];
			p->children[0] = links[2]; p->children[1] = links[3];
			if (!p->head || term_count == SIZE_MAX) goto done;
			first[i] = origin ? first[origin - 1] : p->head;
			++term_count;
		} else {
			struct pg_reduction_certificate *c = pg_alloc(output, sizeof(*c));
			if (!c) goto done;
			records[i] = c;
			int reduction = fgetc(file);
			uint64_t length;
			if (reduction < PG_REDUCTION_WHNF || reduction > PG_REDUCTION_PREFIX || pg_wire_read_u64(file, &length)
				|| !length || length > name_limit || length >= SIZE_MAX) goto done;
			char *name = pg_alloc(&scratch, (size_t)length + 1);
			if (!name || fread(name, 1, (size_t)length, file) != length || memchr(name, 0, (size_t)length)) goto done;
			name[length] = 0;
			c->policy = pg_computation_policy_resolve(name);
			c->kind = (enum pg_reduction_kind)reduction;
			if (!c->policy || read_link(file, i, records, kinds, 0, &links[0], NULL)
				|| read_link(file, i, records, kinds, 1, &links[1], &origin)) goto done;
			c->normality = links[0]; c->phases = links[1];
			first[i] = origin ? first[origin - 1] : NULL;
			if (term_count > SIZE_MAX - 2) goto done;
			term_count += 2;
		}
	}
	for (size_t i = 0; i < root_count; ++i) {
		void *root;
		if (read_link(file, (size_t)count, records, kinds, 0, &root, NULL) || !root) goto done;
		roots[i] = root;
	}
	for (size_t i = 0; i < phase_count; ++i) {
		void *root;
		if (read_link(file, (size_t)count, records, kinds, 1, &root, NULL) || !root) goto done;
		phases[i] = root;
	}
	size_t n;
	const struct pg_term *const *terms;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &n, &terms) || n != term_count) goto done;
	n = 0;
	/* Dependency order supplies each phase's endpoints before reconstruction.
	 * This checks local congruence, not the truth of the WHNF leaf claims. */
	for (size_t i = 0; i < count; ++i) {
		if (kinds[i]) {
			struct pg_reduction_phase *p = records[i];
			p->rebuilt = terms[n++];
			if (pg_reduction_phase_rebuild(output, p->previous, p->head, p->children[0], p->children[1]) != p->rebuilt) goto done;
		} else {
			struct pg_reduction_certificate *c = records[i];
			c->source = terms[n++]; c->target = terms[n++];
			if (!receipt_shape(c, first[i])) goto done;
		}
	}
	struct pg_reduction_archive *result = pg_alloc(output, sizeof(*result));
	if (!result) goto done;
	*result = (struct pg_reduction_archive){(size_t)root_count, roots, (size_t)phase_count, phases};
	*archive = result;
	status = 0;
done:
	pg_graph_destroy(&scratch);
	return status;
}

int pg_reduction_archive_write(FILE *file, const struct pg_reduction_archive *archive,
	const struct pg_graph_codec *codec, void *owner)
{
	return archive ? pg_reduction_records_write(file, archive->count, archive->roots,
		archive->phase_count, archive->phases, codec, owner) : -1;
}

struct pg_reduction_check_state {
	struct collection collection;
	struct pg_dag dag;
	const struct pg_dag_node *cursor;
	const struct pg_reduction_archive *archive;
	struct pg_whnf_work *work;
	struct pg_whnf_job *job;
	struct pg_comparison comparison;
	enum pg_comparison_status status;
	uint64_t steps;
};

int pg_reduction_archive_collect(struct pg_dag *terms, const struct pg_reduction_archive *archive)
{
	if (!terms || !archive) return -1;
	struct collection collection = {0};
	struct pg_dag dag = {0};
	int status = -1;
	if (collect(&collection, &dag, archive->count, archive->roots, archive->phase_count, archive->phases)) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct record *r = node->key;
		if (r->phase) {
			const struct pg_reduction_phase *p = r->value;
			if (pg_dag_add(terms, p->rebuilt)) goto done;
		} else {
			const struct pg_reduction_certificate *c = r->value;
			if (pg_dag_add(terms, c->source) || pg_dag_add(terms, c->target)) goto done;
		}
	}
	status = 0;
done:
	pg_dag_destroy(&dag);
	pg_index_destroy(&collection.records);
	pg_graph_destroy(&collection.storage);
	return status;
}

int pg_reduction_check_init(struct pg_reduction_check *check, struct pg_whnf_work *work,
	const struct pg_reduction_archive *archive)
{
	check->state = NULL;
	if (!work || !archive) return -1;
	struct pg_reduction_check_state *state = calloc(1, sizeof(*state));
	if (!state) return -1;
	check->state = state;
	state->archive = archive;
	state->work = work;
	if (collect(&state->collection, &state->dag, archive->count, archive->roots, archive->phase_count, archive->phases)) {
		pg_reduction_check_destroy(check);
		return -1;
	}
	state->cursor = state->dag.first;
	state->status = state->cursor ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
	return 0;
}

static enum pg_comparison_status check_step(struct pg_reduction_check_state *state)
{
	const struct record *r = state->cursor->key;
	const struct pg_reduction_certificate *c = r->phase ? NULL : r->value;
	if (c && c->kind == PG_REDUCTION_WHNF && !c->normality) {
		if (!state->job) state->job = pg_whnf_request(state->work, c->policy, c->source);
		if (!state->job) return PG_COMPARISON_ERROR;
		if (pg_whnf_status(state->job) == PG_EVAL_PENDING)
			return pg_whnf_advance(state->job, 1) == PG_EVAL_ERROR ? PG_COMPARISON_ERROR : PG_COMPARISON_PENDING;
		const struct pg_term *result = pg_whnf_result(state->job);
		if (!result) return PG_COMPARISON_ERROR;
		if (!state->comparison.state && pg_comparison_init(&state->comparison, result, c->target, NULL, NULL))
			return PG_COMPARISON_ERROR;
		enum pg_comparison_status status = pg_comparison_advance(&state->comparison, 1);
		if (status != PG_COMPARISON_EQUAL) return status;
		pg_comparison_destroy(&state->comparison);
		state->job = NULL;
	}
	state->cursor = state->cursor->next;
	return state->cursor ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
}

enum pg_comparison_status pg_reduction_check_advance(struct pg_reduction_check *check, uint64_t budget)
{
	struct pg_reduction_check_state *state = check->state;
	if (!state) return PG_COMPARISON_ERROR;
	while (state->status == PG_COMPARISON_PENDING && budget) {
		--budget;
		++state->steps;
		state->status = check_step(state);
	}
	return state->status;
}

uint64_t pg_reduction_check_steps(const struct pg_reduction_check *check)
{
	return check->state ? check->state->steps : 0;
}

const struct pg_reduction_certificate *pg_reduction_check_certificate(const struct pg_reduction_check *check, size_t root)
{
	const struct pg_reduction_check_state *state = check->state;
	if (!state || state->status != PG_COMPARISON_EQUAL || root >= state->archive->count) return NULL;
	return state->archive->roots[root];
}

void pg_reduction_check_destroy(struct pg_reduction_check *check)
{
	struct pg_reduction_check_state *state = check->state;
	if (!state) return;
	pg_comparison_destroy(&state->comparison);
	pg_dag_destroy(&state->dag);
	pg_index_destroy(&state->collection.records);
	pg_graph_destroy(&state->collection.storage);
	free(state);
	check->state = NULL;
}
