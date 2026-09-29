#include "program.h"
#include "source_io.h"
#include "synthesis_source.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include "artifact/source.h"
#include "artifact/derivation.h"
#include "artifact/schedule.h"
#include "artifact/file.h"
#include "dag.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Compose real source-input I/O with owner-local continuations. This test
 * envelope supports closed literal modules, not arbitrary source checkpoints. */
static int solving;
static size_t cuts;
static uint64_t verification;
void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *s, uint64_t fuel)
{
	assert(solving);
	__real_pg_synthesis_advance(s, fuel);
}

static void advance(struct pg_synthesis *s, uint64_t fuel)
{
	solving = 1; pg_synthesis_advance(s, fuel); solving = 0;
}

struct checkpoint {
	struct pg_program *program;
	size_t count, prefix_count, source_count, input_count;
	struct pg_synthesis_job **jobs;
	struct pg_synthesis_job **sources;
	uint64_t *recipes;
	const struct pg_artifact_source *source;
	const struct pg_artifact_derivations *rules;
	const struct pg_artifact_schedule *schedule;
	const struct pg_derivation_input *const *inputs;
};

static void add(struct pg_dag *dag, const void *key)
{
	if (key) assert(!pg_dag_add(dag, key));
}

static int registration(const struct pg_synthesis *s, struct pg_synthesis_job *job)
{
	struct pg_definition_frontier view;
	return !pg_synthesis_definition_frontier(s, job, &view);
}

static struct checkpoint collect(struct pg_program *p)
{
	struct checkpoint c = {.program = p};
	struct pg_dag prefix, roots;
	assert(!pg_dag_init(&prefix, NULL, NULL) && !pg_dag_init(&roots, NULL, NULL));
	add(&prefix, p->scope->context_job); add(&prefix, p->root);
	for (const struct pg_dag_node *n = prefix.first->next; n; n = n->next) {
		struct pg_synthesis_job *job = (void *)n->key, *child = NULL;
		const struct pg_source_scope *scope = pg_synthesis_prepared_environment(job);
		if (scope) add(&prefix, scope->registration);
		struct pg_definition_frontier names;
		if (!pg_synthesis_definition_frontier(&p->synthesis, job, &names))
			for (size_t i = 0; i < names.count; ++i) add(&prefix, names.entries[i]);
		struct pg_module_frontier module;
		if (!pg_synthesis_module_frontier(&p->synthesis, job, &module)) add(&prefix, module.previous);
		if (!pg_synthesis_definition_body(&p->synthesis, job, &child)) add(&prefix, child);
		if (pg_synthesis_literal_frontier(&p->synthesis, job, &child) == 1) add(&roots, child);
	}
	c.prefix_count = prefix.count;
	struct pg_synthesis_job **external = pg_wire_array(&p->graph, prefix.count, sizeof(*external));
	c.sources = pg_wire_array(&p->graph, prefix.count, sizeof(*c.sources));
	c.recipes = pg_wire_array(&p->graph, 2 * prefix.count, sizeof(*c.recipes));
	assert(external && c.sources && c.recipes);
	for (const struct pg_dag_node *n = prefix.first; n; n = n->next) {
		struct pg_synthesis_job *job = (void *)n->key;
		external[n->id - 1] = job;
		if (n->id == 1) continue;
		if (registration(&p->synthesis, job)) {
			c.recipes[2 * (n->id - 1)] = 1;
			for (const struct pg_dag_node *anchor = prefix.first->next; anchor; anchor = anchor->next) {
				if (anchor == n || registration(&p->synthesis, (void *)anchor->key)) continue;
				const struct pg_source_scope *scope = pg_synthesis_prepared_environment(anchor->key);
				if (scope && scope->registration == job) { c.recipes[2 * (n->id - 1) + 1] = anchor->id; break; }
			}
			assert(c.recipes[2 * (n->id - 1) + 1]);
		} else {
			c.sources[c.source_count++] = job;
			c.recipes[2 * (n->id - 1)] = 2;
			c.recipes[2 * (n->id - 1) + 1] = c.source_count;
		}
	}
	c.jobs = external; c.count = c.prefix_count;
	if (roots.count) {
		struct pg_synthesis_job **rule_roots = pg_wire_array(&p->graph, roots.count, sizeof(*rule_roots));
		assert(rule_roots);
		for (const struct pg_dag_node *n = roots.first; n; n = n->next) rule_roots[n->id - 1] = (void *)n->key;
		c.rules = pg_artifact_derivations_capture_with_dependencies(&p->graph, &p->synthesis,
			roots.count, rule_roots, c.prefix_count, external, &c.jobs);
		assert(c.rules);
		c.count = pg_artifact_derivations_job_count(c.rules);
		c.inputs = pg_artifact_derivations_inputs(c.rules, &c.input_count);
	}
	c.source = pg_artifact_source_capture(&p->graph, &p->synthesis, c.count, c.jobs, c.source_count, c.sources);
	assert(c.source && c.count == p->synthesis.jobs.count);
	c.schedule = pg_artifact_schedule_capture(&p->graph, &p->synthesis, c.count, c.jobs);
	assert(c.schedule);
	pg_dag_destroy(&prefix); pg_dag_destroy(&roots);
	return c;
}

static int write_continuation(FILE *file, const struct pg_graph_codec *codec, void *object_owner, void *owner)
{
	struct checkpoint *c = owner;
	assert(!pg_wire_write_u64(file, c->prefix_count));
	for (size_t i = 0; i < 2 * c->prefix_count; ++i) assert(!pg_wire_write_u64(file, c->recipes[i]));
	assert(!pg_artifact_source_write(file, c->source));
	assert(!pg_wire_write_u64(file, c->rules != NULL));
	if (c->rules) {
		assert(!pg_artifact_derivations_write(file, c->rules));
		assert(!pg_derivation_headers_write(file, c->input_count, c->inputs, codec, object_owner));
	}
	assert(!pg_artifact_schedule_save(file, c->schedule));
	return 0;
}

static FILE *save(struct checkpoint *c)
{
	struct pg_program *p = c->program;
	size_t terms = p->graph.terms.count, proofs = p->typing.proofs.count, jobs = p->synthesis.jobs.count;
	FILE *file = tmpfile();
	assert(file && !pg_sources_write_with(file, &p->synthesis, c->source_count, c->sources, 0, write_continuation, c));
	assert(terms == p->graph.terms.count && proofs == p->typing.proofs.count && jobs == p->synthesis.jobs.count);
	rewind(file);
	return file;
}

static int read_continuation(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *object_owner, void *owner)
{
	struct checkpoint *c = owner;
	uint64_t n, rules;
	assert(!pg_wire_read_u64(file, &n) && n && n < 10000);
	c->prefix_count = (size_t)n;
	c->recipes = pg_wire_array(typing->graph, 2 * n, sizeof(*c->recipes));
	assert(c->recipes);
	for (size_t i = 0; i < 2 * n; ++i) assert(!pg_wire_read_u64(file, &c->recipes[i]));
	c->source = pg_artifact_source_read(file, typing->graph, limit);
	assert(c->source && !pg_wire_read_u64(file, &rules) && rules <= 1);
	if (rules) {
		c->rules = pg_artifact_derivations_read(file, typing->graph, limit);
		assert(c->rules && !pg_derivation_headers_read(file, typing, limit, name_limit,
			codec, object_owner, &c->input_count, &c->inputs));
	}
	c->count = rules ? pg_artifact_derivations_job_count(c->rules) : c->prefix_count;
	c->schedule = pg_artifact_schedule_read(file, typing->graph, limit);
	assert(c->schedule);
	return 0;
}

static struct checkpoint read_image(FILE *file)
{
	struct checkpoint c = {0};
	struct pg_synthesis_job *const *sources;
	c.program = pg_sources_read_with(file, 100000, &c.source_count, &sources, read_continuation, &c);
	struct pg_program *p = c.program;
	assert(p && !p->synthesis.steps && p->typing.proofs.count == 1);
	c.sources = pg_wire_array(&p->graph, c.source_count, sizeof(*c.sources));
	assert(c.sources); memcpy(c.sources, sources, c.source_count * sizeof(*c.sources));
	size_t n = c.prefix_count;
	c.jobs = pg_wire_array(&p->graph, n, sizeof(*c.jobs));
	assert(c.jobs);
	for (size_t i = 0; i < n; ++i) {
		uint64_t tag = c.recipes[2 * i], index = c.recipes[2 * i + 1];
		if (!tag) { assert(!i && !index); c.jobs[i] = p->scope->context_job; }
		else if (tag == 2) { assert(index && index <= c.source_count); c.jobs[i] = c.sources[index - 1]; }
		else assert(tag == 1);
	}
	for (size_t i = 0; i < n; ++i) if (c.recipes[2 * i] == 1) {
		uint64_t anchor = c.recipes[2 * i + 1];
		assert(anchor && anchor <= n && c.jobs[anchor - 1]);
		const struct pg_source_scope *scope = pg_synthesis_prepared_environment(c.jobs[anchor - 1]);
		assert(scope && scope->registration); c.jobs[i] = scope->registration;
	}
	assert(c.schedule && fgetc(file) == EOF && !p->synthesis.steps && p->typing.proofs.count == 1);
	return c;
}

static uint64_t restore(struct checkpoint *c)
{
	struct pg_program *p = c->program;
	struct pg_synthesis *s = &p->synthesis;
	if (c->rules) {
		struct pg_synthesis_job **mapping;
		assert(!pg_artifact_derivations_prepare_with_dependencies(s, c->rules, c->input_count, c->inputs,
			NULL, c->prefix_count, c->jobs, &mapping));
		c->jobs = mapping;
	}
	assert(!pg_artifact_source_prepare(s, c->source, c->count, c->jobs));
	assert(c->count == s->jobs.count && !s->steps && p->typing.proofs.count == 1);
	size_t *targets = calloc(c->count, sizeof(*targets));
	assert(targets);
	size_t checked_rules = pg_artifact_derivations_validation(c->rules, targets);
	size_t ready = checked_rules + pg_artifact_source_validation(c->source, targets + checked_rules);
	const struct pg_artifact_schedule *validation = pg_artifact_schedule_ready(&p->graph, c->count, ready, targets);
	assert(validation && !pg_artifact_schedule_attach(s, validation, c->count, c->jobs));
	uint64_t spent, available = 100000;
	solving = 1;
	for (size_t i = 0; i < ready; ++i) {
		assert(pg_artifact_revalidate(p, c->jobs[targets[i]], available, available, &spent) == PG_SYNTHESIS_DONE);
		assert(spent <= available); available -= spent;
		verification += spent;
	}
	solving = 0;
	if (c->rules) assert(!pg_artifact_derivations_attach(s, c->rules, c->jobs));
	assert(!pg_artifact_source_attach(s, c->source, c->count, c->jobs));
	assert(!pg_artifact_schedule_attach(s, c->schedule, c->count, c->jobs));
	free(targets);
	assert(s->steps == 100000 - available);
	return s->steps;
}

static void equal(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int x, y;
	do { x = fgetc(a); y = fgetc(b); assert(x == y); } while (x != EOF);
	assert(!ferror(a) && !ferror(b));
}

static struct pg_program *start(const char *text)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	p->root = pg_program_source(p, p->scope, text, strlen(text), &p->parser);
	if (p->parser.error) fprintf(stderr, "%s: %s\n", p->parser.error, text);
	assert(p->root && !p->parser.error);
	return p;
}

struct shared_inputs {
	size_t count;
	const struct pg_derivation_input *const *headers;
	int trailing;
};

static int write_shared(FILE *file, const struct pg_graph_codec *codec, void *objects, void *owner)
{
	const struct shared_inputs *inputs = owner;
	if (pg_derivation_headers_write(file, inputs->count, inputs->headers, codec, objects)) return -1;
	return inputs->trailing ? pg_wire_write_u64(file, 1) : 0;
}

static int read_shared(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *objects, void *owner)
{
	struct shared_inputs *inputs = owner;
	return pg_derivation_headers_read(file, typing, limit, name_limit, codec, objects,
		&inputs->count, &inputs->headers);
}

static void shared_nominal_inputs(void)
{
	struct pg_program *p = start("A := @{ a : *; }; B := @{ a : *; }; f := \\x : A => x;");
	struct pg_synthesis_job *jobs[3];
	const char *names[] = {"A", "B", "f"};
	for (size_t i = 0; i < 3; ++i) {
		struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = names[i], .length = 1};
		jobs[i] = pg_program_select_name(p, p->root, name);
		assert(jobs[i]);
	}
	advance(&p->synthesis, 100000);
	const struct pg_derivation_input *headers[3];
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_evidence *proof = pg_synthesis_result(jobs[i]);
		assert(proof);
		const struct pg_term *core = pg_evidence_subject(proof)->core;
		/* Raw normalization obligations reference the source outputs. These
		 * parameter-only headers carry no premise or accepted result. */
		struct pg_derivation_input *header = pg_alloc(&p->graph, sizeof(*header));
		assert(header);
		*header = (struct pg_derivation_input){.rule = PG_PURE_NORMALIZATION,
			.source = core, .target = core, .reduction_kind = PG_REDUCTION_WHNF, .count = 1};
		headers[i] = header;
	}
	assert(headers[0]->source != headers[1]->source);
	struct shared_inputs out = {3, headers, 0}, in = {0};
	FILE *file = tmpfile(), *plain = tmpfile();
	assert(file && plain && !pg_sources_write_with(file, &p->synthesis, 3, jobs, 1, write_shared, &out));
	assert(!pg_sources_write(plain, &p->synthesis, 3, jobs));
	pg_program_destroy(p);
	rewind(file);
	size_t count = 0;
	struct pg_synthesis_job *const *roots = NULL;
	/* Neither reader may silently ignore a continuation or invent one. */
	assert(!pg_sources_read(file, 1000000, &count, &roots) && !count && !roots);
	rewind(plain);
	assert(!pg_sources_read_with(plain, 1000000, &count, &roots, read_shared, &in) && !count && !roots);
	rewind(file);
	p = pg_sources_read_with(file, 1000000, &count, &roots, read_shared, &in);
	assert(p && count == 3 && in.count == 3 && !p->synthesis.steps && p->typing.proofs.count == 1);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *subject;
		assert(pg_synthesis_materialized(roots[i], &subject) == 1);
		assert(subject->core == in.headers[i]->source && subject->core == in.headers[i]->target);
		assert(!pg_synthesis_result(roots[i]));
	}
	assert(in.headers[0]->source != in.headers[1]->source);
	FILE *copy = tmpfile();
	assert(copy && !pg_sources_write_with(copy, &p->synthesis, count, roots, 1, write_shared, &in));
	equal(file, copy);
	FILE *extra = tmpfile();
	in.trailing = 1;
	assert(extra && !pg_sources_write_with(extra, &p->synthesis, count, roots, 1, write_shared, &in));
	rewind(extra);
	struct shared_inputs bad = {0};
	size_t absent = 0;
	struct pg_synthesis_job *const *missing = NULL;
	assert(!pg_sources_read_with(extra, 1000000, &absent, &missing, read_shared, &bad) && !absent && !missing);
	advance(&p->synthesis, 100000);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *proof = pg_synthesis_result(roots[i]);
		assert(proof && pg_evidence_subject(proof)->core == in.headers[i]->source);
	}
	assert(!fclose(file) && !fclose(copy) && !fclose(extra) && !fclose(plain));
	pg_program_destroy(p);
	puts("source/rule sharing: distinct nominal ADTs, Lambda binders, inert resave, ordinary rechecking and exact boundary");
}

static FILE *result(struct pg_program *p)
{
	assert(p->root->status == PG_SYNTHESIS_DONE && !p->synthesis.ready);
	FILE *file = tmpfile();
	assert(file && !pg_sources_write(file, &p->synthesis, 1, &p->root));
	return file;
}

static void lifecycle(const char *text)
{
	for (uint64_t cut = 0; cut < 1000; ++cut) {
		struct pg_program *p = start(text);
		advance(&p->synthesis, cut);
		struct checkpoint c = collect(p);
		FILE *image = save(&c);
		uint64_t before = p->synthesis.steps;
		advance(&p->synthesis, 100000);
		uint64_t remaining = p->synthesis.steps - before;
		FILE *expected = result(p);
		pg_program_destroy(p);
		c = read_image(image); p = c.program;
		FILE *copy = save(&c); equal(image, copy); assert(!fclose(copy));
		uint64_t checked = restore(&c);
		struct checkpoint live = collect(p);
		copy = save(&live); equal(image, copy); assert(!fclose(copy));
		before = p->synthesis.steps;
		advance(&p->synthesis, 0);
		if (remaining) { advance(&p->synthesis, remaining - 1); assert(p->synthesis.ready); }
		advance(&p->synthesis, remaining ? 1 : 0);
		assert(p->synthesis.steps - before == remaining);
		assert(p->synthesis.steps == checked + remaining);
		copy = result(p); equal(expected, copy);
		assert(!fclose(image) && !fclose(copy) && !fclose(expected));
		pg_program_destroy(p); ++cuts;
		if (!remaining) return;
	}
	assert(!"source fixture did not complete");
}

static void partitions(void)
{
	const char *text = "{{ first := @; second := #\"hello\"; main := #-17; }}.main";
	const unsigned budgets[][2] = {{0, 0}, {10, 10}, {1, 19}, {0, 20}, {20, 0}};
	for (size_t i = 0; i < sizeof(budgets) / sizeof(*budgets); ++i) {
		struct pg_program *p = start(text);
		advance(&p->synthesis, budgets[i][0] + budgets[i][1]);
		struct checkpoint c = collect(p);
		FILE *expected = save(&c);
		pg_program_destroy(p);
		p = start(text); advance(&p->synthesis, budgets[i][0]);
		c = collect(p); FILE *partial = save(&c);
		pg_program_destroy(p);
		c = read_image(partial); p = c.program;
		uint64_t checked = restore(&c);
		advance(&p->synthesis, budgets[i][1]);
		assert(p->synthesis.steps == checked + budgets[i][1]);
		c = collect(p); FILE *actual = save(&c);
		equal(expected, actual);
		assert(!fclose(expected) && !fclose(partial) && !fclose(actual));
		pg_program_destroy(p);
	}
	puts("source partitions: 0+0, 10+10, 1+19, 0+20, 20+0 exact; all rechecking counted in Solve steps");
}

static struct pg_program *expression(const char *definition)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	pg_parser_init(&p->parser, &p->graph, definition, strlen(definition));
	struct pg_definition parsed;
	assert(pg_parser_next(&p->parser, &parsed) == 1);
	p->root = pg_synthesis_request(&p->synthesis, p->scope, parsed.expression);
	assert(p->root);
	return p;
}

static void guards(void)
{
	struct pg_program *p = expression("main := #1;"), *other = expression("main := #1;");
	struct pg_synthesis *s = &p->synthesis;
	struct pg_synthesis_job *rule = NULL;
	assert(pg_synthesis_literal_frontier(s, p->root, &rule) == 1 && !rule);
	assert(!pg_synthesis_prepare_literal(s, other->root));
	assert(!pg_synthesis_prepare_literal(s, p->scope->context_job));
	assert(pg_synthesis_literal_frontier(&other->synthesis, p->root, &rule) == -1);
	struct pg_synthesis_job *jobs[] = {p->root, p->root};
	assert(!pg_artifact_source_capture(&p->graph, s, 2, jobs, 1, &p->root));
	assert(!pg_artifact_source_capture(&p->graph, s, 1, jobs, 2, jobs));
	assert(!pg_artifact_source_capture(&p->graph, s, 1, &other->root, 1, &other->root));
	rule = pg_synthesis_prepare_literal(s, p->root);
	assert(rule && !s->steps && p->typing.proofs.count == 1 && !p->root->result);
	size_t count = s->jobs.count;
	assert(pg_synthesis_prepare_literal(s, p->root) == rule && s->jobs.count == count);
	assert(!pg_artifact_source_capture(&p->graph, s, 1, &p->root, 1, &p->root));
	struct checkpoint c = collect(p);
	assert(pg_artifact_source_prepare(&other->synthesis, c.source, c.count, c.jobs));
	assert(pg_artifact_source_prepare(s, c.source, c.count - 1, c.jobs));
	FILE *file = tmpfile();
	assert(file && !pg_artifact_source_write(file, c.source));
	rewind(file);
	assert(!pg_artifact_source_read(file, &p->graph, c.count - 1));
	assert(!fclose(file));
	advance(s, 10000);
	assert(p->root->status == PG_SYNTHESIS_DONE);
	c = collect(p); file = save(&c);
	pg_program_destroy(p); pg_program_destroy(other);
	c = read_image(file); p = c.program;
	struct pg_synthesis_job **mapping;
	assert(!pg_artifact_derivations_prepare_with_dependencies(&p->synthesis, c.rules,
		c.input_count, c.inputs, NULL, c.prefix_count, c.jobs, &mapping));
	c.jobs = mapping;
	assert(!pg_artifact_source_prepare(&p->synthesis, c.source, c.count, c.jobs));
	assert(pg_artifact_source_attach(&p->synthesis, c.source, c.count, c.jobs));
	assert(!p->synthesis.steps && p->typing.proofs.count == 1);
	assert(!fclose(file)); pg_program_destroy(p);
	p = expression("main := \\x : @ => x;");
	assert(!pg_synthesis_prepare_literal(&p->synthesis, p->root));
	assert(!pg_artifact_source_capture(&p->graph, &p->synthesis, 1, &p->root, 1, &p->root));
	pg_program_destroy(p);
	p = expression("main := #2147483648;");
	assert(!pg_synthesis_prepare_literal(&p->synthesis, p->root));
	assert(!p->synthesis.steps && p->root->status == PG_SYNTHESIS_PENDING);
	advance(&p->synthesis, 1000);
	assert(p->root->status == PG_SYNTHESIS_REJECTED);
	pg_program_destroy(p);
	puts("source guards: invalid ownership/mappings, omitted rule, unsupported syntax and unchecked completion rejected");
}

int main(void)
{
	shared_nominal_inputs();
	guards();
	partitions();
	lifecycle("main := #1;");
	lifecycle("{{ main := #1; }}.main");
	lifecycle("{{ first := @; second := #\"hello\"; main := #-17; }}.main");
	lifecycle("{{ first := #1; second := #1; main := #1; }}.main");
	printf("source checkpoint: %zu complete lifecycle cuts, %llu charged verification steps\n",
		cuts, (unsigned long long)verification);
	return 0;
}
