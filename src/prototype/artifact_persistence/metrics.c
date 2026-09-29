#include "source_io.h"
#include "artifact/file.h"
#include "occurrence_io.h"
#include "context_io.h"
#include "retained_io.h"
#include "wire.h"
#include "dag.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int measuring;
static int retention;
static size_t selected_count;
static const struct pg_occurrence **selected;

/* Diagnostic ablations only: excluding an edge here is NOT a codec policy. */
struct closure_edges { int type, origin; };

static int typed_child(void *owner, const void *key, size_t index, const void **child)
{
	const struct closure_edges *edges = owner;
	const struct pg_occurrence *o = key;
	if (index < o->operand_count) { *child = o->operands[index]; return 1; }
	index -= o->operand_count;
	if (!index) { *child = edges->type ? o->type : NULL; return *child ? 1 : 2; }
	if (index == 1) { *child = edges->origin ? o->origin : NULL; return *child ? 1 : 2; }
	index -= 2;
	if (o->map) {
		if (index < o->map->count) { *child = o->map->images[index]; return 1; }
		index -= o->map->count;
	}
	const struct pg_context_map *const *maps = pg_occurrence_maps(o);
	for (size_t i = 0; i < o->map_count; ++i) {
		if (index < maps[i]->count) { *child = maps[i]->images[index]; return 1; }
		index -= maps[i]->count;
	}
	return 0;
}

void __wrap_pg_synthesis_advance(struct pg_synthesis *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

int __wrap_pg_effect_inference_advance(struct pg_effect_inference *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

static void measure(const char *section, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	struct pg_dag terms = {0}, objects = {0};
	struct pg_graph_codec standalone = *codec;
	standalone.roots = NULL;
	assert(!pg_dag_init(&objects, NULL, NULL));
	assert(!pg_graph_dependencies_init(&terms, &objects, &standalone, owner));
	for (size_t i = 0; i < count; ++i) assert(!pg_dag_add(&terms, roots[i]));
	size_t kinds[3] = {0};
	for (const struct pg_dag_node *node = terms.first; node; node = node->next) {
		const struct pg_term *term = node->key;
		assert(term->kind <= PG_REFERENCE);
		++kinds[term->kind];
	}
	printf("%s roots=%zu terms=%zu objects=%zu lambda=%zu application=%zu reference=%zu\n",
		section, count, terms.count, objects.count, kinds[PG_LAMBDA], kinds[PG_APPLICATION], kinds[PG_REFERENCE]);
	size_t binders = 0;
	for (const struct pg_dag_node *node = objects.first; node; node = node->next)
		binders += ((const struct pg_object *)node->key)->kind == PG_BINDER;
	printf("%s binders=%zu semantic_objects=%zu\n", section, binders, objects.count - binders);
	pg_dag_destroy(&terms);
	pg_dag_destroy(&objects);
}

static uint64_t word(FILE *file)
{
	uint64_t result;
	assert(!pg_wire_read_u64(file, &result));
	return result;
}

static void typed_closure(const char *label, size_t count,
	const struct pg_occurrence *const *roots, struct closure_edges edges,
	const struct pg_graph_codec *codec, void *owner)
{
	struct pg_dag typed = {0};
	assert(!pg_dag_init(&typed, typed_child, &edges));
	for (size_t i = 0; i < count; ++i) assert(!pg_dag_add(&typed, roots[i]));
	const struct pg_term **terms = calloc(3 * typed.count + 1, sizeof(*terms));
	assert(terms);
	size_t n = 0, mapped = 0, derived = 0, map_images = 0;
	for (const struct pg_dag_node *node = typed.first; node; node = node->next) {
		const struct pg_occurrence *o = node->key;
		terms[n++] = o->core;
		if (o->classifier) terms[n++] = o->classifier;
		if (o->annotation) terms[n++] = o->annotation;
		mapped += o->map != NULL;
		derived += o->origin && !o->map;
		if (o->map) map_images += o->map->count;
		const struct pg_context_map *const *maps = pg_occurrence_maps(o);
		for (size_t i = 0; i < o->map_count; ++i) map_images += maps[i]->count;
	}
	printf("%s typed=%zu mapped=%zu derived=%zu map_image_edges=%zu\n",
		label, typed.count, mapped, derived, map_images);
	measure(label, n, terms, codec, owner);
	free(terms);
	pg_dag_destroy(&typed);
}

static int premise(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (index == input->count) return 0;
	*child = input->premises[index];
	return 1;
}

int __real_pg_retained_write_semantic_with(FILE *, size_t, const struct pg_derivation_input *const *,
	const struct pg_effect_inference *, size_t, const struct pg_term *const *, size_t,
	const struct pg_occurrence *const *, const struct pg_graph_codec *, void *,
	int (*)(FILE *, const struct pg_graph_codec *, void *, void *), void *);
int __wrap_pg_retained_write_semantic_with(FILE *file, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms, size_t semantic_count,
	const struct pg_occurrence *const *semantic, const struct pg_graph_codec *codec, void *owner,
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *), void *continuation_owner)
{
	long start = ftell(file);
	int result = __real_pg_retained_write_semantic_with(file, count, roots, effects, term_count,
		terms, semantic_count, semantic, codec, owner, continuation, continuation_owner);
	if (result || !measuring) return result;
	long end = ftell(file);
	/* Inspect the just-written terminal table; no duplicate payload parser. */
	assert(start >= 0 && !fseek(file, start + 8, SEEK_SET));
	uint64_t table = word(file);
	assert(table <= (uint64_t)end && !fseek(file, (long)table + 8, SEEK_SET));
	uint64_t objects = word(file), nodes = word(file), references = word(file);
	printf("shared objects=%llu terms=%llu roots=%llu bytes=%llu\n",
		(unsigned long long)objects, (unsigned long long)nodes,
		(unsigned long long)references, (unsigned long long)((uint64_t)end - table));
	assert(!fseek(file, end, SEEK_SET));
	struct pg_dag rules = {0};
	assert(!pg_dag_init(&rules, premise, NULL));
	for (size_t i = 0; i < count; ++i) assert(!pg_dag_add(&rules, roots[i]));
	size_t edges = 0;
	for (const struct pg_dag_node *node = rules.first; node; node = node->next)
		edges += ((const struct pg_derivation_input *)node->key)->count;
	printf("rule_inputs roots=%zu nodes=%zu premise_edges=%zu\n", count, rules.count, edges);
	pg_dag_destroy(&rules);
	return result;
}

int __real_pg_contexts_write_descriptors(FILE *, size_t, const struct pg_context *const *,
	size_t, const struct pg_term *const *, const struct pg_graph_codec *, void *);
int __wrap_pg_contexts_write_descriptors(FILE *file, size_t count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms, const struct pg_graph_codec *codec, void *owner)
{
	if (measuring) measure("occurrence_headers", term_count, terms, codec, owner);
	return __real_pg_contexts_write_descriptors(file, count, contexts, term_count, terms, codec, owner);
}

int __real_pg_graph_write_descriptors(FILE *, size_t, const struct pg_term *const *,
	const struct pg_graph_codec *, void *);
int __wrap_pg_graph_write_descriptors(FILE *file, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	if (measuring) measure(codec->roots ? "payload" : "shared", count, roots, codec, owner);
	long start = ftell(file);
	int result = __real_pg_graph_write_descriptors(file, count, roots, codec, owner);
	if (measuring) printf("section_bytes=%ld\n", ftell(file) - start);
	return result;
}

int __real_pg_occurrences_write_descriptors(FILE *, size_t, const struct pg_occurrence *const *,
	const struct pg_graph_codec *, void *);
int __wrap_pg_occurrences_write_descriptors(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	if (measuring) {
		const struct pg_term **cores = calloc(count ? count : 1, sizeof(*cores));
		assert(cores);
		for (size_t i = 0; i < count; ++i) cores[i] = roots[i]->core;
		measure("semantic_core", count, cores, codec, owner);
		free(cores);
		if (retention) {
			typed_closure("selected_typed_full", selected_count, selected,
				(struct closure_edges){1, 1}, codec, owner);
			typed_closure("typed_without_type_or_origin", count, roots,
				(struct closure_edges){0}, codec, owner);
			typed_closure("typed_without_origin", count, roots,
				(struct closure_edges){.type = 1}, codec, owner);
			typed_closure("typed_without_type", count, roots,
				(struct closure_edges){.origin = 1}, codec, owner);
			typed_closure("typed_full", count, roots,
				(struct closure_edges){1, 1}, codec, owner);
		}
	}
	long start = ftell(file);
	int result = __real_pg_occurrences_write_descriptors(file, count, roots, codec, owner);
	if (!result && measuring) {
		long end = ftell(file);
		assert(start >= 0 && !fseek(file, start + 8, SEEK_SET));
		uint64_t nodes = word(file);
		printf("typed roots=%zu nodes=%llu bytes=%ld\n", count, (unsigned long long)nodes, end - start);
		assert(!fseek(file, end, SEEK_SET));
	}
	return result;
}

int main(int argc, char **argv)
{
	if (argc > 2 && !strcmp(argv[argc - 1], "--retention")) { retention = 1; --argc; }
	assert(argc == 2 || argc == 3);
	unsigned long long input_limit = 0;
	if (argc == 3) {
		char *end;
		input_limit = strtoull(argv[2], &end, 10);
		assert(*argv[2] && !*end && input_limit && input_limit <= SIZE_MAX);
	}
	FILE *input = fopen(argv[1], "rb");
	assert(input);
	size_t count;
	struct pg_synthesis_job *const *roots;
	/* An explicit smaller cap remains available; default matches the CLI. */
	struct pg_program *program = input_limit
		? pg_sources_read(input, (size_t)input_limit, &count, &roots)
		: pg_artifact_read_file(input, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && !program->synthesis.steps);
	if (retention) {
		selected = calloc(count ? count : 1, sizeof(*selected));
		assert(selected);
		for (size_t i = 0; i < count; ++i) {
			const struct pg_occurrence *root;
			int found = pg_synthesis_materialized(roots[i], &root);
			assert(found >= 0);
			if (found) selected[selected_count++] = root;
		}
		printf("selected materialized=%zu exports=%zu\n", selected_count, count);
	}
	size_t nodes = program->graph.terms.count, occurrences = program->typing.occurrences.count;
	size_t proofs = program->typing.proofs.count, jobs = program->synthesis.jobs.count;
	FILE *file = tmpfile();
	assert(file);
	measuring = 1;
	assert(!pg_sources_write(file, &program->synthesis, count, roots));
	printf("image_bytes=%ld\n", ftell(file));
	assert(!program->synthesis.steps && nodes == program->graph.terms.count);
	assert(occurrences == program->typing.occurrences.count && proofs == program->typing.proofs.count);
	assert(jobs == program->synthesis.jobs.count);
	rewind(input); rewind(file);
	int left, right;
	do { left = fgetc(input); right = fgetc(file); assert(left == right); } while (left != EOF);
	assert(!ferror(input) && !ferror(file) && !fclose(input) && !fclose(file));
	pg_program_destroy(program);
	free(selected);
	return 0;
}
