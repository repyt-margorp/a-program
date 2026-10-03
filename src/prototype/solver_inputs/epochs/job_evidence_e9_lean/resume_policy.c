#include "program.h"
#include "source_io.h"
#include "synthesis_source.h"
#include "artifact/file.h"
#include "wire.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int require_bodies;

static size_t started_bodies(const struct pg_program *p)
{
	size_t count = 0;
	for (size_t i = 0; i < p->synthesis.jobs.capacity; ++i) {
		for (const struct pg_index_entry *entry = p->synthesis.jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *body = NULL;
			if (!pg_synthesis_definition_body(&p->synthesis, (const void *)entry, &body)) count += body != NULL;
		}
	}
	return count;
}

/* Observe the public image and ordinary owners. No imported status admission,
 * replay checker, or reconstruction of private checking state. */
static void report(const char *phase, uint64_t cut, uint64_t budget, uint64_t cap,
	uint64_t validation, uint64_t useful, const struct pg_program *p)
{
	const struct pg_synthesis *s = &p->synthesis;
	size_t indexed = 0, activated = 0, names_complete = 0;
	size_t bodies = 0, checked = 0, materialized = 0, saved_complete = 0, ready = 0;
	uint64_t steps = s->steps;
	size_t proofs = p->typing.proofs.count, jobs = s->jobs.count;
	for (size_t i = 0; i < s->jobs.capacity; ++i) {
		for (const struct pg_index_entry *entry = s->jobs.buckets[i]; entry; entry = entry->next) {
			const struct pg_synthesis_job *job = (const void *)entry;
			struct pg_definition_frontier names;
			if (!pg_synthesis_definition_frontier(s, job, &names)) {
				indexed += names.indexed;
				activated += names.activated;
				names_complete += names.complete;
			}
			struct pg_synthesis_job *body = NULL;
			if (!pg_synthesis_definition_body(s, job, &body)) bodies += body != NULL;
			const struct pg_occurrence *subject = NULL;
			materialized += pg_synthesis_materialized(job, &subject) == 1;
			saved_complete += pg_synthesis_saved_complete(s, job) != 0;
			checked += job->status == PG_SYNTHESIS_DONE;
		}
	}
	for (const struct pg_synthesis_job *job = s->ready; job; job = job->next) ++ready;
	struct pg_module_frontier module = {0};
	int frontier = !pg_synthesis_module_frontier(s, p->root, &module);
	printf("%s\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
		"\t%" PRIu64 "\t%d\t%d\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
		phase, cut, budget, cap, validation, useful, steps, pg_synthesis_status(p->root),
		frontier, module.position, indexed, activated, names_complete, bodies, checked,
		materialized, saved_complete, ready, proofs);
	assert(steps == s->steps && proofs == p->typing.proofs.count && jobs == s->jobs.count);
}

static FILE *save(const struct pg_program *p)
{
	FILE *file = tmpfile();
	struct pg_synthesis_input root = {.pending = pg_synthesis_pending(p->root)};
	uint64_t steps = p->synthesis.steps;
	size_t jobs = p->synthesis.jobs.count, proofs = p->typing.proofs.count;
	assert(file && !pg_sources_write(file, &p->synthesis, 1, &root));
	assert(steps == p->synthesis.steps && jobs == p->synthesis.jobs.count && proofs == p->typing.proofs.count);
	rewind(file);
	return file;
}

static struct pg_program *load(FILE *file)
{
	size_t count = 0;
	struct pg_synthesis_job *const *roots = NULL;
	rewind(file);
	struct pg_program *p = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(p && count == 1 && p->root == roots[0]);
	assert(!p->synthesis.steps && pg_synthesis_status(p->root) == PG_SYNTHESIS_PENDING);
	return p;
}

static void same_bytes(FILE *first, FILE *second)
{
	rewind(first); rewind(second);
	int a, b;
	do { a = fgetc(first); b = fgetc(second); assert(a == b); } while (a != EOF);
	assert(!ferror(first) && !ferror(second));
}

static void policy(FILE *image, uint64_t cut, uint64_t budget, uint64_t cap, size_t bodies, size_t position)
{
	struct pg_program *p = load(image);
	if (require_bodies) assert(started_bodies(p) == bodies);
	struct pg_module_frontier module = {0};
	if (position) assert(!pg_synthesis_module_frontier(&p->synthesis, p->root, &module) && module.position == position);
	report("loaded-untrusted", cut, budget, cap, 0, 0, p);
	FILE *copy = save(p);
	same_bytes(image, copy);
	assert(!fclose(copy));
	uint64_t validation = UINT64_MAX;
	pg_artifact_revalidate(p, p->root, budget, cap, &validation);
	assert(validation <= budget && validation <= cap && validation == p->synthesis.steps);
	report("charged-validation", cut, budget, cap, validation, 0, p);
	uint64_t before = p->synthesis.steps;
	pg_synthesis_advance(&p->synthesis, budget - validation);
	uint64_t useful = p->synthesis.steps - before;
	assert(useful <= budget - validation && p->synthesis.steps <= budget);
	report("remaining-budget", cut, budget, cap, validation, useful, p);
	pg_program_destroy(p);
}

static void invalid_start_markers(FILE *file)
{
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		assert(!fseek(file, (long)scope[6] + (scope[0] == 5 ? 32 : 0), SEEK_CUR));
	}
	assert(!fseek(file, (long)header[2] * 8, SEEK_CUR));
	int tested = 0;
	for (size_t i = 0; i < header[4]; ++i) {
		uint64_t recipe[7];
		for (size_t j = 0; j < 7; ++j) assert(!pg_wire_read_u64(file, &recipe[j]));
		long position = ftell(file);
		int original = fgetc(file);
		assert(original >= 0 && original <= 3);
		if (!recipe[0] || recipe[2]) continue;
		/* A source module/reference is not a started lexical definition, even
		 * though the byte value itself is valid. Reserved bits also reject. */
		const int invalid[] = {original | 2, 4};
		for (size_t j = 0; j < 2; ++j) {
			assert(!fseek(file, position, SEEK_SET) && fputc(invalid[j], file) != EOF && !fflush(file));
			rewind(file);
			size_t count = 91;
			struct pg_synthesis_job *const *roots = NULL;
			struct pg_program *p = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
			assert(!p && count == 91 && !roots);
			assert(!fseek(file, position, SEEK_SET) && fputc(original, file) != EOF && !fflush(file));
		}
		tested = 1;
		break;
	}
	assert(tested);
}

int main(int argc, char **argv)
{
	if (argc != 2 && (argc != 3 || strcmp(argv[2], "--require-body-frontier"))) {
		fprintf(stderr, "usage: %s SOURCE [--require-body-frontier]\n", argv[0]); return 2;
	}
	require_bodies = argc == 3;
	FILE *source = fopen(argv[1], "rb");
	assert(source && !fseek(source, 0, SEEK_END));
	long size = ftell(source);
	assert(size >= 0 && (uintmax_t)size < SIZE_MAX && !fseek(source, 0, SEEK_SET));
	char *text = malloc((size_t)size + 1);
	assert(text && fread(text, 1, (size_t)size, source) == (size_t)size);
	text[size] = 0;
	assert(!fclose(source));
	puts("phase\tcut\tB\tR\tv\tuseful\tlocal_steps\tstatus\thas_module_cursor\tmodule_position\tindexed\tactivated\tcomplete_names\tbody_links\tchecked_jobs\tmaterialized\tsaved_complete\tready\tlocal_evidence");
	const uint64_t cuts[] = {0, 1000, 1600, 1921};
	for (size_t i = 0; i < sizeof(cuts) / sizeof(*cuts); ++i) {
		uint64_t cut = cuts[i], budget = cut == 1921 ? 0 : cut;
		struct pg_program *p = pg_program_create(text, (size_t)size, PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root && !p->parser.error);
		pg_synthesis_advance(&p->synthesis, cut);
		report("before-save", cut, budget, budget, 0, 0, p);
		FILE *image = save(p);
		if (require_bodies) invalid_start_markers(image);
		struct pg_module_frontier module = {0};
		pg_synthesis_module_frontier(&p->synthesis, p->root, &module);
		policy(image, cut, budget, budget, started_bodies(p), module.position);
		if (budget) policy(image, cut, budget, 128, started_bodies(p), module.position);
		uint64_t before = p->synthesis.steps;
		pg_synthesis_advance(&p->synthesis, budget);
		report("uninterrupted", cut, budget, budget, 0, p->synthesis.steps - before, p);
		assert(!fclose(image));
		pg_program_destroy(p);
	}
	free(text);
	return 0;
}
