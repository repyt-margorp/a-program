#include "program.h"
#include "synthesis_source.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include "artifact/schedule.h"
#include "artifact/file.h"
#include "dag.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Known-origin checkpoints of imported derivation workers. This is not the
 * source checkpoint format: other owner continuations are not encoded here. */
static const char magic[8] = "APGDCT\1";
static int solving;
static size_t checked_cuts, preparation_edges, shared_rules;

void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *s, uint64_t budget)
{
	assert(solving);
	__real_pg_synthesis_advance(s, budget);
}

static void advance(struct pg_synthesis *s, uint64_t budget)
{
	solving = 1;
	pg_synthesis_advance(s, budget);
	solving = 0;
}

struct checkpoint {
	struct pg_program *program;
	struct pg_declaration_io codec;
	size_t raw_count, count;
	struct pg_synthesis_job **jobs;
	const struct pg_derivation_input **inputs;
	uint64_t *next, *prepared, *status;
	const struct pg_artifact_schedule *schedule;
};

static struct pg_program *empty(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	/* The empty lexical scope owns one already checked empty-context premise. */
	assert(p && p->synthesis.jobs.count == 1 && p->typing.proofs.count == 1);
	return p;
}

static void equal_files(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int x, y;
	do { x = fgetc(a); y = fgetc(b); assert(x == y); } while (x != EOF);
	assert(!ferror(a) && !ferror(b));
}

static int input_child(void *owner, const void *key, size_t i, const void **child)
{
	(void)owner;
	const struct pg_derivation_input *input = key;
	if (i == input->count) return 0;
	*child = input->premises[i];
	return 1;
}

static void collect(struct checkpoint *c, struct pg_program *p, struct pg_synthesis_job *root)
{
	*c = (struct checkpoint){.program = p};
	assert(!pg_declaration_io_init(&c->codec, &p->typing));
	struct pg_dag inputs;
	assert(!pg_dag_init(&inputs, input_child, NULL));
	assert(!pg_dag_add(&inputs, pg_synthesis_derivation_input(root)));
	size_t capacity = 2 * inputs.count;
	c->jobs = calloc(capacity, sizeof(*c->jobs));
	c->inputs = calloc(inputs.count, sizeof(*c->inputs));
	c->next = calloc(capacity, sizeof(*c->next));
	c->prepared = calloc(inputs.count, sizeof(*c->prepared));
	c->status = calloc(capacity, sizeof(*c->status));
	assert(c->jobs && c->inputs && c->next && c->prepared && c->status);
	size_t jobs_before = p->synthesis.jobs.count;
	for (const struct pg_dag_node *node = inputs.first; node; node = node->next) {
		const void *key[] = {node->key, root->inputs[1]};
		struct pg_synthesis_job *job = pg_synthesis_work_find(&p->synthesis, root->role, 2, key);
		if (!job) continue;
		c->jobs[c->raw_count] = job;
		c->inputs[c->raw_count++] = node->key;
	}
	c->count = c->raw_count;
	for (size_t i = 0; i < c->raw_count; ++i) {
		size_t next;
		struct pg_synthesis_job *rule;
		assert(!pg_synthesis_derivation_frontier(&p->synthesis, c->jobs[i], &next, &rule));
		c->next[i] = next;
		if (!rule) continue;
		size_t slot = c->raw_count;
		while (slot < c->count && c->jobs[slot] != rule) ++slot;
		if (slot == c->count) c->jobs[c->count++] = rule;
		else ++shared_rules;
		c->prepared[i] = slot + 1;
	}
	for (size_t i = 0; i < c->count; ++i) {
		struct pg_synthesis_job *job = c->jobs[i];
		c->status[i] = job->status;
		assert(job->status == PG_SYNTHESIS_PENDING || job->status == PG_SYNTHESIS_DONE);
		if (job->dependency && job->dependency->preparation) ++preparation_edges;
		if (i < c->raw_count || job->status == PG_SYNTHESIS_DONE) continue;
		size_t next;
		assert(pg_synthesis_rule_frontier(&p->synthesis, job, &next) == 1);
		c->next[i] = next;
	}
	assert(jobs_before == p->synthesis.jobs.count);
	assert(c->count + 1 == p->synthesis.jobs.count);
	pg_dag_destroy(&inputs);
}

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	if (pg_wire_write_u64(file, c->raw_count) || pg_wire_write_u64(file, c->count)) return -1;
	for (size_t i = 0; i < c->count; ++i)
		if (pg_wire_write_u64(file, c->next[i]) || pg_wire_write_u64(file, c->status[i])) return -1;
	for (size_t i = 0; i < c->raw_count; ++i) if (pg_wire_write_u64(file, c->prepared[i])) return -1;
	int status = c->schedule ? pg_artifact_schedule_save(file, c->schedule)
		: pg_artifact_schedule_write(file, &c->program->synthesis, c->count, c->jobs);
	if (status) return status;
	return pg_derivation_inputs_write(file, c->raw_count, c->inputs, codec, &c->codec);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t names,
	const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	uint64_t raw_count, count;
	if (pg_wire_read_u64(file, &raw_count) || pg_wire_read_u64(file, &count)) return -1;
	if (!raw_count || count < raw_count || count > limit) return -1;
	c->count = (size_t)count; c->raw_count = (size_t)raw_count;
	c->jobs = calloc(c->count, sizeof(*c->jobs));
	c->inputs = calloc(c->raw_count, sizeof(*c->inputs));
	c->next = calloc(c->count, sizeof(*c->next));
	c->status = calloc(c->count, sizeof(*c->status));
	c->prepared = calloc(c->raw_count, sizeof(*c->prepared));
	if (!c->jobs || !c->inputs || !c->next || !c->status || !c->prepared) return -1;
	for (size_t i = 0; i < c->count; ++i) {
		if (pg_wire_read_u64(file, &c->next[i]) || c->next[i] > limit) return -1;
		if (pg_wire_read_u64(file, &c->status[i]) || c->status[i] > PG_SYNTHESIS_DONE) return -1;
	}
	for (size_t i = 0; i < c->raw_count; ++i)
		if (pg_wire_read_u64(file, &c->prepared[i]) || c->prepared[i] > count) return -1;
	c->schedule = pg_artifact_schedule_read(file, graph, limit);
	size_t n;
	const struct pg_derivation_input *const *inputs;
	if (!c->schedule || pg_derivations_read_descriptors(file, &c->program->typing, limit, names,
		codec, &c->codec, &n, &inputs) || n != c->raw_count) return -1;
	memcpy(c->inputs, inputs, n * sizeof(*c->inputs));
	return 0;
}

static FILE *save(struct checkpoint *c)
{
	size_t proofs = c->program->typing.proofs.count, jobs = c->program->synthesis.jobs.count;
	size_t terms = c->program->graph.terms.count;
	FILE *file = tmpfile();
	assert(file && !pg_graph_image_write(file, magic, &pg_declaration_graph_codec, &c->codec, write_payload, c));
	assert(proofs == c->program->typing.proofs.count && jobs == c->program->synthesis.jobs.count);
	assert(terms == c->program->graph.terms.count);
	rewind(file);
	return file;
}

static void release(struct checkpoint *c)
{
	pg_declaration_io_destroy(&c->codec);
	free(c->jobs); free(c->inputs); free(c->next); free(c->prepared); free(c->status);
}

static void validation_schedule(struct checkpoint *c)
{
	size_t count = 0;
	for (size_t i = c->raw_count; i < c->count; ++i) count += c->status[i] == PG_SYNTHESIS_DONE;
	FILE *file = tmpfile();
	assert(file && fwrite("APGSCH\1", 1, 8, file) == 8);
	assert(!pg_wire_write_u64(file, c->count) && !pg_wire_write_u64(file, count) && !pg_wire_write_u64(file, 0));
	for (size_t i = c->raw_count; i < c->count; ++i)
		if (c->status[i] == PG_SYNTHESIS_DONE) assert(!pg_wire_write_u64(file, i));
	rewind(file);
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_read(file, &c->program->graph, 100000);
	assert(schedule && !pg_artifact_schedule_attach(&c->program->synthesis, schedule, c->count, c->jobs));
	assert(!fclose(file));
}

static void restore(struct checkpoint *c)
{
	struct pg_synthesis *s = &c->program->synthesis;
	for (size_t i = 0; i < c->raw_count; ++i)
		assert((c->jobs[i] = pg_synthesis_derivation(s, c->inputs[i])));
	for (size_t i = 0; i < c->raw_count; ++i) {
		assert(!pg_synthesis_derivation_resume(s, c->jobs[i], c->next[i], c->prepared[i] != 0));
		size_t next;
		struct pg_synthesis_job *rule;
		assert(!pg_synthesis_derivation_frontier(s, c->jobs[i], &next, &rule));
		if (!c->prepared[i]) { assert(!rule); continue; }
		size_t slot = c->prepared[i] - 1;
		assert(slot >= c->raw_count && slot < c->count);
		assert(!c->jobs[slot] || c->jobs[slot] == rule);
		c->jobs[slot] = rule;
	}
	assert(!s->steps && c->program->typing.proofs.count == 1);
	validation_schedule(c);
	uint64_t budget = 100000;
	for (size_t i = c->raw_count; i < c->count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		uint64_t spent;
		solving = 1;
		enum pg_synthesis_status status = pg_artifact_revalidate(c->program, c->jobs[i], budget, budget, &spent);
		solving = 0;
		assert(status == PG_SYNTHESIS_DONE && spent <= budget);
		budget -= spent;
	}
	assert(!s->ready);
	for (size_t i = 0; i < c->raw_count; ++i) {
		if (c->status[i] != PG_SYNTHESIS_DONE) continue;
		assert(c->prepared[i]);
		assert(pg_synthesis_forward(s, c->jobs[i], c->jobs[c->prepared[i] - 1]));
		assert(pg_synthesis_result(c->jobs[i]));
	}
	for (size_t i = c->raw_count; i < c->count; ++i)
		if (c->status[i] == PG_SYNTHESIS_PENDING) assert(!pg_synthesis_rule_resume(s, c->jobs[i], c->next[i]));
	assert(!pg_artifact_schedule_attach(s, c->schedule, c->count, c->jobs));
}

static FILE *result(struct pg_program *p, struct pg_synthesis_job *root)
{
	const struct pg_evidence *proof = pg_synthesis_result(root);
	struct pg_declaration_io codec;
	assert(proof && !pg_declaration_io_init(&codec, &p->typing));
	FILE *file = tmpfile();
	assert(file && !pg_derivations_write_descriptors(file, &p->typing, 1, &proof, &pg_declaration_graph_codec, &codec));
	pg_declaration_io_destroy(&codec);
	return file;
}

static struct pg_synthesis_job *read_seed(struct pg_program *p, FILE *seed)
{
	struct pg_declaration_io codec;
	assert(!pg_declaration_io_init(&codec, &p->typing));
	size_t count;
	const struct pg_derivation_input *const *inputs;
	rewind(seed);
	assert(!pg_derivations_read_descriptors(seed, &p->typing, 100000, 100000,
		&pg_declaration_graph_codec, &codec, &count, &inputs));
	assert(count == 1);
	struct pg_synthesis_job *root = pg_synthesis_derivation(&p->synthesis, inputs[0]);
	assert(root && p->typing.proofs.count == 1 && !p->synthesis.steps);
	pg_declaration_io_destroy(&codec);
	return root;
}

static void partition(FILE *seed, uint64_t cut)
{
	struct pg_program *p = empty();
	struct pg_synthesis_job *root = read_seed(p, seed);
	advance(&p->synthesis, cut);
	struct checkpoint c;
	collect(&c, p, root);
	FILE *image = save(&c);
	size_t root_slot = 0;
	while (root_slot < c.raw_count && c.jobs[root_slot] != root) ++root_slot;
	assert(root_slot < c.raw_count);
	uint64_t before = p->synthesis.steps;
	advance(&p->synthesis, 100000);
	assert(!p->synthesis.ready);
	uint64_t remaining = p->synthesis.steps - before;
	FILE *expected = result(p, root);
	release(&c);
	pg_program_destroy(p);
	p = empty();
	c = (struct checkpoint){.program = p};
	assert(!pg_declaration_io_init(&c.codec, &p->typing));
	assert(!pg_graph_image_read(image, magic, &p->graph, 100000, 100000,
		&pg_declaration_graph_codec, &c.codec, read_payload, &c));
	assert(p->typing.proofs.count == 1 && p->synthesis.jobs.count == 1);
	FILE *copy = save(&c);
	equal_files(image, copy); assert(!fclose(copy));
	restore(&c);
	before = p->synthesis.steps;
	advance(&p->synthesis, 0);
	assert(p->synthesis.steps == before);
	struct checkpoint actual;
	collect(&actual, p, c.jobs[root_slot]);
	copy = save(&actual);
	equal_files(image, copy); assert(!fclose(copy));
	release(&actual);
	if (remaining) { advance(&p->synthesis, remaining - 1); assert(p->synthesis.ready); }
	advance(&p->synthesis, remaining ? 1 : 0);
	assert(!p->synthesis.ready && p->synthesis.steps - before == remaining);
	copy = result(p, c.jobs[root_slot]);
	equal_files(expected, copy);
	assert(!fclose(copy) && !fclose(expected) && !fclose(image));
	release(&c); pg_program_destroy(p);
	++checked_cuts;
}

static void all_partitions(FILE *seed)
{
	struct pg_program *p = empty();
	struct pg_synthesis_job *root = read_seed(p, seed);
	advance(&p->synthesis, 100000);
	assert(pg_synthesis_result(root) && !p->synthesis.ready);
	uint64_t total = p->synthesis.steps;
	pg_program_destroy(p);
	for (uint64_t cut = 0; cut <= total; ++cut) partition(seed, cut);
	assert(!fclose(seed));
}

static void source(const char *text)
{
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	struct pg_token main = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *root = pg_program_select_name(p, p->root, main);
	assert(root);
	advance(&p->synthesis, 100000);
	FILE *seed = result(p, root);
	pg_program_destroy(p);
	all_partitions(seed);
}

static struct pg_derivation_input *input(struct pg_program *p, enum pg_evidence_rule rule,
	size_t count, const struct pg_derivation_input *const *premises)
{
	struct pg_derivation_input *i = pg_alloc(&p->graph, sizeof(*i) + count * sizeof(*premises));
	assert(i);
	i->rule = rule; i->count = count;
	for (size_t n = 0; n < count; ++n) i->premises[n] = premises[n];
	return i;
}

static void shared(void)
{
	struct pg_program *p = empty();
	const struct pg_derivation_input *c0 = input(p, PG_CONTEXT_EMPTY, 0, NULL);
	const struct pg_derivation_input *c1 = input(p, PG_CONTEXT_EMPTY, 0, NULL);
	const struct pg_derivation_input *u = input(p, PG_UNIVERSE_FORM, 1, &c1);
	const struct pg_derivation_input *pair[] = {c0, u};
	const struct pg_object *binder = pg_binder(&p->graph);
	struct pg_derivation_input *context = input(p, PG_CONTEXT_EXTEND, 2, pair);
	context->parameters.binder = binder;
	const struct pg_derivation_input *premise = context;
	struct pg_derivation_input *variable = input(p, PG_VARIABLE, 1, &premise);
	variable->parameters.binder = binder;
	premise = variable;
	const struct pg_derivation_input *root = input(p, PG_RETURN_INTRO, 1, &premise);
	struct pg_declaration_io codec;
	assert(!pg_declaration_io_init(&codec, &p->typing));
	FILE *file = tmpfile();
	assert(file && !pg_derivation_inputs_write(file, 1, &root, &pg_declaration_graph_codec, &codec));
	pg_declaration_io_destroy(&codec); pg_program_destroy(p);
	all_partitions(file);
}

static void guards(void)
{
	struct pg_program *p = empty(), *other = empty();
	struct pg_synthesis *s = &p->synthesis;
	struct pg_derivation_input *context = pg_alloc(&p->graph, sizeof(*context));
	struct pg_derivation_input *wrong = pg_alloc(&p->graph, sizeof(*wrong) + sizeof(void *));
	assert(context && wrong);
	*context = (struct pg_derivation_input){.rule = PG_CONTEXT_EMPTY};
	*wrong = (struct pg_derivation_input){.rule = PG_RETURN_INTRO, .count = 1};
	wrong->premises[0] = context;
	struct pg_synthesis_job *parent = pg_synthesis_derivation(s, wrong), *child, *rule;
	assert(parent);
	size_t next, count = s->jobs.count;
	struct pg_synthesis_job *ready = s->ready, *tail = s->ready_tail;
	assert(pg_synthesis_derivation_frontier(NULL, parent, &next, &rule));
	assert(pg_synthesis_derivation_frontier(s, NULL, &next, &rule));
	assert(pg_synthesis_derivation_frontier(s, parent, NULL, &rule));
	assert(pg_synthesis_derivation_frontier(s, parent, &next, NULL));
	assert(pg_synthesis_derivation_resume(&other->synthesis, parent, 0, 0));
	assert(pg_synthesis_derivation_resume(s, parent, 2, 0));
	assert(pg_synthesis_derivation_resume(s, parent, 0, 2));
	assert(pg_synthesis_derivation_resume(s, parent, 0, 1));
	assert(pg_synthesis_derivation_resume(s, parent, 1, 0));
	assert(pg_synthesis_derivation_resume(s, parent, 1, 1));
	assert(count == s->jobs.count && ready == s->ready && tail == s->ready_tail);
	assert(!pg_synthesis_derivation_frontier(s, parent, &next, &rule) && !next && !rule);
	child = pg_synthesis_derivation(s, context);
	assert(child && pg_synthesis_derivation_resume(s, parent, 1, 0));
	assert(!pg_synthesis_derivation_resume(s, child, 0, 1));
	assert(!pg_synthesis_derivation_frontier(s, child, &next, &rule) && rule && !next);
	assert(pg_synthesis_derivation_resume(s, rule, 0, 1));
	assert(pg_synthesis_derivation_resume(s, child, 0, 1));
	assert(!pg_synthesis_derivation_resume(s, parent, 1, 1));
	assert(pg_synthesis_derivation_resume(s, parent, 0, 0));
	assert(!s->steps && p->typing.proofs.count == 1 && !pg_synthesis_result(parent));
	/* Preparation can restore a malformed input, but cannot accept it. */
	advance(s, 1000);
	assert(parent->status == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(parent));
	assert(child->status == PG_SYNTHESIS_DONE && pg_synthesis_derivation_resume(s, child, 0, 0));
	pg_program_destroy(other); pg_program_destroy(p);
}

int main(void)
{
	guards();
	source("main := @;");
	source("main := \\x : @ => x;");
	source("main := #int_add;");
	shared();
	assert(checked_cuts && preparation_edges && shared_rules);
	printf("derivation checkpoint: %zu cuts, %zu preparation edges, %zu shared rules\n",
		checked_cuts, preparation_edges, shared_rules);
	return 0;
}
