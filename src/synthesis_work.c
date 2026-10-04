#include "synthesis_work.h"
#include "typed_query.h"
#include "effect_inference.h"

const struct pg_synthesis_work_class *pg_synthesis_work_role(const struct pg_synthesis_job *job)
{
	return job ? (const void *)job->pending.role : NULL;
}

struct pg_pending *pg_synthesis_pending(struct pg_synthesis_job *job)
{
	return job ? &job->pending : NULL;
}

static struct pg_synthesis_job *job_owner(struct pg_pending *pending)
{
	return (void *)pending;
}

static struct pg_typed_query *job_query(struct pg_pending *pending)
{
	return pg_pending_query(pg_synthesis_work_output((void *)pending).pending);
}

static const struct pg_evidence *job_result(const struct pg_pending *pending)
{
	return pg_synthesis_result((const void *)pending);
}

const struct pg_pending_ops pg_synthesis_pending_ops = {
	.job = job_owner, .query = job_query, .result = job_result
};

struct resolved_request {
	struct pg_index_entry index;
	struct pg_synthesis_job *owner;
};

int pg_synthesis_yield_query(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_typed_query *query)
{
	if (!pg_typed_query_owned_by(query, synthesis->typing) || query->status) return 0;
	pg_typed_query_advance(query, 1);
	pg_synthesis_enqueue(synthesis, job);
	return 1;
}

int pg_synthesis_await_query(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_typed_query *query)
{
	if (!pg_typed_query_owned_by(query, synthesis->typing)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	if (query->status < 0) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	return pg_synthesis_yield_query(synthesis, job, query);
}

const struct pg_evidence *pg_synthesis_input_result(struct pg_synthesis_input input)
{
	if (input.checked) return input.pending ? NULL : input.checked;
	return pg_pending_result(input.pending);
}

int pg_synthesis_input_owned(const struct pg_synthesis *synthesis, struct pg_synthesis_input input)
{
	if (!synthesis) return 0;
	if (input.checked)
		return !input.pending && pg_evidence_owned_by(input.checked, synthesis->typing);
	struct pg_synthesis_job *job = pg_pending_job(input.pending);
	if (job) {
		if (job->pending.owner != synthesis->owner_key || !synthesis->jobs.capacity) return 0;
		for (const struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, job->pending.index.hash);
			entry; entry = entry->next)
			if (entry == &job->pending.index) return 1;
		return 0;
	}
	struct pg_typed_query *query = pg_pending_query(input.pending);
	if (query) return pg_typed_query_owned_by(query, synthesis->typing);
	return input.pending && input.pending->owner == synthesis->owner_key;
}

int pg_synthesis_await_input(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_input input)
{
	return input.checked ? 0 : pg_synthesis_await_pending(synthesis, job, input.pending);
}

enum pg_synthesis_status pg_synthesis_pending_status(struct pg_pending *pending)
{
	if (!pending) return PG_SYNTHESIS_ERROR;
	struct pg_typed_query *query = pg_pending_query(pending);
	if (query) return !query->status ? PG_SYNTHESIS_PENDING
		: query->status == 1 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED;
	return pg_synthesis_status(pg_pending_job(pending));
}

int pg_synthesis_wait_pending(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_pending *pending)
{
	struct pg_synthesis_job *dependency = pg_pending_job(pending);
	if (dependency && dependency->pending.owner != synthesis->owner_key) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	struct pg_synthesis_input output = pg_synthesis_work_output(dependency);
	if (output.checked) return 0;
	if (output.pending) return pg_synthesis_wait_pending(synthesis, job, output.pending);
	struct pg_typed_query *query = pg_pending_query(pending);
	if (!query) {
		if (!dependency) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
		if (dependency->status != PG_SYNTHESIS_PENDING) return 0;
		pg_synthesis_subscribe(synthesis, job, dependency, 0);
		return 1;
	}
	if (!pg_typed_query_owned_by(query, synthesis->typing)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	return pg_synthesis_yield_query(synthesis, job, query);
}

int pg_synthesis_await_pending(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_pending *pending)
{
	if (pg_synthesis_wait_pending(synthesis, job, pending)) return 1;
	enum pg_synthesis_status status = pg_synthesis_pending_status(pending);
	if (status == PG_SYNTHESIS_DONE) return 0;
	pg_synthesis_finish(synthesis, job, status);
	return 1;
}

struct pg_synthesis_input pg_synthesis_work_dependency(const struct pg_synthesis_job *job, size_t offset)
{
	if (!job || offset >= job->input_count || job->input_count - offset < 2)
		return (struct pg_synthesis_input){0};
	return (struct pg_synthesis_input){job->inputs[offset], (void *)job->inputs[offset + 1]};
}

void pg_synthesis_work_destroy(struct pg_synthesis *synthesis)
{
	/* External owners may outlive this Solve invocation. Disconnect every
	 * borrowed edge before destroying any owner that could notify it. */
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (void *)entry;
			if (job->dependency) pg_subscription_detach(&job->dependency->edge);
			job->dependency = NULL;
		}
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (void *)entry;
			if (pg_synthesis_work_role(job)->destroy) pg_synthesis_work_role(job)->destroy(job);
		}
	pg_index_destroy(&synthesis->jobs);
	pg_index_destroy(&synthesis->resolved_requests);
	synthesis->ready = synthesis->ready_tail = NULL;
	synthesis->free_waiters = NULL;
}

void pg_synthesis_enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	/* Factories can subscribe a request before its initial dispatch. An early
	 * wake must not splice that still-queued node into the FIFO a second time. */
	if (job->next || synthesis->ready_tail == job) return;
	job->next = NULL;
	if (synthesis->ready_tail) synthesis->ready_tail->next = job;
	else synthesis->ready = job;
	synthesis->ready_tail = job;
}

static const void *request_operand(const void *input, size_t index)
{
	const void *const *inputs = input;
	return inputs[index];
}

static size_t state_extent(const struct pg_synthesis_work_class *role)
{
	size_t alignment = _Alignof(struct pg_synthesis_job);
	return (role->size + alignment - 1) / alignment * alignment;
}

static struct pg_synthesis_job *find_request(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, size_t count, const void *key,
	const void *(*operand)(const void *, size_t), uint64_t *hash)
{
	*hash = ((uintptr_t)role ^ count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i)
		*hash = (*hash ^ (uintptr_t)operand(key, i)) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, *hash); entry; entry = entry->next) {
		if (entry->hash != *hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (pg_synthesis_work_role(job) != role) continue;
		if (job->input_count != count) continue;
		size_t i = 0;
		while (i < count && job->inputs[i] == operand(key, i)) ++i;
		if (i == count) return job;
	}
	if (!role->resolved_input || !synthesis->resolved_requests.capacity) return NULL;
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->resolved_requests, *hash);
		entry; entry = entry->next) {
		if (entry->hash != *hash) continue;
		struct pg_synthesis_job *job = ((struct resolved_request *)entry)->owner;
		if (pg_synthesis_work_role(job) != role || job->input_count != count) continue;
		size_t i = 0;
		while (i < count && role->resolved_input(job, i) == operand(key, i)) ++i;
		if (i == count) return job;
	}
	return NULL;
}

static const void *resolved_operand(const void *key, size_t ordinal)
{
	const struct pg_synthesis_job *job = key;
	return pg_synthesis_work_role(job)->resolved_input(job, ordinal);
}

struct pg_synthesis_job *pg_synthesis_work_resolve(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key || !pg_synthesis_work_role(job)->resolved_input) return NULL;
	uint64_t hash;
	struct pg_synthesis_job *existing = find_request(synthesis, pg_synthesis_work_role(job),
		job->input_count, job, resolved_operand, &hash);
	if (existing) return existing;
	if (!synthesis->resolved_requests.capacity && pg_index_init(&synthesis->resolved_requests)) return NULL;
	struct resolved_request *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->owner = job;
	if (pg_index_insert(&synthesis->resolved_requests, &entry->index, hash)) return NULL;
	return job;
}

struct pg_synthesis_job *pg_synthesis_work_find(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, size_t count, const void *const *inputs)
{
	if (!synthesis || !role || (count && !inputs)) return NULL;
	uint64_t hash;
	return find_request(synthesis, role, count, inputs, request_operand, &hash);
}

struct pg_synthesis_job *pg_synthesis_work_request_key(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, size_t count, const void *key,
	const void *(*operand)(const void *, size_t))
{
	if (!synthesis || !role || !role->advance || !operand) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_job)) / sizeof(void *)) return NULL;
	uint64_t hash;
	struct pg_synthesis_job *existing = find_request(synthesis, role, count, key, operand, &hash);
	if (existing) return existing;
	size_t bytes = sizeof(struct pg_synthesis_job) + count * sizeof(void *);
	if (role->size > SIZE_MAX - (_Alignof(struct pg_synthesis_job) - 1)) return NULL;
	size_t offset = state_extent(role);
	if (bytes > SIZE_MAX - offset) return NULL;
	char *storage = pg_alloc(synthesis->typing->graph, offset + bytes);
	if (!storage) return NULL;
	struct pg_synthesis_job *job = (void *)(storage + offset);
	job->pending.owner = synthesis->owner_key;
	job->pending.role = &role->pending;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = operand(key, i);
	if (pg_index_insert(&synthesis->jobs, &job->pending.index, hash) != 0) return NULL;
	if (role->start) role->start(synthesis, job);
	else pg_synthesis_enqueue(synthesis, job);
	return job;
}

struct pg_synthesis_job *pg_synthesis_work_request(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *kind, size_t count, const void *const *inputs)
{
	if (count && !inputs) return NULL;
	return pg_synthesis_work_request_key(synthesis, kind, count, inputs, request_operand);
}

void *pg_synthesis_work_state(const struct pg_synthesis_job *job,
	const struct pg_synthesis_work_class *kind)
{
	return job && pg_synthesis_work_role(job) == kind && kind->size ? (char *)job - state_extent(kind) : NULL;
}

const void *pg_synthesis_work_input(const struct pg_synthesis_job *job, size_t index)
{
	return job && index < job->input_count ? job->inputs[index] : NULL;
}

size_t pg_synthesis_work_input_count(const struct pg_synthesis_job *job)
{
	return job ? job->input_count : 0;
}

struct pg_synthesis_input pg_synthesis_work_output(const struct pg_synthesis_job *job)
{
	if (!job || job->status != PG_SYNTHESIS_DONE) return (struct pg_synthesis_input){0};
	const struct pg_synthesis_work_class *role = pg_synthesis_work_role(job);
	return role->output ? role->output(job) : (struct pg_synthesis_input){0};
}

struct pg_synthesis_projection pg_synthesis_work_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_projection view = {.value_kind = -1};
	if (job && pg_synthesis_work_role(job)->project) view = pg_synthesis_work_role(job)->project(job);
	if (!job || job->status != PG_SYNTHESIS_PENDING) view.preparing = 0;
	return view;
}

struct pg_synthesis_input pg_synthesis_projected_output(const struct pg_synthesis_job *job)
{
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_work_project(job).rule)};
}

static void release_waiter(struct waiter *waiter)
{
	struct pg_synthesis *synthesis = waiter->synthesis;
	pg_subscription_detach(&waiter->edge);
	waiter->parent->dependency = NULL;
	waiter->edge.next = (void *)synthesis->free_waiters;
	synthesis->free_waiters = waiter;
}

static void wake_waiter(struct pg_subscription *edge)
{
	struct waiter *waiter = (void *)edge;
	struct pg_synthesis *synthesis = waiter->synthesis;
	struct pg_synthesis_job *parent = waiter->parent;
	release_waiter(waiter);
	if (parent->status == PG_SYNTHESIS_PENDING) pg_synthesis_enqueue(synthesis, parent);
}

static struct waiter *new_waiter(struct pg_synthesis *synthesis)
{
	struct waiter *waiter = synthesis->free_waiters;
	if (waiter) synthesis->free_waiters = (void *)waiter->edge.next;
	else waiter = pg_alloc(synthesis->typing->graph, sizeof(*waiter));
	if (waiter) *waiter = (struct waiter){0};
	return waiter;
}

int pg_synthesis_prepare_waiters(struct pg_synthesis *synthesis, size_t count)
{
	size_t available = 0;
	for (struct waiter *waiter = synthesis->free_waiters; waiter && available < count;
		waiter = (void *)waiter->edge.next) ++available;
	if (available == count) return 0;
	count -= available;
	if (count > SIZE_MAX / sizeof(struct waiter)) return -1;
	struct waiter *waiters = pg_alloc(synthesis->typing->graph, count * sizeof(*waiters));
	if (!waiters) return -1;
	for (size_t i = 0; i < count; ++i) {
		waiters[i] = (struct waiter){.edge.next = (void *)synthesis->free_waiters};
		synthesis->free_waiters = &waiters[i];
	}
	return 0;
}

void pg_synthesis_unsubscribe(struct pg_synthesis_job *job)
{
	if (job->dependency) release_waiter(job->dependency);
}

static void wake(struct pg_synthesis_job *job, int preparation)
{
	struct pg_subscription *edge = job->waiters;
	while (edge) {
		struct pg_subscription *next = edge->next;
		struct waiter *waiter = (void *)edge;
		if (!preparation || waiter->preparation) {
			pg_subscription_detach(edge);
			edge->notify(edge);
		}
		edge = next;
	}
}

void pg_synthesis_finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	int first_finish = job->status == PG_SYNTHESIS_PENDING;
	pg_synthesis_unsubscribe(job);
	job->status = status;
	if (pg_synthesis_work_role(job)->completed) pg_synthesis_work_role(job)->completed(synthesis, job, first_finish);
	wake(job, 0);
}

void pg_synthesis_subscribe_at(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child, int preparation, struct waiter *waiter)
{
	pg_synthesis_unsubscribe(parent);
	*waiter = (struct waiter){.edge.notify = wake_waiter, .synthesis = synthesis,
		.parent = parent, .child = child, .preparation = preparation};
	pg_subscription_attach(&child->waiters, &waiter->edge);
	parent->dependency = waiter;
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
	struct waiter *waiter = new_waiter(synthesis);
	if (!waiter) { pg_synthesis_finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	pg_synthesis_subscribe_at(synthesis, parent, child, preparation, waiter);
}

int pg_synthesis_await_effects(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_effect_inference *work)
{
	if (!work || work->rows != synthesis->typing->graph || work->failed) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	if (!work->sealed) {
		struct waiter *waiter = job->dependency;
		if (!waiter) waiter = new_waiter(synthesis);
		if (!waiter) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
		pg_subscription_detach(&waiter->edge);
		*waiter = (struct waiter){.edge.notify = wake_waiter, .synthesis = synthesis, .parent = job};
		pg_subscription_attach(&work->waiters, &waiter->edge);
		job->dependency = waiter;
		return 1;
	}
	if (pg_effect_inference_advance(work, 0)) return 0;
	/* The canonical equation owner advances once, including a completing
	 * transition. No mirrored DONE/result state is retained by a wrapper. */
	if (pg_effect_inference_advance(work, 1) < 0) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	else pg_synthesis_enqueue(synthesis, job);
	return 1;
}

/* Zero means the dependency is done. Otherwise suspend or propagate its
 * failure without interpreting its payload or changing the caller's stage. */
int pg_synthesis_await(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *dependency)
{
	return pg_synthesis_await_pending(synthesis, job, pg_synthesis_pending(dependency));
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
		pg_synthesis_work_role(job)->advance(synthesis, job);
		if (job->waiters && !pg_synthesis_work_project(job).preparing)
			wake(job, 1);
	}
}

enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job)
{
	if (!job) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis_input output = pg_synthesis_work_output(job);
	if (output.checked) return PG_SYNTHESIS_DONE;
	if (output.pending) return pg_synthesis_pending_status(output.pending);
	return job->status;
}

enum pg_synthesis_status pg_synthesis_advance_pending(struct pg_synthesis *synthesis,
	struct pg_pending *pending, uint64_t budget)
{
	if (!pg_synthesis_input_owned(synthesis, (struct pg_synthesis_input){.pending = pending}))
		return PG_SYNTHESIS_ERROR;
	while (budget && pg_synthesis_pending_status(pending) == PG_SYNTHESIS_PENDING) {
		struct pg_typed_query *query = pg_pending_query(pending);
		if (query) {
			++synthesis->steps;
			pg_typed_query_advance(query, 1);
		} else {
			if (!synthesis->ready) break;
			pg_synthesis_advance(synthesis, 1);
		}
		--budget;
	}
	return pg_synthesis_pending_status(pending);
}

const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job)
{
	if (!job) return NULL;
	struct pg_synthesis_input output = pg_synthesis_work_output(job);
	if (output.checked || output.pending) return pg_synthesis_input_result(output);
	return job->status == PG_SYNTHESIS_DONE ? job->result : NULL;
}

int pg_synthesis_materialized(const struct pg_synthesis_job *job,
	const struct pg_occurrence **root)
{
	if (!job || !root) return -1;
	const struct pg_evidence *proof = pg_synthesis_result(job);
	const struct pg_occurrence *subject = proof ? pg_evidence_subject(proof) : NULL;
	*root = subject ? subject : job->imported;
	return *root != NULL;
}

static int owns_entry(const struct pg_index *index, const struct pg_index_entry *entry)
{
	for (const struct pg_index_entry *candidate = pg_index_candidates(index, entry->hash);
		candidate; candidate = candidate->next)
		if (candidate == entry) return 1;
	return 0;
}

int pg_synthesis_import_materialized(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_occurrence *root)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key || !root) return -1;
	if (!owns_entry(&synthesis->typing->occurrences, &root->index)) return -1;
	if (job->imported && job->imported != root) return -1;
	job->imported = root;
	return 0;
}

int pg_synthesis_saved_complete(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return 0;
	if (job->status == PG_SYNTHESIS_DONE) return 1;
	return !synthesis->steps && job->imported_complete;
}

int pg_synthesis_import_completion(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return -1;
	if (synthesis->steps) return -1;
	job->imported_complete = 1;
	return 0;
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
	if (pg_synthesis_wait_pending(synthesis, job, pg_synthesis_pending(canonical))) return 1;
	if (!pg_synthesis_work_role(job)->output) job->result = pg_synthesis_result(canonical);
	pg_synthesis_finish(synthesis, job, pg_synthesis_status(canonical));
	return 1;
}
