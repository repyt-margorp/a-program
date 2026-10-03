#include "program.h"
#include "synthesis_work.h"
#include "synthesis_source.h"
#include "derivation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	assert(argc == 2 && (!strcmp(argv[1], "parent") || !strcmp(argv[1], "candidate")));
	int parent = !strcmp(argv[1], "parent");
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work evaluation;
	struct pg_synthesis synthesis;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&evaluation, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &evaluation, PG_DEFINITION_EXPLICIT_THUNK));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	struct pg_synthesis_input premise = {.checked = empty};
	struct pg_synthesis_job *job = pg_synthesis_plain_rule_inputs(&synthesis, PG_UNIVERSE_FORM, NULL, 1, &premise);
	assert(job && pg_synthesis_input_owned(&synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(job)}));
	size_t bytes = sizeof(*job) + job->input_count * sizeof(*job->inputs);
	struct pg_synthesis_job *copied = malloc(bytes);
	assert(copied);
	memcpy(copied, job, bytes);
	struct pg_synthesis_input scratch = {.pending = pg_synthesis_pending(copied)};
	size_t jobs = synthesis.jobs.count, rules = synthesis.rule_inputs.count, proofs = typing.proofs.count;
	struct pg_synthesis_job *ready = synthesis.ready, *tail = synthesis.ready_tail;
	int owned = pg_synthesis_input_owned(&synthesis, scratch);
	struct pg_synthesis_input inputs[] = {scratch, scratch};
	struct pg_synthesis_job *consumer = pg_synthesis_plain_rule_inputs(&synthesis, PG_CONTEXT_PROJECTION, NULL, 2, inputs);
	printf("copied_job_owned=%d factory_created=%d steps=%llu\n", owned, consumer != NULL,
		(unsigned long long)synthesis.steps);
	assert(owned == parent && (consumer != NULL) == parent && !synthesis.steps);
	if (!parent) {
		assert(synthesis.jobs.count == jobs && synthesis.rule_inputs.count == rules && typing.proofs.count == proofs);
		assert(synthesis.ready == ready && synthesis.ready_tail == tail);
		free(copied);
		pg_synthesis_advance(&synthesis, 0);
		assert(!synthesis.steps && !pg_synthesis_result(job));
		for (unsigned i = 0; pg_synthesis_status(job) == PG_SYNTHESIS_PENDING; ++i) {
			assert(i < 100);
			pg_synthesis_advance(&synthesis, 1);
		}
		assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
		assert(pg_evidence_owned_by(pg_synthesis_result(job), &typing));
	}
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&evaluation);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	if (parent) free(copied);
	return 0;
}
