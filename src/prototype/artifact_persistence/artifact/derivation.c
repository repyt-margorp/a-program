#include "derivation.h"
#include "../derivation.h"
#include "file.h"
#include "synthesis_source.h"
#include "dag.h"
#include "wire.h"
#include "effect_inference.h"
#include <string.h>

static const char magic[8] = "APGDRC\3";

struct premise_links {
	size_t count;
	uint64_t *slots;
};

struct pg_artifact_derivations {
	/* External jobs precede direct rules, or input adapters precede rules. */
	size_t external_count, raw_count, count, root_count;
	const struct pg_derivation_input *const *inputs;
	uint64_t *next, *prepared, *status, *roots;
	struct premise_links *links;
};

static struct pg_artifact_derivations *allocate(struct pg_graph *storage,
	size_t external, size_t raw, size_t count, size_t roots)
{
	if (external >= count || count < raw || (raw && external) || (raw && count - raw > raw) || !roots) return NULL;
	struct pg_artifact_derivations *c = pg_alloc(storage, sizeof(*c));
	if (!c) return NULL;
	c->external_count = external; c->raw_count = raw; c->count = count; c->root_count = roots;
	c->next = pg_wire_array(storage, count, sizeof(*c->next));
	c->status = pg_wire_array(storage, count, sizeof(*c->status));
	c->prepared = pg_wire_array(storage, raw, sizeof(*c->prepared));
	c->roots = pg_wire_array(storage, roots, sizeof(*c->roots));
	c->links = pg_wire_array(storage, raw ? 0 : count, sizeof(*c->links));
	return c->next && c->status && c->prepared && c->roots && c->links ? c : NULL;
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
struct rule_closure {
	const struct pg_synthesis *synthesis;
	struct pg_dag external;
};

static int rule_child(void *owner, const void *key, size_t i, const void **child)
{
	const struct rule_closure *closure = owner;
	const struct pg_synthesis_job *job = key;
	if (pg_dag_find(&closure->external, job)) return 0;
	const struct pg_derivation_input *input = pg_synthesis_plain_derivation(job);
	if (!input || job->owner != closure->synthesis->owner_key || job->inputs[1] || input->effect_parameter) return -1;
	if (i == input->count) return 0;
	*child = job->inputs[3 + i];
	return *child ? 1 : -1;
}

static struct pg_artifact_derivations *capture_frontier(struct pg_graph *storage,
	const struct pg_synthesis *s, const struct pg_dag *jobs, size_t external, size_t raw, size_t count,
	struct pg_synthesis_job *const *roots, struct pg_synthesis_job ***output)
{
	struct pg_artifact_derivations *c = allocate(storage, external, raw, jobs->count, count);
	struct pg_synthesis_job **mapping = pg_wire_array(storage, jobs->count, sizeof(*mapping));
	const struct pg_derivation_input **headers = pg_wire_array(storage, raw ? raw : jobs->count - external, sizeof(*headers));
	if (!c || !mapping || !headers) return NULL;
	c->inputs = headers;
	for (const struct pg_dag_node *n = jobs->first; n; n = n->next) {
		struct pg_synthesis_job *job = (void *)n->key;
		size_t i = n->id - 1, next;
		mapping[i] = job;
		if (i < external) continue;
		if (job->status != PG_SYNTHESIS_PENDING && job->status != PG_SYNTHESIS_DONE) return NULL;
		c->status[i] = job->status;
		if (!raw) {
			const struct pg_derivation_input *header = pg_synthesis_plain_derivation(job);
			headers[i - external] = header;
			c->links[i].count = header->count;
			c->links[i].slots = pg_wire_array(storage, header->count, sizeof(uint64_t));
			if (!c->links[i].slots) return NULL;
			for (size_t k = 0; k < header->count; ++k)
				c->links[i].slots[k] = pg_dag_find(jobs, job->inputs[3 + k])->id - 1;
		}
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
	size_t external_count, struct pg_synthesis_job *const *external,
	struct pg_synthesis_job ***output)
{
	struct rule_closure closure = {.synthesis = s};
	struct pg_dag jobs = {0};
	const struct pg_artifact_derivations *result = NULL;
	if (pg_dag_init(&closure.external, NULL, NULL) || pg_dag_init(&jobs, rule_child, &closure)) goto done;
	for (size_t i = 0; i < external_count; ++i) {
		if (!external[i] || external[i]->owner != s->owner_key) goto done;
		if (pg_dag_add(&closure.external, external[i]) || closure.external.count != i + 1) goto done;
		if (pg_dag_add(&jobs, external[i])) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		if (!roots[i] || pg_dag_find(&closure.external, roots[i])) goto done;
		if (pg_dag_add(&jobs, roots[i])) goto done;
	}
	struct pg_synthesis_job **mapping;
	struct pg_artifact_derivations *c = capture_frontier(storage, s, &jobs, external_count, 0, count, roots, &mapping);
	if (!c) goto done;
	*output = mapping;
	result = c;
done:
	pg_dag_destroy(&jobs); pg_dag_destroy(&closure.external);
	return result;
}

const struct pg_artifact_derivations *pg_artifact_derivations_capture(struct pg_graph *storage,
	const struct pg_synthesis *s, size_t count, struct pg_synthesis_job *const *roots,
	struct pg_synthesis_job ***output)
{
	return pg_artifact_derivations_capture_with_dependencies(storage, s, count, roots, 0, NULL, output);
}

const struct pg_artifact_derivations *pg_artifact_derivations_capture_with_dependencies(struct pg_graph *storage,
	const struct pg_synthesis *s, size_t count, struct pg_synthesis_job *const *roots,
	size_t external_count, struct pg_synthesis_job *const *external, struct pg_synthesis_job ***output)
{
	if (!storage || !s || !count || !roots || !roots[0] || !output) return NULL;
	if (external_count && !external) return NULL;
	if (pg_synthesis_plain_derivation(roots[0])) return capture_rules(storage, s, count, roots, external_count, external, output);
	if (external_count) return NULL;
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
	result = capture_frontier(storage, s, &jobs, 0, raw, count, roots, output);
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
	*count = c->raw_count ? c->raw_count : c->count - c->external_count;
	return c->inputs;
}

int pg_artifact_derivations_are_headers(const struct pg_artifact_derivations *c)
{
	return c && !c->raw_count;
}

int pg_artifact_derivations_write(FILE *file, const struct pg_artifact_derivations *c)
{
	if (!file || !c) return -1;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, c->raw_count)
		|| pg_wire_write_u64(file, c->count) || pg_wire_write_u64(file, c->root_count)
		|| pg_wire_write_u64(file, c->external_count)) return -1;
	for (size_t i = c->external_count; i < c->count; ++i) {
		if (pg_wire_write_u64(file, c->next[i]) || pg_wire_write_u64(file, c->status[i])) return -1;
		if (c->raw_count) continue;
		if (pg_wire_write_u64(file, c->links[i].count)) return -1;
		for (size_t k = 0; k < c->links[i].count; ++k)
			if (pg_wire_write_u64(file, c->links[i].slots[k])) return -1;
	}
	for (size_t i = 0; i < c->raw_count; ++i) if (pg_wire_write_u64(file, c->prepared[i])) return -1;
	for (size_t i = 0; i < c->root_count; ++i) if (pg_wire_write_u64(file, c->roots[i])) return -1;
	return 0;
}

const struct pg_artifact_derivations *pg_artifact_derivations_read(FILE *file, struct pg_graph *storage, size_t limit)
{
	if (!file || !storage) return NULL;
	char header[8];
	uint64_t raw, count, roots, external;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &raw) || pg_wire_read_u64(file, &count) || pg_wire_read_u64(file, &roots)) return NULL;
	if (pg_wire_read_u64(file, &external)) return NULL;
	if (count > limit || raw > count || roots > limit || external >= count) return NULL;
	struct pg_artifact_derivations *c = allocate(storage, (size_t)external, (size_t)raw, (size_t)count, (size_t)roots);
	if (!c) return NULL;
	size_t available = limit - (size_t)count;
	for (size_t i = (size_t)external; i < count; ++i) {
		if (pg_wire_read_u64(file, &c->next[i]) || c->next[i] > limit) return NULL;
		if (pg_wire_read_u64(file, &c->status[i]) || c->status[i] > PG_SYNTHESIS_DONE) return NULL;
		if (i >= raw && c->status[i] == PG_SYNTHESIS_DONE && c->next[i]) return NULL;
		if (raw) continue;
		uint64_t arity;
		if (pg_wire_read_u64(file, &arity) || arity > available) return NULL;
		available -= (size_t)arity;
		c->links[i].count = (size_t)arity;
		c->links[i].slots = pg_wire_array(storage, arity, sizeof(uint64_t));
		if (!c->links[i].slots || c->next[i] > arity) return NULL;
		for (size_t k = 0; k < arity; ++k)
			if (pg_wire_read_u64(file, &c->links[i].slots[k]) || c->links[i].slots[k] >= i) return NULL;
	}
	for (size_t i = 0; i < raw; ++i) {
		uint64_t *slot = &c->prepared[i];
		if (pg_wire_read_u64(file, slot) || *slot > count) return NULL;
		if (*slot && *slot <= raw) return NULL;
		if (!*slot && c->status[i] == PG_SYNTHESIS_DONE) return NULL;
	}
	for (size_t i = 0; i < roots; ++i)
		if (pg_wire_read_u64(file, &c->roots[i]) || c->roots[i] < external || c->roots[i] >= (raw ? raw : count)) return NULL;
	return c;
}

int pg_artifact_derivations_prepare(struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	size_t count, const struct pg_derivation_input *const *inputs, struct pg_effect_inference *effects,
	struct pg_synthesis_job ***result)
{
	return pg_artifact_derivations_prepare_with_dependencies(s, c, count, inputs, effects, 0, NULL, result);
}

static int prepare_rules(struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	const struct pg_derivation_input *const *inputs, struct pg_synthesis_job *const *external,
	struct pg_synthesis_job **jobs)
{
	struct pg_dag unique;
	if (pg_dag_init(&unique, NULL, NULL)) return -1;
	int status = -1;
	for (size_t i = 0; i < c->count; ++i) {
		if (i < c->external_count) jobs[i] = external[i];
		else {
			const struct pg_derivation_input *header = inputs[i - c->external_count];
			const struct premise_links *links = &c->links[i];
			if (!header || header->effect_parameter || header->count != links->count) goto done;
			struct pg_synthesis_job **premises = pg_wire_array(&unique.storage, links->count, sizeof(*premises));
			if (!premises) goto done;
			for (size_t k = 0; k < links->count; ++k) premises[k] = jobs[links->slots[k]];
			jobs[i] = pg_synthesis_rule(s, header, premises, NULL, NULL);
		}
		if (!jobs[i] || jobs[i]->owner != s->owner_key) goto done;
		if (pg_dag_add(&unique, jobs[i]) || unique.count != i + 1) goto done;
	}
	status = 0;
done:
	pg_dag_destroy(&unique);
	return status;
}

int pg_artifact_derivations_prepare_with_dependencies(struct pg_synthesis *s, const struct pg_artifact_derivations *c,
	size_t count, const struct pg_derivation_input *const *inputs, struct pg_effect_inference *effects,
	size_t external_count, struct pg_synthesis_job *const *external, struct pg_synthesis_job ***result)
{
	if (!s || !c || count != (c->raw_count ? c->raw_count : c->count - c->external_count) || !inputs || !result) return -1;
	if (external_count != c->external_count || (external_count && !external)) return -1;
	struct pg_synthesis_job **jobs = pg_wire_array(s->typing->graph, c->count, sizeof(*jobs));
	if (!jobs) return -1;
	if (!c->raw_count) {
		if (prepare_rules(s, c, inputs, external, jobs)) return -1;
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
	for (size_t i = c->external_count + c->raw_count; i < c->count; ++i)
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
		for (size_t i = c->external_count; i < c->count; ++i) {
			const struct pg_derivation_input *header = pg_synthesis_plain_derivation(jobs[i]);
			if (!header || header->count != c->links[i].count) return 0;
			for (size_t k = 0; k < header->count; ++k)
				if (jobs[i]->inputs[3 + k] != jobs[c->links[i].slots[k]]) return 0;
		}
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
	for (size_t i = c->external_count + c->raw_count; i < c->count; ++i) {
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
	for (size_t i = c->external_count + c->raw_count; i < c->count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		if (!pg_synthesis_result(jobs[i])) return -1;
	}
	for (size_t i = 0; i < c->raw_count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		if (!pg_synthesis_forward(s, jobs[i], jobs[c->prepared[i] - 1])) return -1;
	}
	for (size_t i = c->external_count + c->raw_count; i < c->count; ++i)
		if (c->status[i] == PG_SYNTHESIS_PENDING && pg_synthesis_rule_resume(s, jobs[i], c->next[i])) return -1;
	return 0;
}

struct pg_synthesis_job *pg_artifact_derivations_root(const struct pg_artifact_derivations *c,
	struct pg_synthesis_job *const *jobs, size_t index)
{
	return c && jobs && index < c->root_count ? jobs[c->roots[index]] : NULL;
}
