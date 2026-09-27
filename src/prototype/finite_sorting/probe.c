#include "program.h"
#include "source_io.h"
#include "computation.h"
#include "classifier.h"
#include "eval_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t number(const char *text)
{
	char *end;
	errno = 0;
	uintmax_t value = strtoumax(text, &end, 10);
	if (errno || !*text || *end || *text == '-' || !value || value > UINT64_MAX - 64) return 0;
	return value;
}

static const struct pg_evidence *selected(struct pg_program *p, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
	struct pg_synthesis_job *job = pg_synthesis_definition(p->root, token);
	return pg_synthesis_result(job);
}

static const struct pg_term *core_input(struct pg_program *p, const struct pg_evidence *proof)
{
	const struct pg_term *input = pg_evidence_subject(proof)->core, *type;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
		pg_thunk_type_view(pg_evidence_classifier(proof), &type))
		input = pg_application(&p->graph, pg_reference(&p->graph, &pg_force_operation), input);
	return input;
}

static const struct pg_term *core_nf(struct pg_program *p, const char *name, uint64_t budget)
{
	const struct pg_evidence *proof = selected(p, name);
	if (!proof || !pg_evidence_subject(proof)) return NULL;
	const struct pg_term *input = core_input(p, proof);
	struct pg_nf_job *job = pg_nf_request(&p->evaluation, &pg_pure_policy, input);
	if (!job) return NULL;
	clock_t start = clock();
	uint64_t before = pg_nf_steps(job);
	uint64_t demand_readback = 0, final_readback = 0, other = 0;
	while (pg_nf_status(job) == PG_NF_PENDING && pg_nf_steps(job) - before < budget) {
		struct pg_nf_job *active = job->depth ? job->stack[job->depth - 1] : job;
		struct pg_whnf_job *head = active->head;
		if (head && head->machine.head_ready && head->machine.frames) ++demand_readback;
		else if (head && head->machine.status == PG_EVAL_WHNF && head->status == PG_EVAL_PENDING) ++final_readback;
		else ++other;
		pg_nf_advance(job, 64);
	}
	printf("core %s: status=%d steps=%" PRIu64 " terms=%zu cpu=%.3f\n", name,
		pg_nf_status(job), pg_nf_steps(job) - before, p->graph.terms.count,
		(double)(clock() - start) / CLOCKS_PER_SEC);
	printf("core samples: demand-readback=%" PRIu64 " final-readback=%" PRIu64 " other=%" PRIu64 "\n",
		demand_readback, final_readback, other);
	const struct pg_term *result = pg_nf_result(job);
	if (result && result->kind == PG_APPLICATION &&
		result->as.application.function == pg_reference(&p->graph, &pg_return_operation))
		result = result->as.application.argument;
	return result;
}

int main(int argc, char **argv)
{
	if (argc != 7) {
		fprintf(stderr, "usage: %s IMAGE NAME EXPECTED SYNTHESIS_BUDGET NORMALIZATION_BUDGET IMAGE_LIMIT\n", argv[0]);
		return 2;
	}
	uint64_t synthesis_budget = number(argv[4]), normalization_budget = number(argv[5]), limit = number(argv[6]);
	if (!synthesis_budget || !normalization_budget || !limit || limit > SIZE_MAX) return 2;
	FILE *file = fopen(argv[1], "rb");
	if (!file) return 2;
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_sources_read(file, (size_t)limit, &count, &roots);
	int closed = fclose(file);
	if (!p || count != 1 || closed) { pg_program_destroy(p); return 2; }
	clock_t start = clock();
	while (p->synthesis.ready && p->synthesis.steps < synthesis_budget)
		pg_synthesis_advance(&p->synthesis, 64);
	printf("source: status=%d steps=%" PRIu64 " terms=%zu jobs=%zu cpu=%.3f\n",
		pg_synthesis_status(p->root), p->synthesis.steps, p->graph.terms.count, p->synthesis.jobs.count,
		(double)(clock() - start) / CLOCKS_PER_SEC);
	if (pg_synthesis_status(p->root) != PG_SYNTHESIS_DONE) { pg_program_destroy(p); return 3; }
	const struct pg_term *left = core_nf(p, argv[2], normalization_budget);
	const struct pg_term *right = core_nf(p, argv[3], normalization_budget);
	int equal = left && right ? pg_alpha_equal(left, right) : -1;
	printf("core-equal: %d\n", equal);
	const struct pg_evidence *proof = selected(p, argv[2]);
	struct pg_synthesis_job *typed = pg_program_normalize(p, proof, 1);
	if (!typed) { pg_program_destroy(p); return 2; }
	uint64_t before = p->synthesis.steps;
	start = clock();
	while (p->synthesis.ready && p->synthesis.steps - before < normalization_budget)
		pg_synthesis_advance(&p->synthesis, 64);
	printf("typed %s: status=%d steps=%" PRIu64 " terms=%zu jobs=%zu cpu=%.3f\n", argv[2],
		pg_synthesis_status(typed), p->synthesis.steps - before, p->graph.terms.count, p->synthesis.jobs.count,
		(double)(clock() - start) / CLOCKS_PER_SEC);
	int result = equal == 1 && pg_synthesis_status(typed) == PG_SYNTHESIS_DONE ? 0 : 3;
	pg_program_destroy(p);
	return result;
}
