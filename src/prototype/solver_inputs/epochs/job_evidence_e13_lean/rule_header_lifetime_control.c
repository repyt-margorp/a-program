#include "program.h"
#include "synthesis_source.h"
#include "derivation.h"
#include "classifier.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static struct pg_synthesis_job *request_universe(struct pg_program *program, uint64_t level)
{
	struct pg_derivation_input header = {.rule = PG_UNIVERSE_FORM,
		.parameters.level = level, .count = 1};
	struct pg_synthesis_input premise = {.checked = pg_synthesis_scope_context(program->scope)};
	struct pg_synthesis_job *job = pg_synthesis_rule_inputs(&program->synthesis,
		&header, &premise, NULL, NULL);
	assert(job);
	header.parameters.level = UINT64_MAX;
	assert(pg_synthesis_plain_derivation(job)->parameters.level == level);
	return job;
}

int main(void)
{
	static const char source[] = "main:=@;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1,
		PG_DEFINITION_IMPLICIT_THUNK);
	assert(program);
	struct pg_synthesis_job *jobs[128];
	struct pg_synthesis_job *(*volatile create)(struct pg_program *, uint64_t) = request_universe;
	for (size_t i = 0; i < sizeof(jobs) / sizeof(*jobs); ++i) jobs[i] = create(program, i);
	size_t headers = program->synthesis.rule_inputs.count, requests = program->synthesis.jobs.count;
	for (size_t i = 0; i < sizeof(jobs) / sizeof(*jobs); ++i) {
		assert(create(program, i) == jobs[i]);
		assert(pg_synthesis_plain_derivation(jobs[i])->parameters.level == i);
	}
	assert(program->synthesis.rule_inputs.count == headers && program->synthesis.jobs.count == requests);
	pg_synthesis_advance(&program->synthesis, 0);
	assert(!program->synthesis.steps);
	while (program->synthesis.ready) pg_synthesis_advance(&program->synthesis, 1);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	for (size_t i = 0; i < sizeof(jobs) / sizeof(*jobs); ++i) {
		uint64_t level;
		const struct pg_evidence *proof = pg_synthesis_result(jobs[i]);
		assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_DONE && proof);
		assert(pg_universe_level(pg_evidence_subject(proof)->core, &level) && level == i);
	}
	printf("rule_headers\t%zu\nsteps\t%" PRIu64 "\nstatus\t%d\n",
		program->synthesis.rule_inputs.count, program->synthesis.steps, pg_synthesis_status(program->root));
	pg_program_destroy(program);
	return 0;
}
