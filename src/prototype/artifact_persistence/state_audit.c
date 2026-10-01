#include "program.h"
#include "synthesis_source.h"
#include "derivation.h"
#include "dag.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Census only: repeated references are not automatically removable premises.
 * Scratch indexes never enter the Program or its artifact. */
static int typed_edge(const struct pg_occurrence *parent, const struct pg_occurrence *child)
{
	if (!parent || !child) return 0;
	if (parent->type == child || parent->origin == child) return 1;
	for (size_t i = 0; i < parent->operand_count; ++i)
		if (parent->operands[i] == child) return 1;
	return 0;
}

static void census(const struct pg_program *p, uint64_t requested)
{
	const struct pg_synthesis *s = &p->synthesis;
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count;
	size_t proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	size_t jobs = s->jobs.count;
	/* Lookup entries contain only an index header and an owner pointer. Report
	 * arena alignment and bucket storage separately from full Job allocations. */
	size_t key_extent = sizeof(struct pg_index_entry) + sizeof(struct pg_synthesis_job *);
	size_t alignment = _Alignof(max_align_t);
	size_t key_bytes = s->resolved_requests.count * ((key_extent + alignment - 1) / alignment * alignment);
	size_t key_buckets = s->resolved_requests.capacity * sizeof(*s->resolved_requests.buckets);
	uint64_t steps = s->steps;
	size_t pending = 0, done = 0, job_bytes = 0;
	size_t rules = 0, lazy = 0, outputs = 0, structural = 0, premise_edges = 0, overlap = 0;
	size_t retained_edges = 0, eliminations = 0;
	struct pg_dag results;
	assert(!pg_dag_init(&results, NULL, NULL));
	for (size_t i = 0; i < s->jobs.capacity; ++i) {
		for (const struct pg_index_entry *entry = s->jobs.buckets[i]; entry; entry = entry->next) {
			const struct pg_synthesis_job *job = (const void *)entry;
			size_t alignment = _Alignof(struct pg_synthesis_job);
			size_t bytes = sizeof(*job) + job->input_count * sizeof(*job->inputs)
				+ (job->role->size + alignment - 1) / alignment * alignment;
			job_bytes += bytes;
			pending += job->status == PG_SYNTHESIS_PENDING;
			done += job->status == PG_SYNTHESIS_DONE;
			rules += pg_synthesis_plain_derivation(job) != NULL;
			lazy += pg_synthesis_derivation_input(job) != NULL;
			structural += job->role->structure != NULL;
			if (job->result) { ++outputs; assert(!pg_dag_add(&results, job->result)); }
		}
	}
	for (size_t i = 0; i < p->typing.proofs.capacity; ++i) {
		for (const struct pg_index_entry *entry = p->typing.proofs.buckets[i]; entry; entry = entry->next) {
			const struct pg_evidence *proof = (const void *)entry;
			const struct pg_occurrence *subject = pg_evidence_subject(proof);
			size_t count = pg_evidence_premise_count(proof);
			premise_edges += count;
			retained_edges += pg_evidence_retained_premise_count(proof);
			eliminations += pg_evidence_rule(proof) == PG_MATCH_ELIM
				|| pg_evidence_rule(proof) == PG_INDUCTION_ELIM;
			for (size_t j = 0; j < count; ++j)
				overlap += typed_edge(subject, pg_evidence_subject(pg_evidence_premise(proof, j)));
		}
	}
	printf("%" PRIu64 "\t%" PRIu64 "\t%d\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
		requested, steps, pg_synthesis_status(p->root), terms, occurrences, proofs,
		jobs, pending, done, job_bytes, rules, lazy, structural,
		outputs, results.count, premise_edges, overlap, retained_edges, eliminations,
		s->resolved_requests.count, key_bytes, key_buckets);
	pg_dag_destroy(&results);
	assert(steps == s->steps && jobs == s->jobs.count && proofs == p->typing.proofs.count);
	assert(terms == p->graph.terms.count && objects == p->graph.objects.count);
	assert(occurrences == p->typing.occurrences.count);
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: %s SOURCE TOTAL_FUEL...\n", argv[0]); return 2; }
	FILE *file = fopen(argv[1], "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long length = ftell(file);
	assert(length >= 0 && (uintmax_t)length < SIZE_MAX && !fseek(file, 0, SEEK_SET));
	char *source = malloc((size_t)length + 1);
	assert(source && fread(source, 1, (size_t)length, file) == (size_t)length && !ferror(file));
	source[length] = 0;
	assert(!fclose(file));
	struct pg_program *p = pg_program_create(source, (size_t)length, PG_DEFINITION_IMPLICIT_THUNK);
	free(source);
	assert(p && p->root);
	puts("requested_fuel\tsteps\tstatus\tterms\toccurrences\tevidence\tjobs\tpending\tdone\tjob_bytes\trules\tlazy_inputs\tstructure_capable_jobs\tresult_refs\tunique_results\tpremise_edges\ttyped_overlap\tretained_premise_edges\telimination_receipts\tresolved_keys\tresolved_key_aligned_bytes\tresolved_index_bytes");
	uint64_t previous = 0;
	for (int i = 2; i < argc; ++i) {
		char *end;
		errno = 0;
		uintmax_t limit = strtoumax(argv[i], &end, 10);
		if (errno || !*argv[i] || *argv[i] == '-' || *end || limit > UINT64_MAX || limit < previous) {
			pg_program_destroy(p); return 2;
		}
		pg_synthesis_advance(&p->synthesis, (uint64_t)limit - previous);
		census(p, limit);
		previous = limit;
	}
	pg_program_destroy(p);
	return 0;
}
