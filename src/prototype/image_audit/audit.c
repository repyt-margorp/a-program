#include "program.h"
#include "source_io.h"
#include "computation_io.h"
#include "derivation_io.h"
#include "eval_internal.h"
#include "wire.h"
#include "syntax_io.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* Read-only instrumentation around the real codecs. No alternate writer. */
static const char *mode;
static size_t substitution_terms;
static uint64_t duplicate_references;
static struct pg_graph *observed_graph;

enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);

enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	struct pg_graph *graph = work->state ? work->state->context.output : NULL;
	size_t before = graph ? graph->terms.count : 0;
	enum pg_substitution_status result = __real_pg_substitution_advance(work, budget);
	if (graph && graph == observed_graph) substitution_terms += graph->terms.count - before;
	return result;
}

int __real_pg_syntax_write(FILE *, size_t, const struct pg_syntax *const *);

int __wrap_pg_syntax_write(FILE *file, size_t count, const struct pg_syntax *const *roots)
{
	long start = ftell(file);
	int status = __real_pg_syntax_write(file, count, roots);
	printf("%s syntax roots=%zu bytes=%ld\n", mode, count, ftell(file) - start);
	return status;
}

static uint64_t word(FILE *file)
{
	uint64_t value;
	assert(!pg_wire_read_u64(file, &value));
	return value;
}

struct triple { uint64_t kind, left, right; };

static int compare(const void *a, const void *b)
{
	const struct triple *x = a, *y = b;
	if (x->kind != y->kind) return x->kind < y->kind ? -1 : 1;
	if (x->left != y->left) return x->left < y->left ? -1 : 1;
	return (x->right > y->right) - (x->right < y->right);
}

static void graph_stats(FILE *file)
{
	char magic[8];
	assert(fread(magic, 1, 8, file) == 8 && !memcmp(magic, "APGCORE\1", 8));
	uint64_t objects = word(file), terms = word(file), roots = word(file);
	long start = ftell(file);
	uint64_t binders = 0, edges = 0, scalars = 0;
	for (uint64_t i = 0; i < objects; ++i) {
		int kind = fgetc(file);
		assert(kind >= 0 && kind <= 4);
		if (!kind) { ++binders; continue; }
		uint64_t length = word(file);
		assert(length < 1000000 && !fseek(file, (long)length, SEEK_CUR));
		if (kind < 3) continue;
		while (word(file)) ++edges;
		int more;
		while ((more = fgetc(file)) == 1) { word(file); ++scalars; }
		assert(more == 0);
	}
	long object_bytes = ftell(file) - start;
	struct triple *items = calloc((size_t)terms, sizeof(*items));
	unsigned char *seen = calloc((size_t)terms + 1, 1);
	assert(items && seen);
	start = ftell(file);
	uint64_t kinds[3] = {0};
	for (uint64_t i = 0; i < terms; ++i) {
		int kind = fgetc(file);
		assert(kind >= PG_LAMBDA && kind <= PG_REFERENCE);
		items[i] = (struct triple){(uint64_t)kind, word(file), 0};
		if (kind != PG_REFERENCE) items[i].right = word(file);
		++kinds[kind];
	}
	long term_bytes = ftell(file) - start;
	uint64_t unique_roots = 0;
	for (uint64_t i = 0; i < roots; ++i) {
		uint64_t id = word(file);
		assert(id && id <= terms);
		if (!seen[id]) ++unique_roots;
		seen[id] = 1;
	}
	qsort(items, (size_t)terms, sizeof(*items), compare);
	uint64_t duplicates[3] = {0};
	for (uint64_t i = 1; i < terms; ++i)
		if (compare(&items[i-1], &items[i]) == 0) ++duplicates[items[i].kind];
	duplicate_references = duplicates[PG_REFERENCE];
	printf("%s graph objects=%" PRIu64 " binders=%" PRIu64 " terms=%" PRIu64
		" roots=%" PRIu64 " unique_roots=%" PRIu64
		" duplicate_lambda=%" PRIu64 " duplicate_app=%" PRIu64 " duplicate_ref=%" PRIu64
		" lambda=%" PRIu64 " app=%" PRIu64 " ref=%" PRIu64
		" object_bytes=%ld term_bytes=%ld root_bytes=%" PRIu64
		" descriptor_edges=%" PRIu64 " scalars=%" PRIu64 "\n",
		mode, objects, binders, terms, roots, unique_roots,
		duplicates[PG_LAMBDA], duplicates[PG_APPLICATION], duplicates[PG_REFERENCE],
		kinds[PG_LAMBDA], kinds[PG_APPLICATION], kinds[PG_REFERENCE],
		object_bytes, term_bytes, roots * 8, edges, scalars);
	free(items); free(seen);
}

int __real_pg_graph_image_write(FILE *, const char [8], const struct pg_graph_codec *, void *,
	int (*)(FILE *, const struct pg_graph_codec *, void *), void *);

int __wrap_pg_graph_image_write(FILE *file, const char version[8],
	const struct pg_graph_codec *codec, void *owner,
	int (*payload)(FILE *, const struct pg_graph_codec *, void *), void *state)
{
	long start = ftell(file);
	int status = __real_pg_graph_image_write(file, version, codec, owner, payload, state);
	long end = ftell(file);
	if (!status) {
		assert(!fseek(file, start + 8, SEEK_SET));
		uint64_t position = word(file);
		printf("%s envelope source_prefix_bytes=%ld payload_bytes=%" PRIu64 " graph_bytes=%" PRIu64 "\n",
			mode, start, position - (uint64_t)start - 16, (uint64_t)end - position);
		assert(!fseek(file, (long)position, SEEK_SET));
		graph_stats(file);
		assert(ftell(file) == end && !fseek(file, end, SEEK_SET));
	}
	return status;
}

int __real_pg_derivation_inputs_write_inference(FILE *, size_t,
	const struct pg_derivation_input *const *, const struct pg_effect_inference *,
	const struct pg_graph_codec *, void *);

int __wrap_pg_derivation_inputs_write_inference(FILE *file, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner)
{
	long start = ftell(file);
	int status = __real_pg_derivation_inputs_write_inference(file, count, roots, work, codec, owner);
	printf("%s derivations roots=%zu bytes=%ld\n", mode, count, ftell(file) - start);
	return status;
}

int __real_pg_reduction_archive_write(FILE *, const struct pg_reduction_archive *, const struct pg_graph_codec *, void *);

int __wrap_pg_reduction_archive_write(FILE *file, const struct pg_reduction_archive *archive,
	const struct pg_graph_codec *codec, void *owner)
{
	size_t identity = 0, kinds[3] = {0};
	struct triple *keys = calloc(archive->count + 1, sizeof(*keys));
	assert(keys);
	for (size_t i = 0; i < archive->count; ++i) {
		const struct pg_reduction_certificate *c = archive->roots[i];
		identity += c->source == c->target;
		++kinds[c->kind];
		keys[i] = (struct triple){(uint64_t)c->kind, (uintptr_t)c->policy, (uintptr_t)c->source};
	}
	qsort(keys, archive->count, sizeof(*keys), compare);
	size_t duplicates = 0;
	for (size_t i = 1; i < archive->count; ++i) duplicates += compare(&keys[i-1], &keys[i]) == 0;
	free(keys);
	long start = ftell(file);
	int status = __real_pg_reduction_archive_write(file, archive, codec, owner);
	long end = ftell(file);
	assert(!status && !fseek(file, start + 8, SEEK_SET));
	uint64_t records = word(file);
	printf("%s reductions roots=%zu pending_phases=%zu records=%" PRIu64
		" identity=%zu duplicate_keys=%zu whnf=%zu nf=%zu prefix=%zu bytes=%ld\n",
		mode, archive->count, archive->phase_count, records, identity, duplicates,
		kinds[PG_REDUCTION_WHNF], kinds[PG_REDUCTION_NF], kinds[PG_REDUCTION_PREFIX], end-start);
	assert(!fseek(file, end, SEEK_SET));
	return status;
}

static void save(struct pg_program *p, const char *directory, uint64_t step, int retained)
{
	char path[4096];
	mode = retained == 2 ? "nonidentity-trial" : retained ? "retained" : "recompute";
	assert(snprintf(path, sizeof(path), "%s/%" PRIu64 "-%s.a", directory, step, mode) < (int)sizeof(path));
	FILE *file = fopen(path, "w+b");
	assert(file);
	struct pg_graph temporary = {0};
	const struct pg_reduction_archive *archive = retained
		? pg_reduction_archive_snapshot(&temporary, &p->evaluation, p->retained_reductions) : NULL;
	assert(!retained || archive);
	struct pg_reduction_archive filtered = {0};
	if (retained == 2) {
		const struct pg_reduction_certificate **roots = pg_alloc(&temporary, (archive->count + 1) * sizeof(*roots));
		assert(roots);
		filtered = *archive;
		filtered.count = 0;
		for (size_t i = 0; i < archive->count; ++i)
			if (archive->roots[i]->source != archive->roots[i]->target) roots[filtered.count++] = archive->roots[i];
		filtered.roots = roots;
		archive = &filtered;
	}
	assert(!pg_sources_write_retained(file, &p->synthesis, 1, &p->root, archive));
	printf("%s file bytes=%ld\n", mode, ftell(file));
	assert(!fclose(file));
	pg_graph_destroy(&temporary);
}

struct test_roots { size_t count; const struct pg_term *const *terms; };

static int test_write(FILE *file, const struct pg_graph_codec *codec, void *state)
{
	const struct test_roots *roots = state;
	return pg_graph_write_descriptors(file, roots->count, roots->terms, codec, NULL);
}

static int test_read(FILE *file, struct pg_graph *graph, size_t limit, size_t names,
	const struct pg_graph_codec *codec, void *state)
{
	struct test_roots *roots = state;
	return pg_graph_read_descriptors(file, graph, limit, names, codec, NULL, &roots->count, &roots->terms);
}

static int self_test(void)
{
	struct pg_graph input = {0}, output = {0};
	assert(!pg_graph_init(&input) && !pg_graph_init(&output));
	const struct pg_term *ref = pg_reference(&input, pg_binder(&input));
	const struct pg_term *terms[] = {pg_application(&input, ref, ref), ref};
	struct test_roots roots = {2, terms};
	FILE *file = tmpfile();
	assert(file);
	mode = "self-test";
	assert(!pg_graph_image_write(file, "APGAUDT1", NULL, NULL, test_write, &roots));
	/* Current duplicate-wrapper reproduction, not a desired invariant. */
	assert(duplicate_references == 1 && !fseek(file, 0, SEEK_SET));
	assert(!pg_graph_image_read(file, "APGAUDT1", &output, 100, 100, NULL, NULL, test_read, &roots));
	assert(roots.count == 2 && roots.terms[0]->kind == PG_APPLICATION);
	assert(roots.terms[0]->as.application.function == roots.terms[1]);
	assert(roots.terms[0]->as.application.argument == roots.terms[1]);
	assert(output.terms.count == 2 && !fclose(file));
	pg_graph_destroy(&input); pg_graph_destroy(&output);
	puts("duplicate Ref wrapper is merged on read; original sharing survives");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test();
	int load = argc > 1 && !strcmp(argv[1], "--load");
	if (load) { --argc; ++argv; }
	if (argc < 4) { fputs("usage: image_audit [--load] INPUT OUTPUT_DIRECTORY CUMULATIVE_STEPS...\n", stderr); return 2; }
	setvbuf(stdout, NULL, _IOLBF, 0);
	FILE *file = fopen(argv[1], "rb");
	assert(file);
	struct pg_program *p;
	if (load) {
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 3000000, &count, &roots);
		assert(p && count == 1 && !fclose(file));
	} else {
		assert(!fseek(file, 0, SEEK_END));
		long length = ftell(file);
		assert(length >= 0 && !fseek(file, 0, SEEK_SET));
		char *source = malloc((size_t)length + 1);
		assert(source && fread(source, 1, (size_t)length, file) == (size_t)length && !fclose(file));
		p = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
		assert(p);
		p->allow_legacy_intrinsic_dot = 1;
		p->root = pg_program_source(p, p->scope, source, (size_t)length, &p->parser);
		free(source);
	}
	assert(p->root);
	observed_graph = &p->graph;
	for (int i = 3; i < argc; ++i) {
		char *end;
		uint64_t budget = strtoull(argv[i], &end, 10);
		assert(*argv[i] && !*end && budget >= p->synthesis.steps);
		pg_synthesis_advance(&p->synthesis, budget - p->synthesis.steps);
		printf("checkpoint requested=%" PRIu64 " steps=%" PRIu64 " status=%d terms=%zu whnf_jobs=%zu nf_jobs=%zu substitution_terms=%zu\n",
			budget, p->synthesis.steps, pg_synthesis_status(p->root), p->graph.terms.count,
			p->evaluation.jobs.count, p->evaluation.normal_forms.count, substitution_terms);
		uint64_t before = p->synthesis.steps;
		save(p, argv[2], budget, 0);
		save(p, argv[2], budget, 1);
		if (i == argc - 1) save(p, argv[2], budget, 2);
		assert(before == p->synthesis.steps);
	}
	pg_program_destroy(p);
	return 0;
}
