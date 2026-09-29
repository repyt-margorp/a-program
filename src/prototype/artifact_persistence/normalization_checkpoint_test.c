#include "program.h"
#include "artifact/schedule.h"
#include "synthesis_conversion.h"
#include "declaration_io.h"
#include "derivation_io.h"
#include "computation_io.h"
#include "computation.h"
#include "eval_internal.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* This fixture owns the saved state and establishes its provenance. Raw
 * decoding alone never authorizes attachment to a checked normalization. */
static const char magic[8] = "APGNCT\1";
static int solving;

void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *s, uint64_t n)
{
	assert(solving);
	__real_pg_synthesis_advance(s, n);
}

enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *w, uint64_t n)
{
	assert(solving);
	return __real_pg_whnf_advance(w, n);
}

static void advance(struct pg_synthesis *s, uint64_t n)
{
	solving = 1;
	pg_synthesis_advance(s, n);
	solving = 0;
}

struct checkpoint {
	struct pg_typing *typing;
	struct pg_effect_inference *effects;
	struct pg_declaration_io *objects;
	const struct pg_derivation_input *const *premises;
	const struct pg_whnf_job *saved;
	struct pg_whnf_job *restored;
	size_t premise_count;
	const struct pg_artifact_schedule *schedule;
	const struct pg_synthesis *synthesis;
	struct pg_synthesis_job *const *jobs;
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	if (pg_derivation_inputs_write_inference(file, c->premise_count, c->premises, c->effects, codec, c->objects)) return -1;
	if (pg_whnf_pending_write(file, c->saved, codec, c->objects)) return -1;
	if (c->premise_count == 2) return 0;
	return c->schedule ? pg_artifact_schedule_save(file, c->schedule)
		: pg_artifact_schedule_write(file, c->synthesis, 5, c->jobs);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	size_t count;
	if (pg_derivations_read_inference(file, c->typing, limit, name_limit, c->effects,
		codec, c->objects, &count, &c->premises) || (count != 2 && count != 3)) return -1;
	c->premise_count = count;
	if (pg_whnf_pending_read(file, c->restored, graph, limit, name_limit, codec, c->objects)) return -1;
	if (count == 2) return 0;
	c->schedule = pg_artifact_schedule_read(file, graph, limit);
	return c->schedule ? 0 : -1;
}

static FILE *write_checkpoint(struct checkpoint *c)
{
	FILE *file = tmpfile();
	size_t proofs = c->typing->proofs.count, terms = c->typing->graph->terms.count;
	assert(file && !pg_graph_image_write(file, magic, &pg_declaration_graph_codec,
		c->objects, write_payload, c));
	assert(proofs == c->typing->proofs.count && terms == c->typing->graph->terms.count);
	rewind(file);
	return file;
}

static void equal_files(FILE *left, FILE *right)
{
	rewind(left); rewind(right);
	int a, b;
	do { a = fgetc(left); b = fgetc(right); assert(a == b); } while (a != EOF);
	assert(!ferror(left) && !ferror(right));
}

static FILE *result_file(struct pg_declaration_io *codec, const struct pg_term *term)
{
	FILE *file = tmpfile();
	assert(file && term && !pg_graph_write_descriptors(file, 1, &term,
		&pg_declaration_graph_codec, codec));
	return file;
}

static void wrong_owner_and_request(struct pg_synthesis *s, struct pg_synthesis_job *job,
	const struct pg_evidence *context, const struct pg_evidence *proof, struct pg_whnf_job *whnf, int force)
{
	struct pg_synthesis other;
	struct pg_whnf_work evaluation;
	assert(!pg_whnf_work_init(&evaluation, s->typing->graph));
	assert(!pg_synthesis_init(&other, s->typing, &evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	assert(pg_synthesis_normalization_attach(&other, job, whnf));
	assert(!pg_synthesis_normalization_pending(&other, job));
	struct pg_synthesis_job *nf = pg_synthesis_nf(&other, context, proof);
	assert(nf && pg_synthesis_normalization_attach(&other, nf, whnf));
	const struct pg_evidence *different = pg_prove_universe(s->typing, context, 42);
	struct pg_synthesis_job *wrong = pg_synthesis_normalize(&other, context, different);
	assert(wrong && pg_synthesis_normalization_attach(&other, wrong, whnf));
	struct pg_synthesis_job *context_job = pg_synthesis_evidence(&other, context);
	struct pg_synthesis_job *proof_job = pg_synthesis_evidence(&other, proof);
	struct pg_synthesis_job *pending = pg_synthesis_normalize_jobs(&other, context_job, nf, PG_REDUCTION_WHNF);
	assert(pending && pg_synthesis_normalization_attach(&other, pending, whnf));
	struct pg_synthesis_job *correct = force
		? pg_synthesis_evaluate_jobs(&other, context_job, proof_job, PG_REDUCTION_WHNF)
		: pg_synthesis_normalize_jobs(&other, context_job, proof_job, PG_REDUCTION_WHNF);
	assert(correct && pg_whnf_request(&evaluation, &pg_pure_policy, whnf->request.input));
	assert(pg_synthesis_normalization_attach(&other, correct, whnf));
	whnf->request.policy = &pg_beta_policy;
	assert(pg_synthesis_normalization_attach(s, job, whnf));
	whnf->request.policy = &pg_pure_policy;
	assert(!whnf->request.work && !other.steps);
	pg_synthesis_destroy(&other);
	pg_whnf_work_destroy(&evaluation);
}

static void shared_jobs(struct pg_synthesis *s, const struct pg_evidence *context,
	const struct pg_evidence *proof, const struct pg_evidence *type, struct pg_synthesis_job **jobs)
{
	struct pg_synthesis_job *c = pg_synthesis_evidence(s, context), *p = pg_synthesis_evidence(s, proof);
	const struct pg_term *content;
	const struct pg_evidence *input = pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
		pg_thunk_type_view(pg_evidence_classifier(proof), &content) ? pg_prove_force(s->typing, proof) : proof;
	assert(input);
	jobs[0] = pg_synthesis_evaluate_jobs(s, c, p, PG_REDUCTION_WHNF);
	jobs[1] = pg_synthesis_normalize(s, context, input);
	struct pg_synthesis_job *t = pg_synthesis_evidence(s, type);
	jobs[2] = pg_synthesis_expect(s, jobs[0], t);
	jobs[3] = pg_synthesis_expect(s, jobs[0], pg_synthesis_evidence(s, pg_prove_universe(s->typing, context, 42)));
	jobs[4] = pg_synthesis_expect(s, jobs[1], t);
	for (size_t i = 0; i < 5; ++i) assert(jobs[i]);
}

static void resume(FILE *file, FILE *expected, uint64_t remaining, int force)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work evaluation;
	struct pg_synthesis synthesis;
	struct pg_effect_inference effects;
	struct pg_declaration_io codec;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&evaluation, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	assert(!pg_effect_inference_init(&effects, &graph) && !pg_declaration_io_init(&codec, &typing));
	struct pg_whnf_job *whnf = pg_alloc(&graph, sizeof(*whnf));
	assert(whnf);
	struct checkpoint c = {.typing = &typing, .effects = &effects, .objects = &codec, .restored = whnf};
	rewind(file);
	assert(!pg_graph_image_read(file, magic, &graph, 100000, 100000,
		&pg_declaration_graph_codec, &codec, read_payload, &c));
	assert(!typing.proofs.count && !synthesis.steps && !whnf->request.work);
	assert(!pg_whnf_certificate(whnf));
	c.saved = whnf;
	FILE *copy = write_checkpoint(&c);
	equal_files(file, copy);
	assert(!fclose(copy));
	struct pg_synthesis_job *premises[3];
	for (size_t i = 0; i < c.premise_count; ++i) premises[i] = pg_synthesis_derivation(&synthesis, c.premises[i]);
	advance(&synthesis, 100000);
	const struct pg_evidence *context = pg_synthesis_result(premises[0]);
	const struct pg_evidence *proof = pg_synthesis_result(premises[1]);
	assert(context && proof && !synthesis.ready);
	struct pg_synthesis_job *jobs[5], *job;
	if (c.schedule) {
		shared_jobs(&synthesis, context, proof, pg_synthesis_result(premises[2]), jobs);
		job = jobs[0];
	} else job = force
			? pg_synthesis_evaluate_jobs(&synthesis, premises[0], premises[1], PG_REDUCTION_WHNF)
			: pg_synthesis_normalize_jobs(&synthesis, premises[0], premises[1], PG_REDUCTION_WHNF);
	assert(job && !pg_synthesis_normalization_pending(&synthesis, job));
	wrong_owner_and_request(&synthesis, job, context, proof, whnf, force);
	assert(!pg_synthesis_normalization_attach(&synthesis, job, whnf));
	if (c.schedule) {
		assert(!pg_synthesis_normalization_attach(&synthesis, jobs[1], whnf));
		assert(!pg_artifact_schedule_attach(&synthesis, c.schedule, 5, jobs));
		assert(synthesis.ready == jobs[0] && jobs[0]->next == jobs[1] && synthesis.ready_tail == jobs[1]);
		assert(jobs[0]->waiters->parent == jobs[3] && jobs[0]->waiters->next->parent == jobs[2]);
		assert(jobs[1]->waiters->parent == jobs[4]);
	}
	assert(pg_synthesis_normalization_attach(&synthesis, job, whnf));
	assert(pg_synthesis_normalization_pending(&synthesis, job) == whnf);
	assert(pg_whnf_request(&evaluation, &pg_pure_policy, whnf->request.input) == whnf);
	assert(!pg_synthesis_result(job));
	uint64_t before = synthesis.steps, reduction_before = whnf->steps;
	advance(&synthesis, 0);
	assert(synthesis.steps == before && whnf->steps == reduction_before);
	copy = write_checkpoint(&c);
	equal_files(file, copy);
	assert(!fclose(copy));
	advance(&synthesis, remaining - 1);
	assert(synthesis.ready);
	if (!c.schedule) assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	advance(&synthesis, 1);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE && !synthesis.ready);
	assert(synthesis.steps - before == remaining);
	if (!c.schedule) assert(whnf->steps - reduction_before == remaining);
	else {
		assert(pg_synthesis_result(jobs[1]) && pg_synthesis_result(jobs[2]) && pg_synthesis_result(jobs[4]));
		assert(pg_synthesis_status(jobs[3]) == PG_SYNTHESIS_REJECTED);
	}
	assert(!pg_synthesis_normalization_pending(&synthesis, job));
	assert(pg_synthesis_normalization_attach(&synthesis, job, whnf));
	const struct pg_term *output = pg_evidence_subject(pg_synthesis_result(job))->core;
	assert(output == pg_whnf_result(whnf));
	FILE *actual = result_file(&codec, output);
	equal_files(expected, actual);
	assert(!fclose(actual));
	pg_declaration_io_destroy(&codec);
	pg_synthesis_destroy(&synthesis);
	pg_effect_inference_destroy(&effects);
	pg_whnf_work_destroy(&evaluation);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

static void partitions(const char *source, int force)
{
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected = pg_program_select_name(program, program->root, name);
	assert(selected);
	advance(&program->synthesis, 100000);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(proof && !program->synthesis.ready);
	struct pg_synthesis_job *premises[] = {
		pg_synthesis_evidence(&program->synthesis, pg_prove_empty_context(&program->typing)), selected};
	struct pg_synthesis_job *job = force
		? pg_synthesis_evaluate_jobs(&program->synthesis, premises[0], premises[1], PG_REDUCTION_WHNF)
		: pg_synthesis_normalize_jobs(&program->synthesis, premises[0], premises[1], PG_REDUCTION_WHNF);
	assert(job && !pg_synthesis_normalization_pending(&program->synthesis, job));
	struct pg_graph storage;
	struct pg_effect_inference effects;
	struct pg_declaration_io codec;
	assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&effects, &storage));
	assert(!pg_declaration_io_init(&codec, &program->typing));
	struct checkpoint c = {.typing = &program->typing, .effects = &effects, .objects = &codec, .premise_count = 2};
	assert(!pg_synthesis_export_rules(&program->synthesis, 2, premises, &storage, &effects, 1, &c.premises));
	FILE *images[512];
	size_t count = 0;
	int evaluating = 0, materializing = 0;
	uint64_t before = program->synthesis.steps;
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		advance(&program->synthesis, 1);
		c.saved = pg_synthesis_normalization_pending(&program->synthesis, job);
		if (!c.saved) break;
		assert(count < sizeof(images) / sizeof(*images));
		evaluating |= c.saved->machine.status == PG_EVAL_PENDING;
		materializing |= c.saved->machine.status == PG_EVAL_WHNF;
		images[count++] = write_checkpoint(&c);
	}
	assert(pg_synthesis_result(job) && count && evaluating && materializing);
	assert(program->synthesis.steps - before == count + 1);
	FILE *expected = result_file(&codec, pg_evidence_subject(pg_synthesis_result(job))->core);
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects);
	pg_graph_destroy(&storage);
	pg_program_destroy(program);
	for (size_t i = 0; i < count; ++i) {
		resume(images[i], expected, count - i, force);
		assert(!fclose(images[i]));
	}
	assert(!fclose(expected));
	printf("WHNF owner: %zu split points, exact remaining fuel/result, force=%d\n", count, force);
}

static void shared_checkpoint(void)
{
	const char source[] = "U := @{u:*;}; id := \\x:U=>x; main := id (id (id U.u));";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected = pg_program_select_name(program, program->root, name);
	advance(&program->synthesis, 100000);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
	const struct pg_term *content;
	const struct pg_evidence *input = pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
		pg_thunk_type_view(pg_evidence_classifier(proof), &content) ? pg_prove_force(&program->typing, proof) : proof;
	const struct pg_evidence *type = pg_prove_classifier(&program->typing, context, input);
	assert(type);
	struct pg_synthesis_job *jobs[5];
	shared_jobs(&program->synthesis, context, proof, type, jobs);
	struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(&program->synthesis, context), selected,
		pg_synthesis_evidence(&program->synthesis, type)};
	struct pg_graph storage;
	struct pg_effect_inference effects;
	struct pg_declaration_io codec;
	assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&effects, &storage));
	assert(!pg_declaration_io_init(&codec, &program->typing));
	struct checkpoint c = {.typing = &program->typing, .effects = &effects, .objects = &codec,
		.premise_count = 3, .synthesis = &program->synthesis, .jobs = jobs};
	assert(!pg_synthesis_export_rules(&program->synthesis, 3, premises, &storage, &effects, 1, &c.premises));
	advance(&program->synthesis, 5);
	c.saved = pg_synthesis_normalization_pending(&program->synthesis, jobs[0]);
	assert(c.saved && c.saved == pg_synthesis_normalization_pending(&program->synthesis, jobs[1]));
	FILE *file = write_checkpoint(&c);
	uint64_t before = program->synthesis.steps;
	advance(&program->synthesis, 100000);
	uint64_t remaining = program->synthesis.steps - before;
	assert(pg_synthesis_result(jobs[2]) && pg_synthesis_result(jobs[4]));
	assert(pg_synthesis_status(jobs[3]) == PG_SYNTHESIS_REJECTED);
	FILE *expected = result_file(&codec, pg_evidence_subject(pg_synthesis_result(jobs[0]))->core);
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects);
	pg_graph_destroy(&storage);
	pg_program_destroy(program);
	resume(file, expected, remaining, 1);
	assert(!fclose(file) && !fclose(expected));
	printf("shared WHNF: preserved ready/wait order, exact %llu remaining Solve dispatches\n", (unsigned long long)remaining);
}

static unsigned trace[8], trace_count;
static unsigned identities[] = {0, 1, 2, 3};

static void idle_start(struct pg_synthesis *s, struct pg_synthesis_job *job)
{
	(void)s; (void)job;
}

static void consumer_step(struct pg_synthesis *s, struct pg_synthesis_job *job)
{
	assert(trace_count < 8);
	trace[trace_count++] = *(const unsigned *)job->inputs[0];
	pg_synthesis_finish(s, job, PG_SYNTHESIS_DONE);
}

static const struct pg_synthesis_work_class consumer = {.start = idle_start, .advance = consumer_step};
static const struct pg_synthesis_work_class preparing;

static void preparation_step(struct pg_synthesis *s, struct pg_synthesis_job *job)
{
	unsigned *stage = pg_synthesis_work_state(job, &preparing);
	assert(trace_count < 8);
	trace[trace_count++] = 0;
	if (++*stage == 1) pg_synthesis_enqueue(s, job);
	else pg_synthesis_finish(s, job, PG_SYNTHESIS_DONE);
}

static struct pg_synthesis_projection preparation_view(const struct pg_synthesis_job *job)
{
	const unsigned *stage = pg_synthesis_work_state(job, &preparing);
	return (struct pg_synthesis_projection){.value_kind = -1, .preparing = !*stage};
}

static const struct pg_synthesis_work_class preparing = {
	.size = sizeof(unsigned), .advance = preparation_step, .project = preparation_view};

static void schedule_jobs(struct pg_synthesis *s, struct pg_synthesis_job **jobs)
{
	jobs[0] = pg_synthesis_work_request(s, &preparing, 0, NULL);
	for (size_t i = 1; i < 4; ++i) {
		const void *inputs[] = {&identities[i]};
		jobs[i] = pg_synthesis_work_request(s, &consumer, 1, inputs);
		assert(jobs[i]);
	}
}

static void schedule_boundaries(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *jobs[4];
	schedule_jobs(&p->synthesis, jobs);
	for (size_t i = 1; i < 4; ++i) pg_synthesis_subscribe(&p->synthesis, jobs[i], jobs[0], i != 2);
	FILE *file = tmpfile();
	assert(file && !pg_artifact_schedule_write(file, &p->synthesis, 4, jobs));
	FILE *partial = tmpfile();
	assert(partial && pg_artifact_schedule_write(partial, &p->synthesis, 3, jobs));
	assert(ftell(partial) == 0 && !fclose(partial));
	long size = ftell(file);
	assert(size == 112);
	unsigned char bytes[112];
	rewind(file);
	assert(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
	trace_count = 0;
	advance(&p->synthesis, 100);
	const unsigned expected[] = {0, 0, 3, 1, 2};
	assert(trace_count == 5 && !memcmp(trace, expected, sizeof(expected)));
	pg_program_destroy(p);
	p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	size_t proofs = p->typing.proofs.count;
	schedule_jobs(&p->synthesis, jobs);
	FILE *before = tmpfile();
	assert(before && !pg_artifact_schedule_write(before, &p->synthesis, 4, jobs));
	for (size_t cut = 0; cut < sizeof(bytes); ++cut) {
		FILE *short_file = tmpfile();
		assert(short_file && fwrite(bytes, 1, cut, short_file) == cut);
		rewind(short_file);
		assert(!pg_artifact_schedule_read(short_file, &p->graph, 4));
		assert(!fclose(short_file));
	}
	/* Header counts, duplicate/overlapping consumers, absent endpoints and
	 * preparation flags are transport errors, not pending logical obligations. */
	const size_t offsets[] = {8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104};
	for (size_t i = 0; i < sizeof(offsets) / sizeof(*offsets); ++i) {
		FILE *bad = tmpfile();
		assert(bad && fwrite(bytes, 1, sizeof(bytes), bad) == sizeof(bytes));
		assert(!fseek(bad, (long)offsets[i], SEEK_SET) && !pg_wire_write_u64(bad, 5));
		rewind(bad);
		assert(!pg_artifact_schedule_read(bad, &p->graph, 4));
		assert(!fclose(bad));
	}
	const uint64_t invalid_records[][2] = {{40, 0}, {64, 3}, {72, 1}};
	for (size_t i = 0; i < sizeof(invalid_records) / sizeof(*invalid_records); ++i) {
		FILE *bad = tmpfile();
		assert(bad && fwrite(bytes, 1, sizeof(bytes), bad) == sizeof(bytes));
		assert(!fseek(bad, (long)invalid_records[i][0], SEEK_SET) && !pg_wire_write_u64(bad, invalid_records[i][1]));
		rewind(bad);
		assert(!pg_artifact_schedule_read(bad, &p->graph, 4));
		assert(!fclose(bad));
	}
	rewind(file);
	assert(!pg_artifact_schedule_read(file, &p->graph, 3));
	rewind(file);
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_read(file, &p->graph, 4);
	assert(schedule && !p->synthesis.steps && p->typing.proofs.count == proofs);
	struct pg_synthesis_job *duplicate[] = {jobs[0], jobs[0], jobs[2], jobs[3]};
	assert(pg_artifact_schedule_attach(&p->synthesis, schedule, 4, duplicate));
	assert(pg_artifact_schedule_attach(&p->synthesis, schedule, 3, jobs));
	struct pg_synthesis foreign;
	assert(!pg_synthesis_init(&foreign, &p->typing, &p->evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	assert(pg_artifact_schedule_attach(&foreign, schedule, 4, jobs));
	pg_synthesis_destroy(&foreign);
	FILE *unchanged = tmpfile();
	assert(unchanged && !pg_artifact_schedule_write(unchanged, &p->synthesis, 4, jobs));
	equal_files(before, unchanged);
	assert(!fclose(unchanged) && !fclose(before));
	assert(!pg_artifact_schedule_attach(&p->synthesis, schedule, 4, jobs));
	FILE *copy = tmpfile();
	assert(copy && !pg_artifact_schedule_write(copy, &p->synthesis, 4, jobs));
	equal_files(file, copy);
	assert(!fclose(copy) && !fclose(file));
	trace_count = 0;
	advance(&p->synthesis, 0);
	assert(!trace_count && !p->synthesis.steps);
	advance(&p->synthesis, 100);
	assert(trace_count == 5 && !memcmp(trace, expected, sizeof(expected)) && p->synthesis.steps == 5);
	assert(p->typing.proofs.count == proofs);
	assert(pg_artifact_schedule_attach(&p->synthesis, schedule, 4, jobs));
	assert(!p->synthesis.ready && !p->synthesis.ready_tail && p->synthesis.steps == 5);
	pg_program_destroy(p);
	puts("scheduler: inert decode, exact preparation/wake order, invalid transport atomicity");
}

static void waiting_cycle(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *jobs[2];
	for (size_t i = 0; i < 2; ++i) {
		const void *input[] = {&identities[i]};
		jobs[i] = pg_synthesis_work_request(&p->synthesis, &consumer, 1, input);
		assert(jobs[i]);
	}
	pg_synthesis_subscribe(&p->synthesis, jobs[0], jobs[1], 0);
	pg_synthesis_subscribe(&p->synthesis, jobs[1], jobs[0], 0);
	assert(pg_synthesis_cycle(jobs[0]) && !p->synthesis.ready);
	FILE *file = tmpfile();
	assert(file && !pg_artifact_schedule_write(file, &p->synthesis, 2, jobs));
	rewind(file);
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_read(file, &p->graph, 2);
	assert(schedule && !pg_artifact_schedule_attach(&p->synthesis, schedule, 2, jobs));
	assert(pg_synthesis_cycle(jobs[0]) && !p->synthesis.ready);
	advance(&p->synthesis, 100);
	assert(!p->synthesis.steps && !pg_synthesis_result(jobs[0]));
	FILE *copy = tmpfile();
	assert(copy && !pg_artifact_schedule_write(copy, &p->synthesis, 2, jobs));
	equal_files(file, copy);
	assert(!fclose(copy) && !fclose(file));
	pg_program_destroy(p);
	puts("scheduler: dormant wait cycle preserved without inventing runnable work");
}

int main(void)
{
	partitions("U := @{u:*;}; id := \\x:U=>x; main := id (id (id U.u));", 1);
	partitions("U := @{u:*;}; main := \\x:U=>x;", 0);
	partitions("Nat := @{zero:*;succ:*->*;}; id := \\n:Nat=>"
		"n @zero=>Nat.zero @succ k=>Nat.succ *k; main := id (Nat.succ (Nat.succ Nat.zero));", 1);
	partitions("main := #int_add #4 #5;", 1);
	shared_checkpoint();
	schedule_boundaries();
	waiting_cycle();
	return 0;
}
