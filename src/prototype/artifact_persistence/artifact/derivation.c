#include "derivation.h"
#include "../derivation.h"
#include "file.h"
#include "synthesis_source.h"
#include "dag.h"
#include "wire.h"
#include "effect_inference.h"
#include <string.h>

static const char magic[8] = "APGDRC\2";

struct pg_artifact_derivations {
	/* Input adapters precede plain rules; zero adapters means a direct DAG. */
	size_t raw_count, count, root_count;
	const struct pg_derivation_input *const *inputs;
	uint64_t *next, *prepared, *status, *roots;
};

static struct pg_artifact_derivations *allocate(struct pg_graph *storage,
	size_t raw, size_t count, size_t roots)
{
	if (!count || count < raw || (raw && count - raw > raw) || !roots) return NULL;
	struct pg_artifact_derivations *c = pg_alloc(storage, sizeof(*c));
	if (!c) return NULL;
	c->raw_count = raw; c->count = count; c->root_count = roots;
	c->next = pg_wire_array(storage, count, sizeof(*c->next));
	c->status = pg_wire_array(storage, count, sizeof(*c->status));
	c->prepared = pg_wire_array(storage, raw, sizeof(*c->prepared));
	c->roots = pg_wire_array(storage, roots, sizeof(*c->roots));
	return c->next && c->status && c->prepared && c->roots ? c : NULL;
}

static int input_child(void *unused, const void *key, size_t i, const void **child)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (input->effect_parameter) return -1;
	if (i == input->count) return 0;
	*child = input->premises[i];
	return *child ? 1 : -1;
}

/* Direct rule requests already have their premise graph. Do not convert them
 * into imported-input adapters: that would change request identity and fuel. */
static int rule_child(void *owner, const void *key, size_t i, const void **child)
{
	const struct pg_synthesis *const *s = owner;
	const struct pg_synthesis_job *job = key;
	const struct pg_derivation_input *input = pg_synthesis_plain_derivation(job);
	if (!input || job->owner != (*s)->owner_key || job->inputs[1] || input->effect_parameter) return -1;
	if (i == input->count) return 0;
	*child = job->inputs[3 + i];
	return *child ? 1 : -1;
}

static struct pg_artifact_derivations *capture_frontier(struct pg_graph *storage,
	const struct pg_synthesis *s, const struct pg_dag *jobs, size_t raw, size_t count,
	struct pg_synthesis_job *const *roots, struct pg_synthesis_job ***output)
{
	struct pg_artifact_derivations *c = allocate(storage, raw, jobs->count, count);
	struct pg_synthesis_job **mapping = pg_wire_array(storage, jobs->count, sizeof(*mapping));
	const struct pg_derivation_input **headers = pg_wire_array(storage, raw, sizeof(*headers));
	if (!c || !mapping || !headers) return NULL;
	c->inputs = headers;
	for (const struct pg_dag_node *n = jobs->first; n; n = n->next) {
		struct pg_synthesis_job *job = (void *)n->key;
		size_t i = n->id - 1, next;
		if (job->status != PG_SYNTHESIS_PENDING && job->status != PG_SYNTHESIS_DONE) return NULL;
		mapping[i] = job; c->status[i] = job->status;
		if (i < raw) {
			struct pg_synthesis_job *rule;
			headers[i] = pg_synthesis_derivation_input(job);
			if (pg_synthesis_derivation_frontier(s, job, &next, &rule)) return NULL;
			c->next[i] = next;
			c->prepared[i] = rule ? pg_dag_find(jobs, rule)->id : 0;
		} else if (job->status == PG_SYNTHESIS_PENDING) {
			if (pg_synthesis_rule_frontier(s, job, &next) != 1) return NULL;
			c->next[i] = next;
		}
	}
	for (size_t i = 0; i < count; ++i) c->roots[i] = pg_dag_find(jobs, roots[i])->id - 1;
	*output = mapping;
	return c;
}

static const struct pg_artifact_derivations *capture_rules(struct pg_graph *storage,
	const struct pg_synthesis *s, size_t count, struct pg_synthesis_job *const *roots,
	struct pg_synthesis_job ***output)
{
	struct pg_dag jobs;
	if (pg_dag_init(&jobs, rule_child, &s)) return NULL;
	const struct pg_artifact_derivations *result = NULL;
	for (size_t i = 0; i < count; ++i) if (!roots[i] || pg_dag_add(&jobs, roots[i])) goto done;
	struct pg_synthesis_job **mapping;
	struct pg_artifact_derivations *c = capture_frontier(storage, s, &jobs, 0, count, roots, &mapping);
	if (!c) goto done;
	struct pg_effect_inference effects;
	if (pg_effect_inference_init(&effects, storage)) goto done;
	int failed = pg_synthesis_export_rules(s, jobs.count, mapping, storage, &effects, 1, &c->inputs);
	pg_effect_inference_destroy(&effects);
	if (failed) goto done;
	*output = mapping;
	result = c;
done:
	pg_dag_destroy(&jobs);
	return result;
}

static int direct_mapping(const struct pg_synthesis *s, size_t count, struct pg_synthesis_job *const *jobs)
{
	struct pg_dag closure;
	if (pg_dag_init(&closure, rule_child, &s)) return 0;
	int valid = 0;
	for (size_t i = 0; i < count; ++i) {
		if (!jobs[i] || pg_dag_add(&closure, jobs[i])) goto done;
		/* Capture supplies every premise, once, before its consumers. */
		if (closure.count != i + 1 || pg_dag_find(&closure, jobs[i])->id != i + 1) goto done;
	}
	valid = 1;
done:
	pg_dag_destroy(&closure);
	return valid;
}

const struct pg_artifact_derivations *pg_artifact_derivations_capture(struct pg_graph *storage,
	const struct pg_synthesis *s, size_t count, struct pg_synthesis_job *const *roots,
	struct pg_synthesis_job ***output)
{
	if (!storage || !s || !count || !roots || !roots[0] || !output) return NULL;
	if (pg_synthesis_plain_derivation(roots[0])) return capture_rules(storage, s, count, roots, output);
	struct pg_dag inputs = {0}, jobs = {0};
	const struct pg_artifact_derivations *result = NULL;
	if (pg_dag_init(&inputs, input_child, NULL) || pg_dag_init(&jobs, NULL, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_derivation_input *input = pg_synthesis_derivation_input(roots[i]);
		if (!input || roots[i]->owner != s->owner_key || roots[i]->inputs[1] != roots[0]->inputs[1]) goto done;
		if (pg_dag_add(&inputs, input)) goto done;
	}
	for (const struct pg_dag_node *n = inputs.first; n; n = n->next) {
		const void *key[] = {n->key, roots[0]->inputs[1]};
		struct pg_synthesis_job *job = pg_synthesis_work_find(s, roots[0]->role, 2, key);
		if (job && pg_dag_add(&jobs, job)) goto done;
	}
	size_t raw = jobs.count;
	for (const struct pg_dag_node *n = jobs.first; n && n->id <= raw; n = n->next) {
		size_t next;
		struct pg_synthesis_job *rule;
		if (pg_synthesis_derivation_frontier(s, n->key, &next, &rule)) goto done;
		if (rule && pg_dag_add(&jobs, rule)) goto done;
	}
	result = capture_frontier(storage, s, &jobs, raw, count, roots, output);
done:
	pg_dag_destroy(&jobs); pg_dag_destroy(&inputs);
	return result;
}

size_t pg_artifact_derivations_job_count(const struct pg_artifact_derivations *c)
{
	return c ? c->count : 0;
}

const struct pg_derivation_input *const *pg_artifact_derivations_inputs(
	const struct pg_artifact_derivations *c, size_t *count)
{
	if (!c || !count || !c->inputs) return NULL;
	*count = c->raw_count ? c->raw_count : c->count;
	return c->inputs;
}

int pg_artifact_derivations_write(FILE *file, const struct pg_artifact_derivations *c)
{
	if (!file || !c) return -1;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, c->raw_count)
		|| pg_wire_write_u64(file, c->count) || pg_wire_write_u64(file, c->root_count)) return -1;
	for (size_t i = 0; i < c->count; ++i)
		if (pg_wire_write_u64(file, c->next[i]) || pg_wire_write_u64(file, c->status[i])) return -1;
	for (size_t i = 0; i < c->raw_count; ++i) if (pg_wire_write_u64(file, c->prepared[i])) return -1;
	for (size_t i = 0; i < c->root_count; ++i) if (pg_wire_write_u64(file, c->roots[i])) return -1;
	return 0;
}

const struct pg_artifact_derivations *pg_artifact_derivations_read(FILE *file, struct pg_graph *storage, size_t limit)
{
	if (!file || !storage) return NULL;
	char header[8];
	uint64_t raw, count, roots;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &raw) || pg_wire_read_u64(file, &count) || pg_wire_read_u64(file, &roots)) return NULL;
	if (count > limit || raw > count || roots > limit) return NULL;
	struct pg_artifact_derivations *c = allocate(storage, (size_t)raw, (size_t)count, (size_t)roots);
	if (!c) return NULL;
	for (size_t i = 0; i < count; ++i) {
		if (pg_wire_read_u64(file, &c->next[i]) || c->next[i] > limit) return NULL;
		if (pg_wire_read_u64(file, &c->status[i]) || c->status[i] > PG_SYNTHESIS_DONE) return NULL;
		if (i >= raw && c->status[i] == PG_SYNTHESIS_DONE && c->next[i]) return NULL;
	}
	for (size_t i = 0; i < raw; ++i) {
		uint64_t *slot = &c->prepared[i];
		if (pg_wire_read_u64(file, slot) || *slot > count) return NULL;
		if (*slot && *slot <= raw) return NULL;
		if (!*slot && c->status[i] == PG_SYNTHESIS_DONE) return NULL;
	}
	for (size_t i = 0; i < roots; ++i)
		if (pg_wire_read_u64(file, &c->roots[i]) || c->roots[i] >= (raw ? raw : count)) return NULL;
	return c;
}

int pg_artifact_derivations_prepare(struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	size_t count, const struct pg_derivation_input *const *inputs, struct pg_effect_inference *effects,
	struct pg_synthesis_job ***result)
{
	if (!s || !c || count != (c->raw_count ? c->raw_count : c->count) || !inputs || !result) return -1;
	struct pg_synthesis_job **jobs = pg_wire_array(s->typing->graph, c->count, sizeof(*jobs));
	if (!jobs) return -1;
	if (!c->raw_count) {
		for (size_t i = 0; i < count; ++i) if (!inputs[i] || inputs[i]->effect_parameter) return -1;
		struct pg_synthesis_job *const *imported;
		if (pg_synthesis_import_rules(s, count, inputs, effects, &imported)) return -1;
		if (!direct_mapping(s, count, imported)) return -1;
		memcpy(jobs, imported, count * sizeof(*jobs));
		*result = jobs;
		return 0;
	}
	for (size_t i = 0; i < count; ++i) {
		if (!inputs[i] || inputs[i]->effect_parameter) return -1;
		jobs[i] = pg_synthesis_derivation_inference(s, inputs[i], effects);
		if (!jobs[i]) return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		if (pg_synthesis_derivation_resume(s, jobs[i], c->next[i], c->prepared[i] != 0)) return -1;
		if (!c->prepared[i]) continue;
		size_t next, slot = c->prepared[i] - 1;
		struct pg_synthesis_job *rule;
		if (pg_synthesis_derivation_frontier(s, jobs[i], &next, &rule)) return -1;
		if (jobs[slot] && jobs[slot] != rule) return -1;
		jobs[slot] = rule;
	}
	for (size_t i = count; i < c->count; ++i) if (!jobs[i]) return -1;
	*result = jobs;
	return 0;
}

size_t pg_artifact_derivations_validation(const struct pg_artifact_derivations *c, size_t *out)
{
	if (!c || !out) return 0;
	size_t count = 0;
	for (size_t i = c->raw_count; i < c->count; ++i)
		if (c->status[i] == PG_SYNTHESIS_DONE) out[count++] = i;
	return count;
}

static int mapping_valid(const struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	struct pg_synthesis_job *const *jobs)
{
	if (!s || !c || !jobs) return 0;
	for (size_t i = 0; i < c->count; ++i)
		if (!jobs[i] || jobs[i]->owner != s->owner_key) return 0;
	if (!c->raw_count) {
		for (size_t i = 0; i < c->count; ++i)
			if (!pg_synthesis_plain_derivation(jobs[i])) return 0;
	}
	for (size_t i = 0; i < c->raw_count; ++i) {
		size_t next;
		struct pg_synthesis_job *rule;
		if (pg_synthesis_derivation_frontier(s, jobs[i], &next, &rule)) return 0;
		if (next != c->next[i]) return 0;
		if (rule != (c->prepared[i] ? jobs[c->prepared[i] - 1] : NULL)) return 0;
	}
	return 1;
}

enum pg_synthesis_status pg_artifact_derivations_revalidate(struct pg_program *p,
	const struct pg_artifact_derivations *c, struct pg_synthesis_job *const *jobs,
	uint64_t budget, uint64_t limit, uint64_t *spent)
{
	if (!p || !spent || !mapping_valid(&p->synthesis, c, jobs)) return PG_SYNTHESIS_ERROR;
	*spent = 0;
	uint64_t available = budget < limit ? budget : limit;
	for (size_t i = c->raw_count; i < c->count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		uint64_t used = 0;
		enum pg_synthesis_status status = pg_artifact_revalidate(p, jobs[i], available, available, &used);
		*spent += used; available -= used;
		if (status != PG_SYNTHESIS_DONE) return status;
	}
	return PG_SYNTHESIS_DONE;
}

int pg_artifact_derivations_attach(struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	struct pg_synthesis_job *const *jobs)
{
	if (!mapping_valid(s, c, jobs)) return -1;
	for (size_t i = c->raw_count; i < c->count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		if (!pg_synthesis_result(jobs[i])) return -1;
	}
	for (size_t i = 0; i < c->raw_count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		if (!pg_synthesis_forward(s, jobs[i], jobs[c->prepared[i] - 1])) return -1;
	}
	for (size_t i = c->raw_count; i < c->count; ++i)
		if (c->status[i] == PG_SYNTHESIS_PENDING && pg_synthesis_rule_resume(s, jobs[i], c->next[i])) return -1;
	return 0;
}

struct pg_synthesis_job *pg_artifact_derivations_root(const struct pg_artifact_derivations *c,
	struct pg_synthesis_job *const *jobs, size_t index)
{
	return c && jobs && index < c->root_count ? jobs[c->roots[index]] : NULL;
}
