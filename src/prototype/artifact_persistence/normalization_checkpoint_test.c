#include "program.h"
#include "synthesis_conversion.h"
#include "declaration_io.h"
#include "derivation_io.h"
#include "computation_io.h"
#include "computation.h"
#include "eval_internal.h"
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
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	if (pg_derivation_inputs_write_inference(file, 2, c->premises, c->effects, codec, c->objects)) return -1;
	return pg_whnf_pending_write(file, c->saved, codec, c->objects);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	size_t count;
	if (pg_derivations_read_inference(file, c->typing, limit, name_limit, c->effects,
		codec, c->objects, &count, &c->premises) || count != 2) return -1;
	return pg_whnf_pending_read(file, c->restored, graph, limit, name_limit, codec, c->objects);
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
	struct pg_synthesis_job *premises[2];
	for (size_t i = 0; i < 2; ++i) premises[i] = pg_synthesis_derivation(&synthesis, c.premises[i]);
	advance(&synthesis, 100000);
	const struct pg_evidence *context = pg_synthesis_result(premises[0]);
	const struct pg_evidence *proof = pg_synthesis_result(premises[1]);
	assert(context && proof && !synthesis.ready);
	struct pg_synthesis_job *job = force
		? pg_synthesis_evaluate_jobs(&synthesis, premises[0], premises[1], PG_REDUCTION_WHNF)
		: pg_synthesis_normalize_jobs(&synthesis, premises[0], premises[1], PG_REDUCTION_WHNF);
	assert(job && !pg_synthesis_normalization_pending(&synthesis, job));
	wrong_owner_and_request(&synthesis, job, context, proof, whnf, force);
	assert(!pg_synthesis_normalization_attach(&synthesis, job, whnf));
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
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	advance(&synthesis, 1);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE && !synthesis.ready);
	assert(synthesis.steps - before == remaining && whnf->steps - reduction_before == remaining);
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
	struct checkpoint c = {.typing = &program->typing, .effects = &effects, .objects = &codec};
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

int main(void)
{
	partitions("U := @{u:*;}; id := \\x:U=>x; main := id (id (id U.u));", 1);
	partitions("U := @{u:*;}; main := \\x:U=>x;", 0);
	partitions("Nat := @{zero:*;succ:*->*;}; id := \\n:Nat=>"
		"n @zero=>Nat.zero @succ k=>Nat.succ *k; main := id (Nat.succ (Nat.succ Nat.zero));", 1);
	partitions("main := #int_add #4 #5;", 1);
	return 0;
}
