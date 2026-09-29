#include "schedule.h"
#include "synthesis_work.h"
#include "dag.h"
#include "wire.h"
#include <stdlib.h>
#include <string.h>

static const char magic[8] = "APGSCH\1";

/* Inert wire ordinals. They are not a second live scheduler or proof state. */
struct pg_artifact_schedule {
	size_t count, ready, waiting;
	uint64_t *queue, *edges;
};

static struct pg_artifact_schedule *allocate(struct pg_graph *graph, size_t count, size_t ready, size_t waiting)
{
	if (ready > count || waiting > count - ready || waiting > SIZE_MAX / 3) return NULL;
	struct pg_artifact_schedule *state = pg_alloc(graph, sizeof(*state));
	if (!state) return NULL;
	state->count = count; state->ready = ready; state->waiting = waiting;
	state->queue = pg_wire_array(graph, ready, sizeof(*state->queue));
	state->edges = pg_wire_array(graph, waiting * 3, sizeof(*state->edges));
	return state->queue && state->edges ? state : NULL;
}

int pg_artifact_schedule_save(FILE *file, const struct pg_artifact_schedule *state)
{
	if (!file || !state) return -1;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, state->count)
		|| pg_wire_write_u64(file, state->ready) || pg_wire_write_u64(file, state->waiting)) return -1;
	for (size_t i = 0; i < state->ready; ++i) if (pg_wire_write_u64(file, state->queue[i])) return -1;
	for (size_t i = 0; i < 3 * state->waiting; ++i) if (pg_wire_write_u64(file, state->edges[i])) return -1;
	return 0;
}

const struct pg_artifact_schedule *pg_artifact_schedule_read(FILE *file, struct pg_graph *graph, size_t limit)
{
	if (!file || !graph) return NULL;
	char header[8];
	uint64_t count, ready, waiting;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &count) || count > limit || pg_wire_read_u64(file, &ready)
		|| ready > count || pg_wire_read_u64(file, &waiting) || waiting > count - ready) return NULL;
	struct pg_artifact_schedule *state = allocate(graph, (size_t)count, (size_t)ready, (size_t)waiting);
	if (!state) return NULL;
	unsigned char *marks = calloc(count ? (size_t)count : 1, 1);
	if (!marks) return NULL;
	int valid = 0;
	for (size_t i = 0; i < ready; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || id >= count || marks[id]) goto done;
		state->queue[i] = id; marks[id] = 1;
	}
	for (size_t i = 0; i < waiting; ++i) {
		uint64_t *edge = &state->edges[3 * i];
		if (pg_wire_read_u64(file, &edge[0]) || edge[0] >= count || marks[edge[0]]
			|| pg_wire_read_u64(file, &edge[1]) || edge[1] >= count
			|| pg_wire_read_u64(file, &edge[2]) || edge[2] > 1) goto done;
		if (i && edge[1] < state->edges[3 * (i - 1) + 1]) goto done;
		marks[edge[0]] = 1;
	}
	valid = 1;
done:
	free(marks);
	return valid ? state : NULL;
}

static int job_map(struct pg_dag *map, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	if (!synthesis || (count && !jobs)) return -1;
	for (size_t i = 0; i < count; ++i) {
		if (!jobs[i] || jobs[i]->owner != synthesis->owner_key) return -1;
		if (pg_dag_add(map, jobs[i]) || map->count != i + 1) return -1;
	}
	return 0;
}

static size_t slot(const struct pg_dag *map, const struct pg_synthesis_job *job)
{
	const struct pg_dag_node *node = pg_dag_find(map, job);
	return node ? node->id - 1 : SIZE_MAX;
}

/* A partial closure must not silently discard another consumer's subscription
 * or runnable work. Marks also bound traversal of an inconsistent linked list. */
static int inspect(const struct pg_synthesis *synthesis, const struct pg_dag *map,
	struct pg_synthesis_job *const *jobs, unsigned char *marks, size_t *ready, size_t *waiting, int replacing)
{
	*ready = *waiting = 0;
	memset(marks, 0, map->count);
	const struct pg_synthesis_job *last = NULL;
	for (const struct pg_synthesis_job *job = synthesis->ready; job; job = job->next) {
		size_t i = slot(map, job);
		if (i == SIZE_MAX || marks[i] || job->status != PG_SYNTHESIS_PENDING) return -1;
		marks[i] = 1;
		++*ready;
		last = job;
	}
	if (last != synthesis->ready_tail) return -1;
	for (size_t i = 0; i < map->count; ++i) {
		const struct pg_synthesis_job *child = jobs[i];
		for (const struct waiter *w = child->waiters; w; w = w->next) {
			size_t p = slot(map, w->parent);
			if (p == SIZE_MAX || (marks[p] & 2) || w->child != child) return -1;
			if (!replacing && marks[p]) return -1;
			if (w->parent->dependency != w || w->parent->status != PG_SYNTHESIS_PENDING) return -1;
			if (child->status != PG_SYNTHESIS_PENDING || (unsigned)w->preparation > 1) return -1;
			marks[p] |= 2;
			++*waiting;
		}
	}
	for (size_t i = 0; i < map->count; ++i) {
		if ((jobs[i]->dependency != NULL) != ((marks[i] & 2) != 0)) return -1;
		if (!(marks[i] & 1) && jobs[i]->next) return -1;
	}
	return 0;
}

int pg_artifact_schedule_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	struct pg_dag map;
	if (!file || pg_dag_init(&map, NULL, NULL)) return -1;
	int result = -1;
	if (job_map(&map, synthesis, count, jobs)) goto done;
	unsigned char *marks = pg_alloc(&map.storage, count);
	size_t ready, waiting;
	if (!marks || inspect(synthesis, &map, jobs, marks, &ready, &waiting, 0)) goto done;
	struct pg_artifact_schedule *state = allocate(&map.storage, count, ready, waiting);
	if (!state) goto done;
	size_t q = 0, e = 0;
	for (const struct pg_synthesis_job *job = synthesis->ready; job; job = job->next)
		state->queue[q++] = slot(&map, job);
	for (size_t i = 0; i < count; ++i)
		for (const struct waiter *w = jobs[i]->waiters; w; w = w->next) {
			state->edges[e++] = slot(&map, w->parent);
			state->edges[e++] = i;
			state->edges[e++] = (unsigned)w->preparation;
		}
	result = pg_artifact_schedule_save(file, state);
done:
	pg_dag_destroy(&map);
	return result;
}

int pg_artifact_schedule_attach(struct pg_synthesis *synthesis, const struct pg_artifact_schedule *state,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	struct pg_dag map;
	if (!state || count != state->count || pg_dag_init(&map, NULL, NULL)) return -1;
	int result = -1;
	if (job_map(&map, synthesis, count, jobs)) goto done;
	unsigned char *marks = pg_alloc(&map.storage, count);
	size_t previous_ready, previous_waiting;
	/* Source recipe reconstruction can subscribe a newly enqueued request
	 * before its initial dispatch. Replace that unpublished queue as a whole;
	 * the saved queue remains disjoint from its dependency parents. */
	if (!marks || inspect(synthesis, &map, jobs, marks, &previous_ready, &previous_waiting, 1)) goto done;
	for (size_t i = 0; i < state->ready; ++i)
		if (jobs[state->queue[i]]->status != PG_SYNTHESIS_PENDING) goto done;
	struct waiter *edges = pg_wire_array(synthesis->typing->graph, state->waiting, sizeof(*edges));
	if (!edges) goto done;
	for (size_t i = 0; i < state->waiting; ++i) {
		const uint64_t *edge = &state->edges[3 * i];
		struct pg_synthesis_job *parent = jobs[edge[0]], *child = jobs[edge[1]];
		if (parent->status != PG_SYNTHESIS_PENDING || child->status != PG_SYNTHESIS_PENDING) goto done;
		edges[i] = (struct waiter){.parent = parent, .child = child, .preparation = (int)edge[2]};
	}
	/* Publish only after every mapping and edge has passed validation. */
	for (size_t i = 0; i < count; ++i) {
		jobs[i]->next = NULL;
		jobs[i]->waiters = jobs[i]->dependency = NULL;
	}
	synthesis->ready = synthesis->ready_tail = NULL;
	for (size_t i = 0; i < state->ready; ++i) pg_synthesis_enqueue(synthesis, jobs[state->queue[i]]);
	for (size_t i = state->waiting; i-- > 0;) {
		struct waiter *w = &edges[i];
		w->next = w->child->waiters;
		w->child->waiters = w;
		w->parent->dependency = w;
	}
	result = 0;
done:
	pg_dag_destroy(&map);
	return result;
}
