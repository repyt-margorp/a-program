#include "identity_internal.h"
#include "eval_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const char magic[8] = "APGIBD\2";
static const char scope_magic[8] = "APGISC\3";
static const char higher_magic[8] = "APGHSC\1";
static const char discovery_magic[8] = "APGASW\1";
static const char family_magic[8] = "APGFSW\1";
static const char family_result_magic[8] = "APGFRW\1";
static const char shadow_magic[8] = "APGSHD\2";
static const char visit_magic[8] = "APGSVS\2";
static const char analysis_magic[8] = "APGSAW\1";

static int scope_work_cursor(const struct scope_work *work)
{
	size_t n = work->scope.count;
	if (!n || !work->scope.source || !work->scope.body || !work->scope.bindings) return 0;
	if (!work->cursor || !work->order || !work->used || work->phase > SCOPE_READY) return 0;
	if (work->count > n || work->position > n || work->selected > work->count) return 0;
	if (work->phase == SCOPE_FILTER && work->selected > work->position) return 0;
	if (work->phase == SCOPE_APPLY && work->position > work->count) return 0;
	if (work->reference && (work->reference->kind != PG_REFERENCE || work->reference_position >= n)) return 0;
	if (work->phase == SCOPE_SOURCES && work->position < n && work->cursor->kind != PG_LAMBDA) return 0;
	if (work->phase >= SCOPE_FILTER && !work->result) return 0;
	for (size_t i = 0; i < n; ++i) if (work->order[i] >= n || work->used[i] > 1) return 0;
	return 1;
}

int pg_scope_work_write(FILE *file, const struct scope_work *work,
	size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots) || !scope_work_cursor(work)) return -1;
	if (scope_count && !scopes) return -1;
	if (scope_count >= SIZE_MAX / sizeof(const struct action_scope *)) return -1;
	size_t seen = work->seen.count, sources = work->sources.count;
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (sources > maximum - 4 || count > maximum - 4 - sources
		|| seen >= SIZE_MAX / sizeof(struct scope_visit *)) return -1;
	struct pg_graph temporary = {0};
	int status = -1;
	if (pg_graph_init(&temporary)) return -1;
	struct scope_visit **visits = pg_alloc(&temporary, (seen + 1) * sizeof(*visits));
	const struct action_scope **owned = pg_alloc(&temporary, (scope_count + 1) * sizeof(*owned));
	const struct pg_term **terms = pg_alloc(&temporary, (4 + sources + count) * sizeof(*terms));
	if (!visits || !terms || !owned) goto done;
	owned[0] = &work->scope;
	for (size_t i = 0; i < scope_count; ++i) owned[i + 1] = scopes[i];
	const struct pg_term *optional[] = {work->head, work->reference, work->cursor, work->result};
	unsigned mask = 0;
	for (size_t i = 0; i < 4; ++i) {
		if (optional[i]) mask |= 1u << i;
		terms[i] = optional[i] ? optional[i] : work->scope.source;
	}
	uint64_t fields[] = {work->scope.count, work->phase, work->count, work->position,
		work->selected, work->reference_position, (work->changed != 0) | (work->canonical != 0) << 1,
		mask, seen, sources};
	if (fwrite(analysis_magic, 1, 8, file) != 8) goto done;
	for (size_t i = 0; i < 10; ++i) if (pg_wire_write_u64(file, fields[i])) goto done;
	for (size_t i = 0; i < work->scope.count; ++i)
		if (pg_wire_write_u64(file, work->order[i]) || pg_wire_write_u64(file, work->used[i])) goto done;
	size_t n = 0;
	for (size_t i = 0; i < work->sources.capacity; ++i) {
		for (struct pg_index_entry *entry = work->sources.buckets[i]; entry; entry = entry->next) {
			const struct scope_binding_index *source = (const struct scope_binding_index *)entry;
			if (n == sources || source->position >= work->scope.count) goto done;
			if (pg_wire_write_u64(file, source->position)) goto done;
			terms[4 + n++] = pg_reference(&temporary, source->binder);
		}
	}
	if (n != sources) goto done;
	visits[0] = work->pending;
	n = 0;
	for (size_t i = 0; i < work->seen.capacity; ++i)
		for (struct pg_index_entry *entry = work->seen.buckets[i]; entry; entry = entry->next) {
			if (n == seen) goto done;
			visits[1 + n++] = (struct scope_visit *)entry;
		}
	if (n != seen) goto done;
	for (size_t i = 0; i < count; ++i) terms[4 + sources + i] = roots[i];
	status = pg_scope_visits_write(file, seen + 1, visits, 1, &work->shadow,
		scope_count + 1, owned, 4 + sources + count, terms, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_scope_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct scope_work **work, size_t *scope_count, struct action_scope *const **owned,
	size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots || !scope_count || !owned) return -1;
	*work = NULL;
	*scope_count = 0;
	*owned = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t f[10], value;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, analysis_magic, 8)) return -1;
	for (size_t i = 0; i < 10; ++i) if (pg_wire_read_u64(file, &f[i])) return -1;
	if (!f[0] || f[0] > limit || f[0] > SIZE_MAX / sizeof(size_t) || f[1] > SCOPE_READY
		|| f[2] > f[0] || f[3] > f[0] || f[4] > f[2] || f[5] > SIZE_MAX
		|| f[6] > 3 || f[7] > 15 || f[8] >= limit || f[9] > f[0]
		|| f[9] > SIZE_MAX / sizeof(struct scope_binding_index *)) return -1;
	struct scope_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->arena = arena;
	candidate->output = output;
	candidate->phase = (int)f[1];
	candidate->count = (size_t)f[2];
	candidate->position = (size_t)f[3];
	candidate->selected = (size_t)f[4];
	candidate->reference_position = (size_t)f[5];
	candidate->changed = f[6] & 1;
	candidate->canonical = (f[6] >> 1) & 1;
	candidate->order = pg_alloc(arena, (size_t)f[0] * sizeof(*candidate->order));
	candidate->used = pg_alloc(arena, (size_t)f[0]);
	struct scope_binding_index **sources = pg_alloc(arena, (size_t)f[9] * sizeof(*sources));
	if (!candidate->order || !candidate->used || !sources) return -1;
	for (size_t i = 0; i < f[0]; ++i) {
		if (pg_wire_read_u64(file, &value) || value >= f[0]) return -1;
		candidate->order[i] = (size_t)value;
		if (pg_wire_read_u64(file, &value) || value > 1) return -1;
		candidate->used[i] = (unsigned char)value;
	}
	for (size_t i = 0; i < f[9]; ++i) {
		if (pg_wire_read_u64(file, &value) || value >= f[0]) return -1;
		sources[i] = pg_alloc(arena, sizeof(*sources[i]));
		if (!sources[i]) return -1;
		sources[i]->position = (size_t)value;
	}
	size_t nv, ns, no, nt;
	struct scope_visit *const *visits;
	const struct scope_shadow *const *shadows;
	struct action_scope *const *scopes;
	const struct pg_term *const *terms;
	if (pg_scope_visits_read(file, arena, output, limit, name_limit, codec, owner,
		&nv, &visits, &ns, &shadows, &no, &scopes, &nt, &terms)) return -1;
	if (nv != f[8] + 1 || ns != 1 || !no || !scopes[0] || scopes[0]->count != f[0]
		|| nt < 4 || f[9] > nt - 4) return -1;
	candidate->scope = *scopes[0];
	candidate->pending = visits[0];
	candidate->shadow = shadows[0];
	for (size_t i = 0; i < 4; ++i)
		if (!(f[7] & (1u << i)) && terms[i] != candidate->scope.source) return -1;
	candidate->head = f[7] & 1 ? terms[0] : NULL;
	candidate->reference = f[7] & 2 ? terms[1] : NULL;
	candidate->cursor = f[7] & 4 ? terms[2] : NULL;
	candidate->result = f[7] & 8 ? terms[3] : NULL;
	if (!scope_work_cursor(candidate)) return -1;
	for (size_t i = 0; i < f[9]; ++i) {
		const struct pg_term *term = terms[4 + i];
		if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) return -1;
		sources[i]->binder = term->as.reference;
		if (candidate->scope.bindings[sources[i]->position].source != sources[i]->binder) return -1;
	}
	if (no - 1 > SIZE_MAX / sizeof(struct action_scope *)) return -1;
	struct action_scope **external = pg_alloc(arena, (no - 1) * sizeof(*external));
	if (!external) return -1;
	/* The embedded scope's final address is part of work identity. */
	for (size_t i = 1; i < no; ++i)
		external[i - 1] = scopes[i] == scopes[0] ? &candidate->scope : scopes[i];
	if (pg_scope_indexes_restore(candidate, (size_t)f[8], visits + 1, (size_t)f[9], sources)) return -1;
	*work = candidate;
	*scope_count = no - 1;
	*owned = external;
	*count = nt - 4 - (size_t)f[9];
	*roots = terms + 4 + (size_t)f[9];
	return 0;
}

static int visit_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct scope_visit *visit = key;
	if (index || !visit->next) return 0;
	*child = visit->next;
	return 1;
}

int pg_scope_visits_write(FILE *file, size_t visit_count, struct scope_visit *const *visits,
	size_t shadow_count, const struct scope_shadow *const *shadows,
	size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (visit_count && !visits) || (shadow_count && !shadows) || (count && !roots)) return -1;
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_dag_init(&dag, visit_child, NULL)) goto done;
	for (size_t i = 0; i < visit_count; ++i)
		if (visits[i] && pg_dag_add(&dag, visits[i])) goto done;
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (dag.count > maximum || count > maximum - dag.count) goto done;
	maximum = SIZE_MAX / sizeof(const struct scope_shadow *);
	if (dag.count > maximum || shadow_count > maximum - dag.count) goto done;
	const struct pg_term **terms = pg_alloc(&dag.storage, (dag.count + count) * sizeof(*terms));
	const struct scope_shadow **all = pg_alloc(&dag.storage, (dag.count + shadow_count) * sizeof(*all));
	if (!terms || !all) goto done;
	if (fwrite(visit_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count)
		|| pg_wire_write_u64(file, visit_count)) goto done;
	for (size_t i = 0; i < visit_count; ++i) {
		const struct pg_dag_node *node = pg_dag_find(&dag, visits[i]);
		if (pg_wire_write_u64(file, node ? node->id : 0)) goto done;
	}
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct scope_visit *visit = node->key;
		if (!visit->term) goto done;
		const struct pg_dag_node *next = pg_dag_find(&dag, visit->next);
		if (pg_wire_write_u64(file, next ? next->id : 0)) goto done;
		terms[node->id - 1] = visit->term;
		all[node->id - 1] = visit->shadow;
	}
	for (size_t i = 0; i < count; ++i) terms[dag.count + i] = roots[i];
	for (size_t i = 0; i < shadow_count; ++i) all[dag.count + i] = shadows[i];
	status = pg_scope_shadows_write(file, dag.count + shadow_count, all,
		scope_count, scopes, dag.count + count, terms, codec, owner);
done:
	pg_dag_destroy(&dag);
	return status;
}

int pg_scope_visits_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *visit_count, struct scope_visit *const **visits,
	size_t *shadow_count, const struct scope_shadow *const **shadows,
	size_t *scope_count, struct action_scope *const **scopes,
	size_t *count, const struct pg_term *const **roots)
{
	if (!visit_count || !visits || !shadow_count || !shadows || !scope_count || !scopes || !count || !roots) return -1;
	*visit_count = *shadow_count = *count = 0;
	*scope_count = 0;
	*scopes = NULL;
	*visits = NULL;
	*shadows = NULL;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t amount, root_count, id;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, visit_magic, 8)
		|| pg_wire_read_u64(file, &amount) || pg_wire_read_u64(file, &root_count)) return -1;
	if (amount > limit || root_count > limit || amount > SIZE_MAX / sizeof(struct scope_visit)
		|| root_count > SIZE_MAX / sizeof(struct scope_visit *)) return -1;
	struct scope_visit *nodes = pg_alloc(arena, (size_t)amount * sizeof(*nodes));
	struct scope_visit **selected = pg_alloc(arena, (size_t)root_count * sizeof(*selected));
	unsigned char *used = pg_alloc(arena, (size_t)amount);
	if (!nodes || !selected || !used) return -1;
	for (size_t i = 0; i < root_count; ++i) {
		if (pg_wire_read_u64(file, &id) || id > amount) return -1;
		selected[i] = id ? &nodes[id - 1] : NULL;
		if (id) used[id - 1] = 1;
	}
	for (size_t i = 0; i < amount; ++i) {
		if (pg_wire_read_u64(file, &id) || id > i) return -1;
		nodes[i].next = id ? &nodes[id - 1] : NULL;
		if (id) used[id - 1] = 1;
	}
	for (size_t i = 0; i < amount; ++i) if (!used[i]) return -1;
	size_t n, total, kept;
	struct action_scope *const *retained;
	const struct scope_shadow *const *all;
	const struct pg_term *const *terms;
	if (pg_scope_shadows_read(file, arena, output, limit, name_limit, codec, owner,
		&n, &all, &kept, &retained, &total, &terms) || n < amount || total < amount) return -1;
	for (size_t i = 0; i < amount; ++i) {
		nodes[i].term = terms[i];
		nodes[i].shadow = all[i];
	}
	*visit_count = (size_t)root_count;
	*visits = selected;
	*shadow_count = n - (size_t)amount;
	*shadows = all + (size_t)amount;
	*scope_count = kept;
	*scopes = retained;
	*count = total - (size_t)amount;
	*roots = terms + (size_t)amount;
	return 0;
}

static int shadow_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct scope_shadow *shadow = key;
	if (index || !shadow->parent) return 0;
	*child = shadow->parent;
	return 1;
}

int pg_scope_shadows_write(FILE *file, size_t shadow_count, const struct scope_shadow *const *shadows,
	size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (shadow_count && !shadows) || (count && !roots)) return -1;
	struct pg_dag dag = {0};
	int status = -1;
	if (pg_dag_init(&dag, shadow_child, NULL) || pg_graph_init(&dag.storage)) goto done;
	for (size_t i = 0; i < shadow_count; ++i)
		if (shadows[i] && pg_dag_add(&dag, shadows[i])) goto done;
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (dag.count > maximum || count > maximum - dag.count) goto done;
	const struct pg_term **all = pg_alloc(&dag.storage, (dag.count + count) * sizeof(*all));
	if (!all) goto done;
	if (fwrite(shadow_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count)
		|| pg_wire_write_u64(file, shadow_count)) goto done;
	for (size_t i = 0; i < shadow_count; ++i) {
		const struct pg_dag_node *node = pg_dag_find(&dag, shadows[i]);
		if (pg_wire_write_u64(file, node ? node->id : 0)) goto done;
	}
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct scope_shadow *shadow = node->key;
		if (!shadow->binder || shadow->binder->kind != PG_BINDER) goto done;
		const struct pg_dag_node *parent = pg_dag_find(&dag, shadow->parent);
		if (pg_wire_write_u64(file, parent ? parent->id : 0)) goto done;
		all[node->id - 1] = pg_reference(&dag.storage, shadow->binder);
	}
	for (size_t i = 0; i < count; ++i) all[dag.count + i] = roots[i];
	status = pg_action_scopes_write(file, scope_count, scopes, dag.count + count, all, codec, owner);
done:
	pg_dag_destroy(&dag);
	return status;
}

int pg_scope_shadows_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *shadow_count, const struct scope_shadow *const **shadows,
	size_t *scope_count, struct action_scope *const **scopes,
	size_t *count, const struct pg_term *const **roots)
{
	if (!shadow_count || !shadows || !scope_count || !scopes || !count || !roots) return -1;
	*shadow_count = 0;
	*shadows = NULL;
	*scope_count = 0;
	*scopes = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t amount, root_count, id;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, shadow_magic, 8)
		|| pg_wire_read_u64(file, &amount) || pg_wire_read_u64(file, &root_count)) return -1;
	if (amount > limit || root_count > limit || amount > SIZE_MAX / sizeof(struct scope_shadow)
		|| root_count > SIZE_MAX / sizeof(const struct scope_shadow *)) return -1;
	struct scope_shadow *nodes = pg_alloc(arena, (size_t)amount * sizeof(*nodes));
	const struct scope_shadow **selected = pg_alloc(arena, (size_t)root_count * sizeof(*selected));
	unsigned char *used = pg_alloc(arena, (size_t)amount);
	if (!nodes || !selected || !used) return -1;
	for (size_t i = 0; i < root_count; ++i) {
		if (pg_wire_read_u64(file, &id) || id > amount) return -1;
		selected[i] = id ? &nodes[id - 1] : NULL;
		if (id) used[id - 1] = 1;
	}
	for (size_t i = 0; i < amount; ++i) {
		if (pg_wire_read_u64(file, &id) || id > i) return -1;
		nodes[i].parent = id ? &nodes[id - 1] : NULL;
		if (id) used[id - 1] = 1;
	}
	for (size_t i = 0; i < amount; ++i) if (!used[i]) return -1;
	size_t total, kept;
	struct action_scope *const *retained;
	const struct pg_term *const *all;
	if (pg_action_scopes_read(file, arena, output, limit, name_limit, codec, owner,
		&kept, &retained, &total, &all) || total < amount) return -1;
	for (size_t i = 0; i < amount; ++i) {
		if (all[i]->kind != PG_REFERENCE || all[i]->as.reference->kind != PG_BINDER) return -1;
		nodes[i].binder = all[i]->as.reference;
	}
	*shadow_count = (size_t)root_count;
	*shadows = selected;
	*scope_count = kept;
	*scopes = retained;
	*count = total - (size_t)amount;
	*roots = all + (size_t)amount;
	return 0;
}

static int family_result_cursor(const struct family_result_work *work)
{
	if (!work->family || work->count % 3 || work->position > work->count) return 0;
	if (work->closure.discard || work->closure.remaining > work->count / 3) return 0;
	switch (work->phase) {
	case FAMILY_COLLECT:
		if (work->closure.remaining != work->count / 3) return 0;
		return !work->position || work->family->kind == PG_APPLICATION;
	case FAMILY_WRAP:
		return !work->position;
	case FAMILY_APPLY:
		return !work->closure.remaining;
	}
	return 0;
}

int pg_family_result_write(FILE *file, const struct family_result_work *work,
	size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots) || !family_result_cursor(work)) return -1;
	size_t start = work->phase == FAMILY_COLLECT ? work->position : 0;
	size_t kept = work->count - start, maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (kept >= maximum || count > maximum - kept - 1 || (kept && !work->arguments)) return -1;
	struct pg_graph temporary = {0};
	int status = -1;
	const struct pg_term **all = pg_alloc(&temporary, (1 + kept + count) * sizeof(*all));
	if (!all) goto done;
	all[0] = work->family;
	for (size_t i = 0; i < kept; ++i) all[1 + i] = work->arguments[start + i];
	for (size_t i = 0; i < count; ++i) all[1 + kept + i] = roots[i];
	if (fwrite(family_result_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->count)
		|| pg_wire_write_u64(file, work->position) || pg_wire_write_u64(file, work->phase)) goto done;
	status = pg_action_ownership_write(file, scope_count, scopes, &work->closure,
		1 + kept + count, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_family_result_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct family_result_work **work, size_t *scope_count, struct action_scope *const **scopes,
	size_t *count, const struct pg_term *const **roots)
{
	if (!work || !scope_count || !scopes || !count || !roots) return -1;
	*work = NULL;
	*scope_count = 0;
	*scopes = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t arity, position, phase;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, family_result_magic, 8)
		|| pg_wire_read_u64(file, &arity) || pg_wire_read_u64(file, &position)
		|| pg_wire_read_u64(file, &phase)) return -1;
	if (arity > limit || arity > SIZE_MAX / sizeof(const struct pg_term *)
		|| position > arity || phase > FAMILY_APPLY) return -1;
	size_t total, n;
	const struct pg_term *const *all;
	struct action_scope *const *saved;
	struct action_result_work *closure;
	if (pg_action_ownership_read(file, arena, output, limit, name_limit, codec, owner,
		&n, &saved, &closure, &total, &all) || !closure) return -1;
	size_t start = phase == FAMILY_COLLECT ? (size_t)position : 0, kept = (size_t)arity - start;
	if (!total || kept > total - 1) return -1;
	struct family_result_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	*candidate = (struct family_result_work){.closure = *closure, .family = all[0],
		.count = (size_t)arity, .position = (size_t)position, .phase = (int)phase};
	if (!family_result_cursor(candidate)) return -1;
	candidate->arguments = pg_alloc(arena, (size_t)arity * sizeof(*candidate->arguments));
	if (!candidate->arguments) return -1;
	for (size_t i = 0; i < kept; ++i) candidate->arguments[start + i] = all[1 + i];
	*work = candidate;
	*scope_count = n;
	*scopes = saved;
	*count = total - 1 - kept;
	*roots = all + 1 + kept;
	return 0;
}

static int family_cursor(const struct family_scope_work *work)
{
	if (!work->cursor || work->scope.bindings) return 0;
	if (!work->scope.source) return !work->scope.body && !work->scope.count && !work->content;
	if (!work->scope.body || !work->supplied || work->supplied % 3) return 0;
	return work->scope.count <= work->supplied / 3;
}

int pg_family_scope_write(FILE *file, const struct family_scope_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots) || !family_cursor(work)) return -1;
	if (count > SIZE_MAX / sizeof(const struct pg_term *) - 5) return -1;
	struct pg_graph temporary = {0};
	int status = -1;
	const struct pg_term **all = pg_alloc(&temporary, (count + 5) * sizeof(*all));
	if (!all) goto done;
	const struct pg_term *optional[] = {work->scope.source, work->scope.body, work->content, work->value};
	unsigned mask = 0;
	all[0] = work->cursor;
	for (size_t i = 0; i < 4; ++i) {
		if (optional[i]) mask |= 1u << i;
		all[i + 1] = optional[i] ? optional[i] : work->cursor;
	}
	for (size_t i = 0; i < count; ++i) all[i + 5] = roots[i];
	if (fwrite(family_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->supplied)
		|| pg_wire_write_u64(file, work->scope.count) || pg_wire_write_u64(file, mask)) goto done;
	status = pg_graph_write_descriptors(file, count + 5, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_family_scope_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct family_scope_work **work, size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t supplied, arity, mask;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, family_magic, 8)
		|| pg_wire_read_u64(file, &supplied) || pg_wire_read_u64(file, &arity)
		|| pg_wire_read_u64(file, &mask)) return -1;
	if (supplied > SIZE_MAX || arity > limit || arity > SIZE_MAX || mask > 15) return -1;
	size_t total;
	const struct pg_term *const *all;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &total, &all) || total < 5) return -1;
	for (size_t i = 0; i < 4; ++i) if (!(mask & (1u << i)) && all[i + 1] != all[0]) return -1;
	struct family_scope_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->cursor = all[0];
	candidate->scope.source = mask & 1 ? all[1] : NULL;
	candidate->scope.body = mask & 2 ? all[2] : NULL;
	candidate->content = mask & 4 ? all[3] : NULL;
	candidate->value = mask & 8 ? all[4] : NULL;
	candidate->supplied = (size_t)supplied;
	candidate->scope.count = (size_t)arity;
	if (!family_cursor(candidate)) return -1;
	*work = candidate;
	*count = total - 5;
	*roots = all + 5;
	return 0;
}

static int scope_cursor(const struct action_scope_work *work)
{
	if (!work->scope.source || !work->scope.body || !work->cursor || work->scope.bindings) return 0;
	if (work->scope.count > SIZE_MAX / 3 || work->position > work->scope.count) return 0;
	if (work->center % 3 || work->center / 3 > work->position) return 0;
	if (work->scope.body->kind != PG_REFERENCE) return !work->position && !work->center;
	return work->position == work->scope.count || work->cursor->kind == PG_LAMBDA;
}

int pg_action_scope_work_write_with(FILE *file, const struct action_scope_work *work,
	size_t count, const struct pg_eval_configuration *roots,
	int (*write_configurations)(FILE *, size_t, const struct pg_eval_configuration *, void *), void *owner)
{
	if (!file || !work || !write_configurations || (count && !roots) || !scope_cursor(work)) return -1;
	if (count > SIZE_MAX / sizeof(struct pg_eval_configuration) - 3) return -1;
	struct pg_graph temporary = {0};
	int status = -1;
	struct pg_eval_configuration *all = pg_alloc(&temporary, (count + 3) * sizeof(*all));
	if (!all) goto done;
	all[0] = (struct pg_eval_configuration){{work->cursor, NULL}, work->arguments};
	all[1].head.term = work->scope.source;
	all[2].head.term = work->scope.body;
	for (size_t i = 0; i < count; ++i) all[i + 3] = roots[i];
	if (fwrite(discovery_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->scope.count)
		|| pg_wire_write_u64(file, work->position) || pg_wire_write_u64(file, work->center)) goto done;
	status = write_configurations(file, count + 3, all, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_action_scope_work_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_configurations)(FILE *, struct pg_graph *, size_t, size_t, size_t *,
		const struct pg_eval_configuration **, void *), void *owner,
	struct action_scope_work **work, size_t *count, const struct pg_eval_configuration **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output || !read_configurations) return -1;
	char header[8];
	uint64_t arity, position, center;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, discovery_magic, 8)
		|| pg_wire_read_u64(file, &arity) || pg_wire_read_u64(file, &position)
		|| pg_wire_read_u64(file, &center)) return -1;
	if (arity > limit || arity > SIZE_MAX / 3 || position > arity || center > SIZE_MAX) return -1;
	size_t total;
	const struct pg_eval_configuration *all;
	if (read_configurations(file, output, limit, name_limit, &total, &all, owner) || total < 3) return -1;
	for (size_t i = 0; i < 3; ++i) if (all[i].head.environment) return -1;
	if (all[1].arguments || all[2].arguments) return -1;
	struct action_scope_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->scope = (struct action_scope){all[1].head.term, all[2].head.term, (size_t)arity, NULL};
	candidate->cursor = all[0].head.term;
	candidate->arguments = all[0].arguments;
	candidate->position = (size_t)position;
	candidate->center = (size_t)center;
	if (!scope_cursor(candidate)) return -1;
	*work = candidate;
	*count = total - 3;
	*roots = all + 3;
	return 0;
}

struct discovery_configuration_codec {
	const struct pg_graph_codec *codec;
	void *owner;
};

static int discovery_configurations_write(FILE *file, size_t count,
	const struct pg_eval_configuration *roots, void *opaque)
{
	struct discovery_configuration_codec *context = opaque;
	return pg_eval_configurations_write(file, count, roots, context->codec, context->owner);
}

static int discovery_configurations_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_eval_configuration **roots, void *opaque)
{
	struct discovery_configuration_codec *context = opaque;
	return pg_eval_configurations_read(file, graph, limit, name_limit, context->codec, context->owner, count, roots);
}

int pg_action_scope_work_write(FILE *file, const struct action_scope_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner)
{
	struct discovery_configuration_codec context = {codec, owner};
	return pg_action_scope_work_write_with(file, work, count, roots, discovery_configurations_write, &context);
}

int pg_action_scope_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_scope_work **work, size_t *count, const struct pg_eval_configuration **roots)
{
	struct discovery_configuration_codec context = {codec, owner};
	return pg_action_scope_work_read_with(file, arena, output, limit, name_limit,
		discovery_configurations_read, &context, work, count, roots);
}

static int higher_state(size_t arity, size_t position, size_t lambdas, unsigned flags)
{
	if (!arity || position > arity || flags > 7) return 0;
	if (lambdas && !(flags & 4)) return 0;
	if (!(flags & 1)) return !position && !(flags & 2);
	return (flags & 4) && lambdas;
}

int pg_higher_scope_write(FILE *file, const struct higher_scope_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots)) return -1;
	unsigned flags = (work->body != NULL) | (work->wrapping != 0) << 1 | (work->collecting != 0) << 2;
	if (!higher_state(work->arity, work->position, work->lambda_count, flags)) return -1;
	size_t kept = work->wrapping ? work->arity : work->position;
	size_t base = work->body ? 3 : 2, maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (kept > maximum - base || count > maximum - base - kept || (kept && !work->binders)) return -1;
	struct pg_graph temporary;
	if (pg_graph_init(&temporary)) return -1;
	int status = -1;
	const struct pg_term **all = pg_alloc(&temporary, (base + kept + count) * sizeof(*all));
	if (!all) goto done;
	all[0] = work->source;
	all[1] = work->cursor;
	if (work->body) all[2] = work->body;
	for (size_t i = 0; i < kept; ++i) {
		if (!work->binders[i] || work->binders[i]->kind != PG_BINDER) goto done;
		all[base + i] = pg_reference(&temporary, work->binders[i]);
	}
	for (size_t i = 0; i < count; ++i) all[base + kept + i] = roots[i];
	if (fwrite(higher_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->arity)
		|| pg_wire_write_u64(file, work->position) || pg_wire_write_u64(file, work->lambda_count)
		|| pg_wire_write_u64(file, flags)) goto done;
	status = pg_graph_write_descriptors(file, base + kept + count, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_higher_scope_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct higher_scope_work **work, size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t arity, position, lambdas, flags;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, higher_magic, 8)
		|| pg_wire_read_u64(file, &arity) || pg_wire_read_u64(file, &position)
		|| pg_wire_read_u64(file, &lambdas) || pg_wire_read_u64(file, &flags)) return -1;
	if (arity > limit || arity > SIZE_MAX / sizeof(const struct pg_object *) || position > arity
		|| lambdas > limit || lambdas > SIZE_MAX || flags > 7) return -1;
	if (!higher_state((size_t)arity, (size_t)position, (size_t)lambdas, (unsigned)flags)) return -1;
	size_t base = flags & 1 ? 3 : 2, kept = flags & 2 ? (size_t)arity : (size_t)position;
	size_t total;
	const struct pg_term *const *all;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &total, &all)) return -1;
	if (total < base || kept > total - base) return -1;
	struct higher_scope_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->graph = output;
	candidate->arena = arena;
	candidate->source = all[0];
	candidate->cursor = all[1];
	candidate->arity = (size_t)arity;
	candidate->position = (size_t)position;
	candidate->lambda_count = (size_t)lambdas;
	candidate->wrapping = (flags & 2) != 0;
	candidate->collecting = (flags & 4) != 0;
	if (flags & 1) {
		candidate->body = all[2];
		candidate->binders = pg_alloc(arena, (size_t)arity * sizeof(*candidate->binders));
		if (!candidate->binders) return -1;
	}
	for (size_t i = 0; i < kept; ++i) {
		const struct pg_term *term = all[base + i];
		if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) return -1;
		candidate->binders[i] = term->as.reference;
	}
	*work = candidate;
	*count = total - base - kept;
	*roots = all + base + kept;
	return 0;
}

int pg_action_ownership_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	const struct action_result_work *result, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (scope_count && !scopes) || (count && !roots)) return -1;
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *);
	struct pg_dag owners = {0}, arrays = {0};
	/* Distinct scopes can share mutable binding storage. Preserve both
	 * pointer identities rather than duplicating or content-interning it. */
	int status = -1;
	if (pg_dag_init(&owners, NULL, NULL) || pg_graph_init(&owners.storage)) goto done;
	if (pg_dag_init(&arrays, NULL, NULL)) goto done;
	for (size_t i = 0; i < scope_count; ++i)
		if (scopes[i] && pg_dag_add(&owners, scopes[i])) goto done;
	size_t capacity = count;
	if (capacity > maximum) goto done;
	if (result) {
		if (!result->result || capacity == maximum || (result->remaining && !result->bindings)) goto done;
		++capacity;
		if (result->bindings && pg_dag_add(&arrays, result->bindings)) goto done;
	}
	for (const struct pg_dag_node *node = owners.first; node; node = node->next) {
		const struct action_scope *scope = node->key;
		if (!scope->source || !scope->body || maximum - capacity < 2) goto done;
		capacity += 2;
		if (scope->bindings && pg_dag_add(&arrays, scope->bindings)) goto done;
	}
	if (arrays.count > SIZE_MAX / sizeof(size_t)) goto done;
	size_t *lengths = pg_alloc(&owners.storage, arrays.count * sizeof(*lengths));
	if (!lengths) goto done;
	if (result && result->bindings) lengths[pg_dag_find(&arrays, result->bindings)->id - 1] = result->remaining;
	for (const struct pg_dag_node *node = owners.first; node; node = node->next) {
		const struct action_scope *scope = node->key;
		if (!scope->bindings) continue;
		size_t index = pg_dag_find(&arrays, scope->bindings)->id - 1;
		if (lengths[index] < scope->count) lengths[index] = scope->count;
	}
	for (size_t i = 0; i < arrays.count; ++i) {
		if (lengths[i] > (maximum - capacity) / 4) goto done;
		capacity += 4 * lengths[i];
	}
	const struct pg_term **all = pg_alloc(&owners.storage, capacity * sizeof(*all));
	if (!all) goto done;
	size_t total = 0;
	if (fwrite(scope_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, owners.count)
		|| pg_wire_write_u64(file, scope_count) || pg_wire_write_u64(file, arrays.count)
		|| pg_wire_write_u64(file, result != NULL)) goto done;
	for (size_t i = 0; i < scope_count; ++i) {
		const struct pg_dag_node *node = scopes[i] ? pg_dag_find(&owners, scopes[i]) : NULL;
		if (pg_wire_write_u64(file, node ? node->id : 0)) goto done;
	}
	for (const struct pg_dag_node *node = owners.first; node; node = node->next) {
		const struct action_scope *scope = node->key;
		all[total++] = scope->source;
		all[total++] = scope->body;
		const struct pg_dag_node *array = scope->bindings ? pg_dag_find(&arrays, scope->bindings) : NULL;
		if (pg_wire_write_u64(file, scope->count) || pg_wire_write_u64(file, array ? array->id : 0)) goto done;
	}
	if (result) {
		const struct pg_dag_node *array = result->bindings ? pg_dag_find(&arrays, result->bindings) : NULL;
		if (pg_wire_write_u64(file, result->remaining) || pg_wire_write_u64(file, result->discard)
			|| pg_wire_write_u64(file, array ? array->id : 0)) goto done;
		all[total++] = result->result;
	}
	for (const struct pg_dag_node *node = arrays.first; node; node = node->next) {
		const struct action_binding *bindings = node->key;
		size_t length = lengths[node->id - 1];
		if (pg_wire_write_u64(file, length)) goto done;
		for (size_t i = 0; i < length; ++i) {
			const struct action_binding *binding = &bindings[i];
			const struct pg_object *fields[] = {binding->source,
				binding->arguments[0], binding->arguments[1], binding->arguments[2]};
			unsigned mask = 0;
			for (size_t j = 0; j < 4; ++j) {
				if (!fields[j]) continue;
				if (fields[j]->kind != PG_BINDER) goto done;
				mask |= 1u << j;
				all[total] = pg_reference(&owners.storage, fields[j]);
				if (!all[total++]) goto done;
			}
			if (pg_wire_write_u64(file, mask)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) all[total++] = roots[i];
	status = pg_graph_write_descriptors(file, total, all, codec, owner);
done:
	pg_dag_destroy(&arrays);
	pg_dag_destroy(&owners);
	return status;
}

int pg_action_ownership_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *scope_count, struct action_scope *const **scopes, struct action_result_work **result,
	size_t *count, const struct pg_term *const **roots)
{
	if (!scope_count || !scopes || !result || !count || !roots) return -1;
	*result = NULL;
	*scope_count = 0;
	*scopes = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t n, references, array_count, has_result;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, scope_magic, 8)
		|| pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &references)
		|| pg_wire_read_u64(file, &array_count) || pg_wire_read_u64(file, &has_result) || has_result > 1) return -1;
	if (n > references || references > limit || n > SIZE_MAX / sizeof(struct action_scope)
		|| references > SIZE_MAX / sizeof(struct action_scope *) || array_count > n + has_result) return -1;
	struct action_scope *entries = pg_alloc(arena, (size_t)n * sizeof(*entries));
	struct action_scope **links = pg_alloc(arena, (size_t)references * sizeof(*links));
	unsigned char **masks = pg_alloc(arena, (size_t)array_count * sizeof(*masks));
	struct action_binding **arrays = pg_alloc(arena, (size_t)array_count * sizeof(*arrays));
	size_t *lengths = pg_alloc(arena, (size_t)array_count * sizeof(*lengths));
	size_t *array_ids = pg_alloc(arena, (size_t)n * sizeof(*array_ids));
	unsigned char *array_used = pg_alloc(arena, (size_t)array_count + 1);
	unsigned char *used = pg_alloc(arena, (size_t)n + 1);
	if (!entries || !links || !masks || !used || !arrays || !lengths || !array_ids || !array_used) return -1;
	for (size_t i = 0; i < references; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || id > n) return -1;
		links[i] = id ? &entries[id - 1] : NULL;
		used[id] = 1;
	}
	for (size_t index = 0; index < n; ++index) {
		uint64_t arity, id;
		if (!used[index + 1] || pg_wire_read_u64(file, &arity) || pg_wire_read_u64(file, &id)) return -1;
		if (arity > limit || arity > SIZE_MAX / sizeof(struct action_binding) || id > array_count) return -1;
		entries[index].count = (size_t)arity;
		array_ids[index] = (size_t)id;
		array_used[id] = 1;
		if (id && lengths[id - 1] < arity) lengths[id - 1] = (size_t)arity;
	}
	struct action_result_work *pending = NULL;
	size_t result_array = 0;
	if (has_result) {
		uint64_t remaining, discard, id;
		if (pg_wire_read_u64(file, &remaining) || pg_wire_read_u64(file, &discard)
			|| pg_wire_read_u64(file, &id)) return -1;
		if (remaining > limit || remaining > SIZE_MAX / sizeof(struct action_binding)
			|| discard > SIZE_MAX || id > array_count || (remaining && !id)) return -1;
		pending = pg_alloc(arena, sizeof(*pending));
		if (!pending) return -1;
		pending->graph = output;
		pending->remaining = (size_t)remaining;
		pending->discard = (size_t)discard;
		result_array = (size_t)id;
		array_used[id] = 1;
		if (id && lengths[id - 1] < remaining) lengths[id - 1] = (size_t)remaining;
	}
	size_t remaining = limit;
	for (size_t index = 0; index < array_count; ++index) {
		uint64_t length;
		if (!array_used[index + 1] || pg_wire_read_u64(file, &length)) return -1;
		if (length != lengths[index] || length > remaining) return -1;
		remaining -= (size_t)length;
		arrays[index] = pg_alloc(arena, (size_t)length * sizeof(**arrays));
		masks[index] = pg_alloc(arena, (size_t)length);
		if (!arrays[index] || !masks[index]) return -1;
		for (size_t i = 0; i < length; ++i) {
			uint64_t mask;
			if (pg_wire_read_u64(file, &mask) || mask > 15) return -1;
			masks[index][i] = (unsigned char)mask;
		}
	}
	for (size_t i = 0; i < n; ++i)
		entries[i].bindings = array_ids[i] ? arrays[array_ids[i] - 1] : NULL;
	if (pending) pending->bindings = result_array ? arrays[result_array - 1] : NULL;
	size_t total;
	const struct pg_term *const *all;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &total, &all)) return -1;
	size_t position = 0;
	for (size_t index = 0; index < n; ++index) {
		struct action_scope *candidate = &entries[index];
		if (total - position < 2) return -1;
		candidate->source = all[position++];
		candidate->body = all[position++];
	}
	if (pending) {
		if (position == total) return -1;
		pending->result = all[position++];
	}
	for (size_t index = 0; index < array_count; ++index) {
		for (size_t i = 0; i < lengths[index]; ++i) {
			struct action_binding *binding = &arrays[index][i];
			const struct pg_object **fields[] = {&binding->source,
				&binding->arguments[0], &binding->arguments[1], &binding->arguments[2]};
			for (size_t j = 0; j < 4; ++j) {
				if (!(masks[index][i] & (1u << j))) continue;
				if (position == total) return -1;
				const struct pg_term *term = all[position++];
				if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) return -1;
				*fields[j] = term->as.reference;
			}
		}
	}
	*scope_count = (size_t)references;
	*scopes = links;
	*result = pending;
	*count = total - position;
	*roots = all + position;
	return 0;
}

int pg_action_scopes_write(FILE *file, size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	return pg_action_ownership_write(file, scope_count, scopes, NULL, count, roots, codec, owner);
}

int pg_action_scopes_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *scope_count, struct action_scope *const **scopes, size_t *count, const struct pg_term *const **roots)
{
	if (!scope_count || !scopes || !count || !roots) return -1;
	*scope_count = 0;
	*scopes = NULL;
	*count = 0;
	*roots = NULL;
	size_t n, total;
	struct action_scope *const *items;
	struct action_result_work *result;
	const struct pg_term *const *all;
	if (pg_action_ownership_read(file, arena, output, limit, name_limit, codec, owner,
		&n, &items, &result, &total, &all) || result) return -1;
	*scope_count = n;
	*scopes = items;
	*count = total;
	*roots = all;
	return 0;
}

int pg_action_scope_write(FILE *file, const struct action_scope *scope,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!scope) return -1;
	return pg_action_scopes_write(file, 1, &scope, count, roots, codec, owner);
}

int pg_action_scope_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_scope **scope, size_t *count, const struct pg_term *const **roots)
{
	if (!scope || !count || !roots) return -1;
	*scope = NULL;
	*count = 0;
	*roots = NULL;
	size_t n, total;
	struct action_scope *const *scopes;
	const struct pg_term *const *all;
	if (pg_action_scopes_read(file, arena, output, limit, name_limit, codec, owner, &n, &scopes, &total, &all)) return -1;
	if (n != 1 || !scopes[0]) return -1;
	*scope = scopes[0];
	*count = total;
	*roots = all;
	return 0;
}

struct body_scope_codec {
	struct pg_graph *arena;
	const struct pg_graph_codec *codec;
	void *owner;
	size_t count;
	const struct action_scope *const *inputs;
	struct action_scope *const *scopes;
};

static int body_scopes_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct body_scope_codec *context = opaque;
	return pg_action_scopes_write(file, context->count, context->inputs, count, roots, context->codec, context->owner);
}

static int body_scopes_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct body_scope_codec *context = opaque;
	return pg_action_scopes_read(file, context->arena, graph, limit, name_limit, context->codec, context->owner,
		&context->count, &context->scopes, count, roots);
}

int pg_action_body_write(FILE *file, const struct action_body_work *work,
	size_t scope_count, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots)) return -1;
	if (scope_count && !scopes) return -1;
	if (scope_count >= SIZE_MAX / sizeof(const struct action_scope *)) return -1;
	if ((unsigned)work->phase > BODY_READY || work->position > work->scope.count) return -1;
	if (count > SIZE_MAX / sizeof(const struct pg_term *) - 2) return -1;
	struct pg_graph scratch;
	if (pg_graph_init(&scratch)) return -1;
	int status = -1;
	const struct pg_term **all = pg_alloc(&scratch, (2 + count) * sizeof(*all));
	const struct action_scope **owned = pg_alloc(&scratch, (scope_count + 1) * sizeof(*owned));
	if (!all || !owned) goto done;
	all[0] = work->answer;
	all[1] = work->cursor ? work->cursor : work->scope.body;
	for (size_t i = 0; i < count; ++i) all[2 + i] = roots[i];
	owned[0] = &work->scope;
	for (size_t i = 0; i < scope_count; ++i) owned[1 + i] = scopes[i];
	struct body_scope_codec context = {.codec = codec, .owner = owner, .count = scope_count + 1, .inputs = owned};
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->phase)
		|| pg_wire_write_u64(file, work->scope.count) || pg_wire_write_u64(file, work->position)
		|| pg_wire_write_u64(file, work->cursor != NULL)) goto done;
	status = pg_comparison_write_with(file, &work->comparison, 2 + count, all, body_scopes_write, &context);
done:
	pg_graph_destroy(&scratch);
	return status;
}

int pg_action_body_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_body_work **work, size_t *scope_count, struct action_scope *const **scopes,
	size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots || !scope_count || !scopes) return -1;
	*work = NULL;
	*scope_count = 0;
	*scopes = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t phase, arity, position, cursor;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &phase) || pg_wire_read_u64(file, &arity)
		|| pg_wire_read_u64(file, &position) || pg_wire_read_u64(file, &cursor)) return -1;
	if (phase > BODY_READY || position > arity || arity > limit || cursor > 1) return -1;
	if (arity > SIZE_MAX / sizeof(struct action_binding)) return -1;
	struct action_body_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->arena = arena;
	candidate->output = output;
	candidate->phase = phase;
	candidate->scope.count = (size_t)arity;
	candidate->position = (size_t)position;
	size_t total;
	const struct pg_term *const *all;
	struct body_scope_codec context = {.arena = arena, .codec = codec, .owner = owner};
	if (pg_comparison_read_with(file, output, limit, name_limit, body_scopes_read, &context,
		&candidate->comparison, &total, &all)) return -1;
	if (total < 2 || !context.count || !context.scopes[0] || context.scopes[0]->count != arity) goto failure;
	candidate->scope = *context.scopes[0];
	candidate->answer = all[0];
	candidate->cursor = cursor ? all[1] : NULL;
	if (!cursor && all[1] != candidate->scope.body) goto failure;
	enum pg_comparison_status compared = pg_comparison_status(&candidate->comparison);
	if (phase == BODY_COMPARE) {
		if (position || cursor || compared == PG_COMPARISON_DIFFERENT) goto failure;
	} else if (phase == BODY_READY) {
		if (position || compared == PG_COMPARISON_PENDING) goto failure;
	} else {
		if (compared != PG_COMPARISON_DIFFERENT || !cursor) goto failure;
		if (!candidate->scope.bindings) goto failure;
		for (size_t i = 0; i < position; ++i)
			if (!candidate->scope.bindings[i].source) goto failure;
	}
	if (phase == BODY_COLLECT && position < arity && candidate->cursor->kind != PG_LAMBDA) goto failure;
	if (context.count - 1 > SIZE_MAX / sizeof(struct action_scope *)) goto failure;
	struct action_scope **external = pg_alloc(arena, (context.count - 1) * sizeof(*external));
	if (!external) goto failure;
	for (size_t i = 1; i < context.count; ++i)
		external[i - 1] = context.scopes[i] == context.scopes[0] ? &candidate->scope : context.scopes[i];
	*work = candidate;
	*scope_count = context.count - 1;
	*scopes = external;
	*count = total - 2;
	*roots = all + 2;
	return 0;
failure:
	pg_action_body_operation.destroy(candidate);
	return -1;
}

struct body_configuration_codec {
	const struct action_body_work *source;
	struct action_body_work *restored;
	struct pg_graph *arena;
	const struct pg_graph_codec *codec;
	void *owner;
};

static int write_body_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct body_configuration_codec *context = opaque;
	return pg_action_body_write(file, context->source, 0, NULL, count, roots, context->codec, context->owner);
}

static int read_body_terms(FILE *file, struct pg_graph *output, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct body_configuration_codec *context = opaque;
	size_t scope_count;
	struct action_scope *const *scopes;
	int status = pg_action_body_read(file, context->arena, output, limit, name_limit,
		context->codec, context->owner, &context->restored, &scope_count, &scopes, count, roots);
	return status || scope_count ? -1 : 0;
}

int pg_action_body_configurations_write(FILE *file, const struct action_body_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner)
{
	struct body_configuration_codec context = {.source = work, .codec = codec, .owner = owner};
	return pg_eval_configurations_write_with(file, count, roots, write_body_terms, &context);
}

int pg_action_body_configurations_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_body_work **work, size_t *count, const struct pg_eval_configuration **roots)
{
	if (!work) return -1;
	*work = NULL;
	struct body_configuration_codec context = {.arena = arena, .codec = codec, .owner = owner};
	if (pg_eval_configurations_read_with(file, output, limit, name_limit, count, roots, read_body_terms, &context)) {
		if (context.restored) pg_action_body_operation.destroy(context.restored);
		return -1;
	}
	*work = context.restored;
	return 0;
}
