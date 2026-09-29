#include "source.h"
#include "synthesis_source.h"
#include "dag.h"
#include "wire.h"
#include <string.h>

static const char magic[8] = "APGSRCW1";
enum source_record_kind { DEFINITION, MODULE, LITERAL };

struct source_record {
	enum source_record_kind kind;
	size_t job, count;
	uint64_t *data;
	enum pg_synthesis_status status;
};

struct pg_artifact_source {
	size_t job_count, count;
	struct source_record *records;
};

static int kind(const struct pg_synthesis *s, const struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax, *definitions;
	if (!pg_synthesis_definition_input(s, job, &scope, &definitions, &syntax)) return DEFINITION;
	if (pg_synthesis_source_input(s, job, &scope, &syntax)) return -1;
	if (syntax->kind == PG_SYNTAX_DEFINITIONS) return MODULE;
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_DEFINITIONS) return MODULE;
	struct pg_synthesis_job *rule;
	return pg_synthesis_literal_frontier(s, job, &rule) == 1 ? LITERAL : -1;
}

static uint64_t reference(const struct pg_dag *jobs, const struct pg_synthesis_job *job)
{
	if (!job) return 0;
	const struct pg_dag_node *node = pg_dag_find(jobs, job);
	return node ? node->id : UINT64_MAX;
}

static struct pg_artifact_source *allocate(struct pg_graph *storage, size_t jobs, size_t count)
{
	if (!jobs || count > jobs) return NULL;
	struct pg_artifact_source *c = pg_alloc(storage, sizeof(*c));
	if (!c) return NULL;
	c->job_count = jobs; c->count = count;
	c->records = pg_wire_array(storage, count, sizeof(*c->records));
	return c->records ? c : NULL;
}

const struct pg_artifact_source *pg_artifact_source_capture(struct pg_graph *storage,
	const struct pg_synthesis *s, size_t job_count, struct pg_synthesis_job *const *jobs,
	size_t count, struct pg_synthesis_job *const *owners)
{
	if (!storage || !s || !jobs || (count && !owners)) return NULL;
	struct pg_artifact_source *c = allocate(storage, job_count, count);
	struct pg_dag map = {0}, unique = {0};
	int failed = 1;
	if (!c || pg_dag_init(&map, NULL, NULL) || pg_dag_init(&unique, NULL, NULL)) goto done;
	for (size_t i = 0; i < job_count; ++i) {
		if (!jobs[i] || jobs[i]->owner != s->owner_key) goto done;
		if (pg_dag_add(&map, jobs[i]) || map.count != i + 1) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		struct pg_synthesis_job *job = owners[i], *child = NULL;
		uint64_t slot = reference(&map, job);
		if (!slot || slot > job_count || pg_dag_add(&unique, job) || unique.count != i + 1) goto done;
		int type = kind(s, job);
		if (type < 0 || job->status > PG_SYNTHESIS_DONE) goto done;
		struct source_record *r = &c->records[i];
		*r = (struct source_record){.kind = (enum source_record_kind)type, .job = (size_t)slot - 1, .status = job->status};
		struct pg_module_frontier module;
		switch (r->kind) {
		case MODULE:
			r->count = pg_synthesis_module_frontier(s, job, &module) ? 0 : 3;
			break;
		default: r->count = 1; break;
		}
		r->data = pg_wire_array(storage, r->count, sizeof(*r->data));
		if (!r->data) goto done;
		switch (r->kind) {
		case DEFINITION:
			pg_synthesis_definition_body(s, job, &child);
			r->data[0] = reference(&map, child);
			break;
		case MODULE:
			if (!r->count) break;
			const struct pg_source_scope *scope = pg_synthesis_prepared_environment(job);
			if (!scope) goto done;
			r->data[0] = reference(&map, scope->registration);
			r->data[1] = module.position; r->data[2] = reference(&map, module.previous);
			break;
		case LITERAL:
			if (pg_synthesis_literal_frontier(s, job, &child) != 1) goto done;
			r->data[0] = reference(&map, child);
			break;
		}
		for (size_t j = 0; j < r->count; ++j) if (r->data[j] == UINT64_MAX) goto done;
	}
	failed = 0;
done:
	pg_dag_destroy(&map); pg_dag_destroy(&unique);
	return failed ? NULL : c;
}

int pg_artifact_source_write(FILE *file, const struct pg_artifact_source *c)
{
	if (!file || !c || fwrite(magic, 1, 8, file) != 8) return -1;
	if (pg_wire_write_u64(file, c->job_count) || pg_wire_write_u64(file, c->count)) return -1;
	for (size_t i = 0; i < c->count; ++i) {
		const struct source_record *r = &c->records[i];
		if (pg_wire_write_u64(file, r->kind) || pg_wire_write_u64(file, r->job)
			|| pg_wire_write_u64(file, r->status) || pg_wire_write_u64(file, r->count)) return -1;
		for (size_t j = 0; j < r->count; ++j) if (pg_wire_write_u64(file, r->data[j])) return -1;
	}
	return 0;
}

const struct pg_artifact_source *pg_artifact_source_read(FILE *file, struct pg_graph *storage, size_t limit)
{
	char header[8];
	uint64_t jobs, count;
	if (!file || !storage || fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &jobs) || pg_wire_read_u64(file, &count) || jobs > limit || count > jobs) return NULL;
	struct pg_artifact_source *c = allocate(storage, (size_t)jobs, (size_t)count);
	unsigned char *seen = pg_wire_array(storage, jobs, 1);
	if (!c || !seen) return NULL;
	size_t available = limit;
	for (size_t i = 0; i < c->count; ++i) {
		uint64_t type, slot, status, n;
		if (pg_wire_read_u64(file, &type) || type > LITERAL || pg_wire_read_u64(file, &slot) || slot >= jobs) return NULL;
		if (pg_wire_read_u64(file, &status) || status > PG_SYNTHESIS_DONE) return NULL;
		if (seen[slot]++ || pg_wire_read_u64(file, &n) || n > available) return NULL;
		available -= (size_t)n;
		struct source_record *r = &c->records[i];
		*r = (struct source_record){.kind = (enum source_record_kind)type, .job = (size_t)slot,
			.status = (enum pg_synthesis_status)status, .count = (size_t)n};
		r->data = pg_wire_array(storage, n, sizeof(*r->data));
		if (!r->data) return NULL;
		for (size_t j = 0; j < n; ++j) if (pg_wire_read_u64(file, &r->data[j])) return NULL;
		size_t first = 0;
		switch (r->kind) {
		case MODULE:
			if (n && n != 3) return NULL;
			if (n && (!r->data[0] || r->data[0] > jobs || !r->data[2] || r->data[2] > jobs)) return NULL;
			first = (size_t)n;
			break;
		default: if (n != 1) return NULL; break;
		}
		for (size_t j = first; j < n; ++j) if (r->data[j] > jobs) return NULL;
	}
	return c;
}

static struct pg_synthesis_job *resolve(struct pg_synthesis_job *const *jobs, uint64_t slot)
{
	return slot ? jobs[slot - 1] : NULL;
}

static int mapping_valid(const struct pg_synthesis *s, const struct pg_artifact_source *c,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	if (!s || !c || count != c->job_count || !jobs) return 0;
	for (size_t i = 0; i < count; ++i) if (!jobs[i] || jobs[i]->owner != s->owner_key) return 0;
	for (size_t i = 0; i < c->count; ++i)
		if (kind(s, jobs[c->records[i].job]) != (int)c->records[i].kind) return 0;
	return 1;
}

int pg_artifact_source_prepare(struct pg_synthesis *s, const struct pg_artifact_source *c,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	if (!mapping_valid(s, c, count, jobs)) return -1;
	for (size_t i = 0; i < c->count; ++i) {
		const struct source_record *r = &c->records[i];
		struct pg_synthesis_job *job = jobs[r->job];
		if (job->status != PG_SYNTHESIS_PENDING) return -1;
		if (r->kind == MODULE && r->count && pg_synthesis_prepare_module(s, job) != resolve(jobs, r->data[0])) return -1;
	}
	for (size_t i = 0; i < c->count; ++i) {
		const struct source_record *r = &c->records[i];
		if (r->kind != DEFINITION && r->kind != LITERAL) continue;
		struct pg_synthesis_job *child = resolve(jobs, r->data[0]);
		if (!child) continue;
		if (r->kind == DEFINITION) {
			if (pg_synthesis_definition_resume_body(s, jobs[r->job], child)) return -1;
		} else if (pg_synthesis_prepare_literal(s, jobs[r->job]) != child) return -1;
	}
	return 0;
}

size_t pg_artifact_source_validation(const struct pg_artifact_source *c, size_t *slots)
{
	if (!c || !slots) return 0;
	size_t count = 0;
	for (size_t i = 0; i < c->count; ++i) {
		const struct source_record *r = &c->records[i];
		if (r->status == PG_SYNTHESIS_DONE) slots[count++] = r->job;
	}
	return count;
}

int pg_artifact_source_attach(struct pg_synthesis *s, const struct pg_artifact_source *c,
	size_t count, struct pg_synthesis_job *const *jobs)
{
	if (!mapping_valid(s, c, count, jobs)) return -1;
	for (size_t i = 0; i < c->count; ++i)
		if (jobs[c->records[i].job]->status != c->records[i].status) return -1;
	for (size_t i = 0; i < c->count; ++i) {
		const struct source_record *r = &c->records[i];
		if (r->kind != MODULE || !r->count || r->status != PG_SYNTHESIS_PENDING) continue;
		struct pg_module_frontier view = {r->data[1], resolve(jobs, r->data[2])};
		if (pg_synthesis_module_resume(s, jobs[r->job], &view)) return -1;
	}
	return 0;
}
