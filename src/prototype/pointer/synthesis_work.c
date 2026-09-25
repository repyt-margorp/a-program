#include "synthesis_work.h"

struct waiter {
	struct pg_synthesis_job *parent;
	struct pg_synthesis_job *child;
	struct waiter *next;
	int preparation;
};

void pg_synthesis_work_destroy(struct pg_synthesis *synthesis)
{
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (void *)entry;
			if (job->role->destroy) job->role->destroy(job);
		}
	pg_index_destroy(&synthesis->jobs);
	synthesis->ready = synthesis->ready_tail = NULL;
}

void pg_synthesis_enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	job->next = NULL;
	if (synthesis->ready_tail) synthesis->ready_tail->next = job;
	else synthesis->ready = job;
	synthesis->ready_tail = job;
}

/* Read the caller's fixed prefix and dependency array without assembling a
 * temporary key. Stored requests still own one flat immutable input array. */
static const void *request_operand(size_t prefix, const void *const *inputs,
	struct pg_synthesis_job *const *jobs, size_t index)
{
	return index < prefix ? inputs[index] : jobs[index - prefix];
}

static size_t state_extent(const struct pg_synthesis_work_class *role)
{
	size_t alignment = _Alignof(struct pg_synthesis_job);
	return (role->size + alignment - 1) / alignment * alignment;
}

struct pg_synthesis_job *pg_synthesis_work_request_inputs(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, size_t prefix, const void *const *inputs,
	size_t job_count, struct pg_synthesis_job *const *jobs)
{
	if (!role || !role->advance || (prefix && !inputs) || (job_count && !jobs)) return NULL;
	if (job_count > SIZE_MAX - prefix) return NULL;
	size_t count = prefix + job_count;
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_job)) / sizeof(*inputs)) return NULL;
	uint64_t hash = ((uintptr_t)role ^ count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i)
		hash = (hash ^ (uintptr_t)request_operand(prefix, inputs, jobs, i)) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (job->role != role) continue;
		if (job->input_count != count) continue;
		size_t i = 0;
		while (i < count && job->inputs[i] == request_operand(prefix, inputs, jobs, i)) ++i;
		if (i == count) return job;
	}
	size_t bytes = sizeof(struct pg_synthesis_job) + count * sizeof(*inputs);
	if (role->size > SIZE_MAX - (_Alignof(struct pg_synthesis_job) - 1)) return NULL;
	size_t offset = state_extent(role);
	if (bytes > SIZE_MAX - offset) return NULL;
	char *storage = pg_alloc(synthesis->typing->graph, offset + bytes);
	if (!storage) return NULL;
	struct pg_synthesis_job *job = (void *)(storage + offset);
	job->owner = synthesis->owner_key;
	job->role = role;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = request_operand(prefix, inputs, jobs, i);
	if (pg_index_insert(&synthesis->jobs, &job->index, hash) != 0) return NULL;
	if (role->start) role->start(synthesis, job);
	else pg_synthesis_enqueue(synthesis, job);
	return job;
}

struct pg_synthesis_job *pg_synthesis_work_request(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *kind, size_t count, const void *const *inputs)
{
	return pg_synthesis_work_request_inputs(synthesis, kind, count, inputs, 0, NULL);
}

void *pg_synthesis_work_state(const struct pg_synthesis_job *job,
	const struct pg_synthesis_work_class *kind)
{
	return job && job->role == kind && kind->size ? (char *)job - state_extent(kind) : NULL;
}

const void *pg_synthesis_work_input(const struct pg_synthesis_job *job, size_t index)
{
	return job && index < job->input_count ? job->inputs[index] : NULL;
}

size_t pg_synthesis_work_input_count(const struct pg_synthesis_job *job)
{
	return job ? job->input_count : 0;
}

struct pg_synthesis_projection pg_synthesis_work_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_projection view = {.value_kind = -1};
	if (job && job->role->project) view = job->role->project(job);
	if (!job || job->status != PG_SYNTHESIS_PENDING) view.preparing = 0;
	return view;
}

static void wake(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int preparation)
{
	struct waiter **link = &job->waiters;
	while (*link) {
		struct waiter *waiter = *link;
		if (preparation && !waiter->preparation) { link = &waiter->next; continue; }
		*link = waiter->next;
		waiter->parent->dependency = NULL;
		pg_synthesis_enqueue(synthesis, waiter->parent);
	}
}

void pg_synthesis_finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	int first_finish = job->status == PG_SYNTHESIS_PENDING;
	job->status = status;
	if (job->role->completed) job->role->completed(synthesis, job, first_finish);
	wake(synthesis, job, 0);
}

/* Subscribe exactly once at a stage transition. Completed dependencies need
 * no subscription; pending dependencies wake their consumers on completion. */
void pg_synthesis_subscribe(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child, int preparation)
{
	if (!child) { pg_synthesis_finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	if (child->status != PG_SYNTHESIS_PENDING) {
		pg_synthesis_enqueue(synthesis, parent);
		return;
	}
	struct waiter *waiter = pg_alloc(synthesis->typing->graph, sizeof(*waiter));
	if (!waiter) { pg_synthesis_finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	*waiter = (struct waiter){parent, child, child->waiters, preparation};
	child->waiters = waiter;
	parent->dependency = waiter;
}

/* Zero means the dependency is done. Otherwise suspend or propagate its
 * failure without interpreting its payload or changing the caller's stage. */
int pg_synthesis_await(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *dependency)
{
	if (!dependency) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	if (dependency->status == PG_SYNTHESIS_DONE) return 0;
	if (dependency->status == PG_SYNTHESIS_PENDING) pg_synthesis_subscribe(synthesis, job, dependency, 0);
	else pg_synthesis_finish(synthesis, job, dependency->status);
	return 1;
}

void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget)
{
	while (synthesis->ready && budget) {
		struct pg_synthesis_job *job = synthesis->ready;
		synthesis->ready = job->next;
		if (!synthesis->ready) synthesis->ready_tail = NULL;
		job->next = NULL;
		--budget;
		++synthesis->steps;
		job->role->advance(synthesis, job);
		if (job->waiters && !pg_synthesis_work_project(job).preparing)
			wake(synthesis, job, 1);
	}
}

enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job) { return job->status; }

const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job)
{
	return job->status == PG_SYNTHESIS_DONE ? job->result : NULL;
}

const struct pg_synthesis_job *pg_synthesis_dependency(const struct pg_synthesis_job *job)
{
	return job && job->dependency ? job->dependency->child : NULL;
}

const struct pg_synthesis_job *pg_synthesis_cycle(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *slow = job, *fast = job;
	do {
		slow = pg_synthesis_dependency(slow);
		fast = pg_synthesis_dependency(pg_synthesis_dependency(fast));
		if (!slow || !fast) return NULL;
	} while (slow != fast);
	return slow;
}

/* Forward proof-result jobs only; schema/namespace outputs have other payloads. */
int pg_synthesis_forward(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *canonical)
{
	if (!canonical) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	if (canonical == job) return 0;
	if (canonical->status == PG_SYNTHESIS_PENDING) pg_synthesis_subscribe(synthesis, job, canonical, 0);
	else {
		job->result = canonical->result;
		pg_synthesis_finish(synthesis, job, canonical->status);
	}
	return 1;
}
