#include "identity_internal.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGIBD\1";

static size_t retained_bindings(const struct action_body_work *work)
{
	return work->phase == BODY_COLLECT || work->phase == BODY_WRAP ? work->position : 0;
}

int pg_action_body_write(FILE *file, const struct action_body_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots)) return -1;
	if ((unsigned)work->phase > BODY_READY || work->position > work->scope.count) return -1;
	size_t kept = retained_bindings(work), maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (kept > maximum - 4 || count > maximum - 4 - kept) return -1;
	struct pg_graph scratch;
	if (pg_graph_init(&scratch)) return -1;
	int status = -1;
	const struct pg_term **all = pg_alloc(&scratch, (4 + kept + count) * sizeof(*all));
	if (!all) goto done;
	all[0] = work->scope.source;
	all[1] = work->scope.body;
	all[2] = work->answer;
	all[3] = work->cursor ? work->cursor : work->scope.body;
	for (size_t i = 0; i < kept; ++i) {
		if (!work->scope.bindings) goto done;
		all[4 + i] = pg_reference(&scratch, work->scope.bindings[i].source);
		if (!all[4 + i]) goto done;
	}
	for (size_t i = 0; i < count; ++i) all[4 + kept + i] = roots[i];
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->phase)
		|| pg_wire_write_u64(file, work->scope.count) || pg_wire_write_u64(file, work->position)
		|| pg_wire_write_u64(file, work->cursor != NULL)) goto done;
	status = pg_comparison_write(file, &work->comparison, 4 + kept + count, all, codec, owner);
done:
	pg_graph_destroy(&scratch);
	return status;
}

int pg_action_body_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct action_body_work **work, size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
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
	if (pg_comparison_read(file, output, limit, name_limit, codec, owner,
		&candidate->comparison, &total, &all)) return -1;
	size_t kept = retained_bindings(candidate);
	if (total < 4 || kept > total - 4) goto failure;
	candidate->scope.source = all[0];
	candidate->scope.body = all[1];
	candidate->answer = all[2];
	candidate->cursor = cursor ? all[3] : NULL;
	if (!cursor && all[3] != all[1]) goto failure;
	enum pg_comparison_status compared = pg_comparison_status(&candidate->comparison);
	if (phase == BODY_COMPARE) {
		if (position || cursor || compared == PG_COMPARISON_DIFFERENT) goto failure;
	} else if (phase == BODY_READY) {
		if (position || compared == PG_COMPARISON_PENDING) goto failure;
	} else {
		if (compared != PG_COMPARISON_DIFFERENT || !cursor) goto failure;
		candidate->scope.bindings = pg_alloc(arena, (size_t)arity * sizeof(*candidate->scope.bindings));
		if (!candidate->scope.bindings) goto failure;
	}
	if (phase == BODY_COLLECT && position < arity && all[3]->kind != PG_LAMBDA) goto failure;
	for (size_t i = 0; i < kept; ++i) {
		const struct pg_term *binder = all[4 + i];
		if (binder->kind != PG_REFERENCE || binder->as.reference->kind != PG_BINDER) goto failure;
		candidate->scope.bindings[i].source = binder->as.reference;
	}
	*work = candidate;
	*count = total - 4 - kept;
	*roots = all + 4 + kept;
	return 0;
failure:
	pg_action_body_operation.destroy(candidate);
	return -1;
}
