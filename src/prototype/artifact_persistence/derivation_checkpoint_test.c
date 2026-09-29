#include "program.h"
#include "synthesis_source.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include "artifact/derivation.h"
#include "artifact/schedule.h"
#include "dag.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Exercise the artifact-owned codec, not a test-owned restoration engine.
 * Source continuation integration remains a separate, non-passing gate. */
static const char magic[8] = "APGDCT\1";
static int solving;
static size_t checked_cuts, direct_cuts, preparation_edges, shared_rules;

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
	const struct pg_artifact_derivations *state;
	const struct pg_artifact_schedule *schedule;
	size_t count;
	const struct pg_derivation_input *const *inputs;
	struct pg_synthesis_job **jobs;
};

static struct pg_program *empty(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
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

static void collect(struct checkpoint *c, struct pg_program *p, struct pg_synthesis_job *root)
{
	*c = (struct checkpoint){.program = p};
	assert(!pg_declaration_io_init(&c->codec, &p->typing));
	struct pg_synthesis_job *roots[] = {root, root};
	c->state = pg_artifact_derivations_capture(&p->graph, &p->synthesis, 2, roots, &c->jobs);
	assert(c->state);
	c->schedule = pg_artifact_schedule_capture(&p->graph, &p->synthesis,
		pg_artifact_derivations_job_count(c->state), c->jobs);
	assert(c->schedule);
	c->inputs = pg_artifact_derivations_inputs(c->state, &c->count);
	assert(c->inputs && c->count);
	struct pg_dag seen;
	assert(!pg_dag_init(&seen, NULL, NULL));
	for (size_t i = 0; i < p->synthesis.jobs.capacity; ++i)
		for (struct pg_index_entry *e = p->synthesis.jobs.buckets[i]; e; e = e->next) {
			struct pg_synthesis_job *job = (void *)e;
			if (job->dependency && job->dependency->preparation) ++preparation_edges;
			if (!pg_synthesis_derivation_input(job)) continue;
			struct pg_synthesis_job *rule = pg_synthesis_work_project(job).rule;
			if (!rule) continue;
			if (pg_dag_find(&seen, rule)) ++shared_rules;
			assert(!pg_dag_add(&seen, rule));
		}
	pg_dag_destroy(&seen);
}

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	if (pg_artifact_derivations_write(file, c->state)) return -1;
	if (pg_artifact_schedule_save(file, c->schedule)) return -1;
	return pg_derivation_inputs_write(file, c->count, c->inputs, codec, &c->codec);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t names,
	const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	c->state = pg_artifact_derivations_read(file, graph, limit);
	if (!c->state) return -1;
	c->schedule = pg_artifact_schedule_read(file, graph, limit);
	if (!c->schedule) return -1;
	return pg_derivations_read_descriptors(file, &c->program->typing, limit, names,
		codec, &c->codec, &c->count, &c->inputs);
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

static void validation_schedule(struct pg_program *p, const struct pg_artifact_derivations *c,
	struct pg_synthesis_job **jobs)
{
	size_t count = pg_artifact_derivations_job_count(c);
	size_t *targets = calloc(count, sizeof(*targets));
	assert(targets);
	size_t ready = pg_artifact_derivations_validation(c, targets);
	const struct pg_artifact_schedule *queue = pg_artifact_schedule_ready(&p->graph, count, ready, targets);
	assert(queue && !pg_artifact_schedule_attach(&p->synthesis, queue, count, jobs));
	free(targets);
}

static void restore(struct checkpoint *c)
{
	struct pg_synthesis *s = &c->program->synthesis;
	assert(!pg_artifact_derivations_prepare(s, c->state, c->count, c->inputs, NULL, &c->jobs));
	assert(s->jobs.count == pg_artifact_derivations_job_count(c->state) + 1);
	validation_schedule(c->program, c->state, c->jobs);
	assert(!s->steps && c->program->typing.proofs.count == 1);
	uint64_t total = 100000, used = UINT64_MAX;
	solving = 1;
	enum pg_synthesis_status status = pg_artifact_derivations_revalidate(c->program, c->state, c->jobs, total, 0, &used);
	assert(!used && !s->steps);
	if (status == PG_SYNTHESIS_PENDING) assert(pg_artifact_derivations_attach(s, c->state, c->jobs));
	while (status == PG_SYNTHESIS_PENDING) {
		assert(total);
		status = pg_artifact_derivations_revalidate(c->program, c->state, c->jobs, 1, total, &used);
		assert(used == 1);
		total -= used;
	}
	solving = 0;
	assert(status == PG_SYNTHESIS_DONE && !s->ready);
	assert(!pg_artifact_derivations_attach(s, c->state, c->jobs));
	assert(!pg_artifact_schedule_attach(s, c->schedule, pg_artifact_derivations_job_count(c->state), c->jobs));
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

static struct pg_synthesis_job *read_seed(struct pg_program *p, FILE *seed, int direct)
{
	struct pg_declaration_io codec;
	assert(!pg_declaration_io_init(&codec, &p->typing));
	size_t count;
	const struct pg_derivation_input *const *inputs;
	rewind(seed);
	assert(!pg_derivations_read_descriptors(seed, &p->typing, 100000, 100000,
		&pg_declaration_graph_codec, &codec, &count, &inputs));
	assert(count == 1);
	struct pg_synthesis_job *root;
	if (direct) {
		struct pg_synthesis_job *const *rules;
		assert(!pg_synthesis_import_rules(&p->synthesis, count, inputs, NULL, &rules));
		root = rules[0];
	} else root = pg_synthesis_derivation(&p->synthesis, inputs[0]);
	assert(root && p->typing.proofs.count == 1 && !p->synthesis.steps);
	pg_declaration_io_destroy(&codec);
	return root;
}

static void partition(FILE *seed, uint64_t cut, int direct)
{
	struct pg_program *p = empty();
	struct pg_synthesis_job *root = read_seed(p, seed, direct);
	advance(&p->synthesis, cut);
	struct checkpoint c;
	collect(&c, p, root);
	FILE *image = save(&c);
	uint64_t before = p->synthesis.steps;
	advance(&p->synthesis, 100000);
	assert(!p->synthesis.ready);
	uint64_t remaining = p->synthesis.steps - before;
	FILE *expected = result(p, root);
	pg_declaration_io_destroy(&c.codec); pg_program_destroy(p);
	p = empty();
	c = (struct checkpoint){.program = p};
	assert(!pg_declaration_io_init(&c.codec, &p->typing));
	assert(!pg_graph_image_read(image, magic, &p->graph, 100000, 100000,
		&pg_declaration_graph_codec, &c.codec, read_payload, &c));
	assert(p->typing.proofs.count == 1 && p->synthesis.jobs.count == 1);
	FILE *copy = save(&c);
	equal_files(image, copy); assert(!fclose(copy));
	restore(&c);
	root = pg_artifact_derivations_root(c.state, c.jobs, 0);
	assert(root && root == pg_artifact_derivations_root(c.state, c.jobs, 1));
	assert(!pg_artifact_derivations_root(c.state, c.jobs, 2));
	before = p->synthesis.steps;
	advance(&p->synthesis, 0);
	assert(p->synthesis.steps == before);
	struct checkpoint actual;
	collect(&actual, p, root);
	copy = save(&actual);
	equal_files(image, copy); assert(!fclose(copy));
	pg_declaration_io_destroy(&actual.codec);
	if (remaining) { advance(&p->synthesis, remaining - 1); assert(p->synthesis.ready); }
	advance(&p->synthesis, remaining ? 1 : 0);
	assert(!p->synthesis.ready && p->synthesis.steps - before == remaining);
	copy = result(p, root);
	equal_files(expected, copy);
	assert(!fclose(copy) && !fclose(expected) && !fclose(image));
	pg_declaration_io_destroy(&c.codec); pg_program_destroy(p);
	++checked_cuts;
	direct_cuts += direct;
}

static void all_partitions(FILE *seed)
{
	FILE *expected = NULL;
	for (int direct = 0; direct < 2; ++direct) {
		struct pg_program *p = empty();
		struct pg_synthesis_job *root = read_seed(p, seed, direct);
		advance(&p->synthesis, 100000);
		assert(pg_synthesis_result(root) && !p->synthesis.ready);
		uint64_t total = p->synthesis.steps;
		FILE *actual = result(p, root);
		if (expected) { equal_files(expected, actual); assert(!fclose(actual)); }
		else expected = actual;
		pg_program_destroy(p);
		for (uint64_t cut = 0; cut <= total; ++cut) partition(seed, cut, direct);
	}
	assert(!fclose(expected) && !fclose(seed));
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

static FILE *metadata_copy(const unsigned char *bytes, size_t count)
{
	FILE *file = tmpfile();
	assert(file && fwrite(bytes, 1, count, file) == count);
	rewind(file);
	return file;
}

static void codec_boundaries(void)
{
	struct pg_program *p = empty();
	struct pg_derivation_input *header = input(p, PG_CONTEXT_EMPTY, 0, NULL);
	struct pg_synthesis_job *root = pg_synthesis_derivation(&p->synthesis, header);
	advance(&p->synthesis, 1);
	struct pg_synthesis_job **captured;
	const struct pg_artifact_derivations *c = pg_artifact_derivations_capture(&p->graph, &p->synthesis, 1, &root, &captured);
	assert(c);
	FILE *file = tmpfile();
	assert(file && !pg_artifact_derivations_write(file, c));
	long length = ftell(file);
	assert(length == 80);
	unsigned char *bytes = malloc((size_t)length);
	assert(bytes);
	rewind(file);
	assert(fread(bytes, 1, (size_t)length, file) == (size_t)length && !fclose(file));
	for (size_t cut = 0; cut < (size_t)length; ++cut) {
		file = metadata_copy(bytes, cut);
		assert(!pg_artifact_derivations_read(file, &p->graph, 100));
		assert(!fclose(file));
	}
	const uint64_t invalid[][2] = {{8, 0}, {16, 0}, {24, 0}, {40, 2}, {64, 1}, {72, 1}};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		file = metadata_copy(bytes, (size_t)length);
		assert(!fseek(file, (long)invalid[i][0], SEEK_SET) && !pg_wire_write_u64(file, invalid[i][1]));
		rewind(file);
		assert(!pg_artifact_derivations_read(file, &p->graph, 100));
		assert(!fclose(file));
	}
	free(bytes);
	struct pg_synthesis_job *ready = p->synthesis.ready;
	size_t duplicate[] = {0, 0}, absent[] = {2};
	assert(!pg_artifact_schedule_ready(&p->graph, 2, 2, duplicate));
	assert(!pg_artifact_schedule_ready(&p->graph, 2, 1, absent));
	assert(ready == p->synthesis.ready);
	const struct pg_derivation_input *premise = header;
	struct pg_synthesis_job *other = pg_synthesis_derivation(&p->synthesis, input(p, PG_UNIVERSE_FORM, 1, &premise));
	assert(other && pg_artifact_derivations_capture(&p->graph, &p->synthesis, 1, &root, &captured));
	assert(!pg_artifact_schedule_capture(&p->graph, &p->synthesis,
		pg_artifact_derivations_job_count(c), captured));
	/* The owner payload composes with other work; the global schedule must
	 * still include every runnable consumer. */
	struct pg_synthesis_job *roots[] = {other, root, other};
	c = pg_artifact_derivations_capture(&p->graph, &p->synthesis, 3, roots, &captured);
	assert(c);
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_capture(&p->graph,
		&p->synthesis, pg_artifact_derivations_job_count(c), captured);
	assert(schedule);
	size_t count;
	const struct pg_derivation_input *const *inputs = pg_artifact_derivations_inputs(c, &count);
	struct pg_program *q = empty();
	struct pg_synthesis_job **jobs = NULL;
	assert(pg_artifact_derivations_prepare(&q->synthesis, c, count - 1, inputs, NULL, &jobs) && !jobs);
	assert(!pg_artifact_derivations_prepare(&q->synthesis, c, count, inputs, NULL, &jobs));
	validation_schedule(q, c, jobs);
	struct pg_synthesis_job *first = jobs[0];
	jobs[0] = jobs[1];
	assert(pg_artifact_derivations_attach(&q->synthesis, c, jobs));
	jobs[0] = first;
	uint64_t spent = UINT64_MAX;
	solving = 1;
	assert(pg_artifact_derivations_revalidate(q, c, jobs, 0, 100, &spent) == PG_SYNTHESIS_DONE && !spent);
	assert(pg_artifact_derivations_revalidate(p, c, jobs, 100, 100, &spent) == PG_SYNTHESIS_ERROR);
	solving = 0;
	assert(pg_artifact_derivations_attach(&p->synthesis, c, jobs));
	assert(!pg_artifact_derivations_attach(&q->synthesis, c, jobs));
	assert(!pg_artifact_schedule_attach(&q->synthesis, schedule, pg_artifact_derivations_job_count(c), jobs));
	advance(&q->synthesis, 100);
	assert(pg_synthesis_result(pg_artifact_derivations_root(c, jobs, 0)));
	assert(pg_synthesis_result(pg_artifact_derivations_root(c, jobs, 1)));
	assert(pg_artifact_derivations_root(c, jobs, 0) == pg_artifact_derivations_root(c, jobs, 2));
	pg_program_destroy(q); pg_program_destroy(p);
}

static void invalid_completed_assertion(int direct)
{
	struct pg_program *p = empty(), *q = empty();
	const struct pg_derivation_input *context = input(p, PG_CONTEXT_EMPTY, 0, NULL);
	const struct pg_derivation_input *invalid = input(p, PG_RETURN_INTRO, 1, &context);
	struct pg_synthesis_job *root;
	if (direct) {
		struct pg_synthesis_job *const *rules;
		assert(!pg_synthesis_import_rules(&p->synthesis, 1, &invalid, NULL, &rules));
		root = rules[0];
	} else {
		root = pg_synthesis_derivation(&p->synthesis, invalid);
		for (unsigned i = 0; !pg_synthesis_work_project(root).rule; ++i) {
			assert(i < 100);
			advance(&p->synthesis, 1);
		}
	}
	struct pg_synthesis_job **captured;
	const struct pg_artifact_derivations *c = pg_artifact_derivations_capture(&p->graph, &p->synthesis, 1, &root, &captured);
	assert(c);
	size_t count;
	const struct pg_derivation_input *const *inputs = pg_artifact_derivations_inputs(c, &count);
	FILE *file = tmpfile();
	assert(file && !pg_artifact_derivations_write(file, c));
	uint64_t jobs_count;
	assert(!fseek(file, 16, SEEK_SET) && !pg_wire_read_u64(file, &jobs_count));
	for (size_t i = 0; i < jobs_count; ++i) {
		assert(!fseek(file, (long)(40 + 16 * i), SEEK_SET));
		assert(!pg_wire_write_u64(file, PG_SYNTHESIS_DONE));
	}
	rewind(file);
	c = pg_artifact_derivations_read(file, &q->graph, 100);
	assert(c && !fclose(file));
	struct pg_synthesis_job **jobs;
	assert(!pg_artifact_derivations_prepare(&q->synthesis, c, count, inputs, NULL, &jobs));
	validation_schedule(q, c, jobs);
	assert(pg_artifact_derivations_attach(&q->synthesis, c, jobs));
	uint64_t spent;
	solving = 1;
	assert(pg_artifact_derivations_revalidate(q, c, jobs, 100, 100, &spent) == PG_SYNTHESIS_REJECTED);
	solving = 0;
	assert(spent && spent <= 100 && pg_artifact_derivations_attach(&q->synthesis, c, jobs));
	assert(!pg_synthesis_result(pg_artifact_derivations_root(c, jobs, 0)));
	pg_program_destroy(q); pg_program_destroy(p);
}

static void direct_boundaries(void)
{
	struct pg_program *p = empty(), *other = empty();
	struct pg_synthesis_job *context = pg_synthesis_plain_rule(&p->synthesis, PG_CONTEXT_EMPTY, NULL, 0, NULL);
	struct pg_synthesis_job *root = pg_synthesis_plain_rule(&p->synthesis, PG_UNIVERSE_FORM, NULL, 1, &context);
	struct pg_synthesis_job *bad[] = {root, NULL};
	struct pg_synthesis_job **mapping = NULL;
	assert(!pg_artifact_derivations_capture(&p->graph, &p->synthesis, 2, bad, &mapping) && !mapping);
	bad[1] = pg_synthesis_plain_rule(&other->synthesis, PG_CONTEXT_EMPTY, NULL, 0, NULL);
	assert(!pg_artifact_derivations_capture(&p->graph, &p->synthesis, 2, bad, &mapping) && !mapping);
	/* A completed external owner is not silently replaced by a fresh rule. */
	bad[1] = pg_synthesis_evidence(&p->synthesis, pg_prove_empty_context(&p->typing));
	bad[0] = pg_synthesis_plain_rule(&p->synthesis, PG_UNIVERSE_FORM, NULL, 1, &bad[1]);
	assert(!pg_artifact_derivations_capture(&p->graph, &p->synthesis, 1, bad, &mapping) && !mapping);
	const struct pg_artifact_derivations *c = pg_artifact_derivations_capture(&p->graph, &p->synthesis, 1, &root, &mapping);
	assert(c && pg_artifact_derivations_job_count(c) == 2);
	size_t count;
	const struct pg_derivation_input *const *inputs = pg_artifact_derivations_inputs(c, &count);
	assert(count == 2);
	const struct pg_derivation_input *invalid[] = {inputs[0], inputs[0]};
	struct pg_synthesis_job **output = NULL;
	assert(pg_artifact_derivations_prepare(&other->synthesis, c, 2, invalid, NULL, &output) && !output);
	invalid[0] = inputs[1]; invalid[1] = inputs[0];
	assert(pg_artifact_derivations_prepare(&other->synthesis, c, 2, invalid, NULL, &output) && !output);
	assert(!other->synthesis.steps && other->typing.proofs.count == 1);
	FILE *file = tmpfile();
	assert(file && !pg_artifact_derivations_write(file, c));
	assert(!fseek(file, 6, SEEK_SET) && fputc(1, file) != EOF);
	rewind(file);
	assert(!pg_artifact_derivations_read(file, &other->graph, 100));
	assert(!fclose(file));
	pg_program_destroy(other); pg_program_destroy(p);
}

int main(void)
{
	guards();
	codec_boundaries();
	invalid_completed_assertion(0);
	invalid_completed_assertion(1);
	direct_boundaries();
	source("main := @;");
	source("main := \\x : @ => x;");
	source("main := #int_add;");
	shared();
	assert(checked_cuts && direct_cuts && preparation_edges && shared_rules);
	printf("derivation checkpoint: %zu cuts (%zu direct), %zu preparation edges, %zu shared rules\n",
		checked_cuts, direct_cuts, preparation_edges, shared_rules);
	return 0;
}
