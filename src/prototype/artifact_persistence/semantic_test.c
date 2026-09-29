#define _POSIX_C_SOURCE 200809L
#include "source_io.h"
#include "synthesis_source.h"
#include "artifact/file.h"
#include "computation.h"
#include "retained_io.h"
#include "declaration_io.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int inspect_semantic_root(const struct pg_occurrence *root);
void inspect_semantic_function(const struct pg_occurrence *root);
static int solving;

void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget)
{
	assert(solving);
	__real_pg_synthesis_advance(synthesis, budget);
}

enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(solving);
	return __real_pg_whnf_advance(work, budget);
}

enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(solving);
	return __real_pg_substitution_advance(work, budget);
}

int __real_pg_effect_inference_advance(struct pg_effect_inference *, uint64_t);
int __wrap_pg_effect_inference_advance(struct pg_effect_inference *work, uint64_t budget)
{
	assert(solving);
	return __real_pg_effect_inference_advance(work, budget);
}

static void advance(struct pg_program *program, uint64_t budget)
{
	solving = 1;
	pg_synthesis_advance(&program->synthesis, budget);
	solving = 0;
}

static void equal_files(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int left, right;
	do {
		left = fgetc(a); right = fgetc(b);
		assert(left == right);
	} while (left != EOF);
	assert(!ferror(a) && !ferror(b));
}

static FILE *save(struct pg_program *program, size_t count, struct pg_synthesis_job *const *roots)
{
	size_t terms = program->graph.terms.count, proofs = program->typing.proofs.count;
	size_t occurrences = program->typing.occurrences.count, jobs = program->synthesis.jobs.count;
	uint64_t steps = program->synthesis.steps;
	FILE *file = tmpfile();
	assert(file && !pg_sources_write(file, &program->synthesis, count, roots));
	assert(terms == program->graph.terms.count && proofs == program->typing.proofs.count);
	assert(occurrences == program->typing.occurrences.count && jobs == program->synthesis.jobs.count);
	assert(steps == program->synthesis.steps);
	rewind(file);
	return file;
}

static void no_import_preparation(const struct pg_program *program)
{
	assert(!program->synthesis.steps && program->typing.proofs.count == 1);
	const struct pg_index *jobs = &program->synthesis.jobs;
	for (size_t i = 0; i < jobs->capacity; ++i)
		for (const struct pg_index_entry *entry = jobs->buckets[i]; entry; entry = entry->next)
			assert(!pg_synthesis_derivation_input((const void *)entry));
}

static void finish_work(struct pg_synthesis *s, struct pg_synthesis_job *job)
{
	pg_synthesis_finish(s, job, PG_SYNTHESIS_DONE);
}

static void early_wake(void)
{
	static const struct pg_synthesis_work_class role = {.advance = finish_work};
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *jobs[3];
	for (size_t i = 0; i < 3; ++i) {
		const void *key[] = {&jobs[i]};
		jobs[i] = pg_synthesis_work_request(&p->synthesis, &role, 1, key);
		assert(jobs[i]);
	}
	pg_synthesis_enqueue(&p->synthesis, jobs[1]);
	assert(p->synthesis.ready == jobs[0] && jobs[0]->next == jobs[1] && jobs[1]->next == jobs[2]);
	pg_synthesis_subscribe(&p->synthesis, jobs[1], jobs[0], 0);
	advance(p, 1);
	assert(p->synthesis.ready == jobs[1] && jobs[1]->next == jobs[2]);
	assert(!jobs[1]->dependency && p->synthesis.ready_tail == jobs[2]);
	pg_synthesis_enqueue(&p->synthesis, jobs[1]);
	pg_synthesis_enqueue(&p->synthesis, jobs[2]);
	assert(p->synthesis.ready == jobs[1] && jobs[1]->next == jobs[2] && !jobs[2]->next);
	advance(p, 100);
	assert(!p->synthesis.ready && !p->synthesis.ready_tail && p->synthesis.steps == 3);
	for (size_t i = 0; i < 3; ++i) assert(jobs[i]->status == PG_SYNTHESIS_DONE);
	pg_program_destroy(p);
}

static void lexical_module_roots(void)
{
	const char source[] = "outer := #42;";
	const char nested[] = "outer :: #Int; main := outer;";
	for (size_t selected = 1; selected <= 2; ++selected) {
		struct pg_program *p = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		const struct pg_source_scope *scope;
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&p->synthesis, p->root, &scope, &syntax));
		scope = pg_synthesis_definition_scope(&p->synthesis, scope, syntax);
		struct pg_parser diagnostic;
		struct pg_synthesis_job *child = pg_program_source(p, scope, nested, sizeof(nested) - 1, &diagnostic);
		assert(child && !diagnostic.error);
		advance(p, 100000);
		assert(child->status == PG_SYNTHESIS_DONE && p->root->status == PG_SYNTHESIS_DONE);
		/* Lexical obligations survive even without an exported parent module. */
		struct pg_synthesis_job *initial[] = {child, p->root};
		FILE *file = save(p, selected, initial);
		pg_program_destroy(p);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
		assert(p && count == selected);
		no_import_preparation(p);
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		FILE *copy = save(p, count, roots);
		equal_files(file, copy);
		assert(!fclose(copy) && !fclose(file));
		advance(p, 100000);
		for (size_t i = 0; i < count; ++i) assert(roots[i]->status == PG_SYNTHESIS_DONE);
		pg_program_destroy(p);
	}
}

static void shared_registration_inputs(void)
{
	const char source[] = "first := #1; second := #2;";
	struct pg_program *p = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	assert(p && !pg_synthesis_source_input(&p->synthesis, p->root, &scope, &syntax));
	struct pg_syntax *selections = pg_alloc(&p->graph, 4 * sizeof(*selections));
	struct pg_synthesis_job *initial[2];
	assert(selections && syntax->item_count == 2);
	for (size_t i = 0; i < 2; ++i) {
		selections[i + 2] = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = syntax->items[i].name};
		selections[i] = (struct pg_syntax){.kind = PG_SYNTAX_QUALIFIED,
			.left = syntax, .right = &selections[i + 2]};
		initial[i] = pg_synthesis_request(&p->synthesis, scope, &selections[i]);
		assert(initial[i]);
	}
	advance(p, 10000);
	assert(initial[0]->status == PG_SYNTHESIS_DONE && initial[1]->status == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_prepared_environment(initial[0]) == pg_synthesis_prepared_environment(initial[1]));
	FILE *file = save(p, 2, initial);
	pg_program_destroy(p);
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	assert(header[2] == 2 && header[5] == 2); /* One entry table, not one per selection. */
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	p = pg_sources_read(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(p && count == 2 && roots[0] != roots[1]);
	no_import_preparation(p);
	assert(pg_synthesis_prepared_environment(roots[0]) == pg_synthesis_prepared_environment(roots[1]));
	FILE *copy = save(p, count, roots);
	equal_files(file, copy);
	assert(!fclose(file) && !fclose(copy));
	advance(p, 10000);
	assert(roots[0]->status == PG_SYNTHESIS_DONE && roots[1]->status == PG_SYNTHESIS_DONE);
	pg_program_destroy(p);
}

static struct pg_derivation_input *rule_input(struct pg_graph *g, enum pg_evidence_rule rule,
	size_t count, const struct pg_derivation_input *const *premises)
{
	struct pg_derivation_input *input = pg_alloc(g, sizeof(*input) + count * sizeof(*premises));
	assert(input);
	input->rule = rule; input->count = count;
	for (size_t i = 0; i < count; ++i) input->premises[i] = premises[i];
	return input;
}

static void direct_rule_import(void)
{
	struct pg_program *p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	const struct pg_derivation_input *context = rule_input(&p->graph, PG_CONTEXT_EMPTY, 0, NULL);
	const struct pg_derivation_input *universe = rule_input(&p->graph, PG_UNIVERSE_FORM, 1, &context);
	const struct pg_derivation_input *deep = universe;
	for (size_t i = 0; i < 2048; ++i) {
		const struct pg_derivation_input *premises[] = {context, deep};
		deep = rule_input(&p->graph, PG_CONTEXT_PROJECTION, 2, premises);
	}
	const struct pg_derivation_input *inputs[] = {deep, context, deep, universe,
		rule_input(&p->graph, PG_CONTEXT_EMPTY, 0, NULL),
		rule_input(&p->graph, PG_RETURN_INTRO, 1, &context)};
	struct pg_synthesis_job *const *jobs = NULL;
	assert(!pg_synthesis_import_rules(&p->synthesis, 6, inputs, NULL, &jobs));
	no_import_preparation(p);
	assert(jobs[0] == jobs[2] && jobs[1] == jobs[4]);
	assert(jobs[1] == pg_synthesis_rule(&p->synthesis, context, NULL, NULL, NULL));
	assert(jobs[3] == pg_synthesis_rule(&p->synthesis, universe, &jobs[1], NULL, NULL));
	assert(p->synthesis.jobs.count == 2052 && !pg_synthesis_result(jobs[0]));
	advance(p, 0);
	no_import_preparation(p);
	advance(p, 100000);
	assert(pg_synthesis_status(jobs[0]) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(pg_synthesis_result(jobs[0]))->core == pg_universe(&p->graph, 0));
	assert(pg_synthesis_status(jobs[5]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(jobs[5]));
	struct pg_synthesis_job *const *again;
	size_t count = p->synthesis.jobs.count;
	uint64_t steps = p->synthesis.steps;
	assert(!pg_synthesis_import_rules(&p->synthesis, 6, inputs, NULL, &again));
	assert(count == p->synthesis.jobs.count && steps == p->synthesis.steps);
	for (size_t i = 0; i < 6; ++i) assert(again[i] == jobs[i]);
	pg_program_destroy(p);
	p = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(p);
	context = rule_input(&p->graph, PG_CONTEXT_EMPTY, 0, NULL);
	struct pg_derivation_input *cycle = rule_input(&p->graph, PG_UNIVERSE_FORM, 1, &context);
	cycle->premises[0] = cycle;
	const struct pg_derivation_input *cyclic = cycle;
	again = NULL;
	assert(pg_synthesis_import_rules(&p->synthesis, 1, &cyclic, NULL, &again) && !again);
	no_import_preparation(p);
	pg_program_destroy(p);
	puts("direct rule import: shared iterative relocation, no preparation workers or proof admission");
}

static void cycles(uint64_t budget)
{
	const char *source = "Nat := @{zero:*;succ:*->*;}; id := \\x:Nat=>x; main := id Nat.zero;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token main = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected = pg_program_select_name(program, program->root, main);
	assert(selected);
	struct pg_synthesis_job *initial[] = {selected, program->root, selected};
	struct pg_synthesis_job *const *roots = initial;
	size_t count = 3;
	advance(program, budget);
	const struct pg_occurrence *before;
	int available = pg_synthesis_materialized(roots[0], &before);
	assert(available >= 0 && inspect_semantic_root(before) == available);
	if (!budget) assert(!available);
	if (budget == 100000) assert(available && pg_synthesis_result(selected));
	FILE *file = save(program, count, roots);
	size_t unchanged = 7;
	struct pg_synthesis_job *const *missing = NULL;
	assert(!pg_sources_read(file, 1, &unchanged, &missing) && unchanged == 7 && !missing);
	rewind(file);
	for (size_t i = 0; i < 3; ++i) {
		pg_program_destroy(program);
		program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
		assert(program && count == 3 && roots[0] == roots[2]);
		no_import_preparation(program);
		assert(!program->synthesis.steps && !pg_synthesis_result(roots[0]));
		const struct pg_occurrence *view;
		size_t proofs = program->typing.proofs.count, jobs = program->synthesis.jobs.count;
		assert(pg_synthesis_materialized(roots[0], &view) == available);
		assert(inspect_semantic_root(view) == available);
		assert(proofs == program->typing.proofs.count && jobs == program->synthesis.jobs.count);
		FILE *again = save(program, count, roots);
		equal_files(file, again);
		assert(!fclose(file));
		file = again;
		rewind(file);
	}
	advance(program, 100000);
	assert(pg_synthesis_result(roots[0]) && pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void file_extent(void)
{
	const char source[] = "main := #0;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	FILE *image = save(program, 1, &program->root);
	struct pg_program *source_program = program;
	assert(!fseek(image, 0, SEEK_END));
	long end = ftell(image);
	assert(end > 0);
	size_t size = (size_t)end;
	unsigned char *bytes = malloc(size + 1);
	assert(bytes);
	rewind(image);
	assert(fread(bytes, 1, size, image) == size);
	for (size_t prefix = 0; prefix <= 17; prefix += 17) {
		FILE *file = tmpfile();
		assert(file);
		for (size_t i = 0; i < prefix; ++i) assert(fputc(0, file) == 0);
		assert(!pg_sources_write(file, &source_program->synthesis, 1, &source_program->root));
		assert(!fseek(file, (long)prefix, SEEK_SET));
		size_t count;
		struct pg_synthesis_job *const *roots;
		program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
		assert(program && count == 1 && !program->synthesis.steps && !pg_synthesis_result(roots[0]));
		FILE *again = save(program, count, roots);
		equal_files(image, again);
		assert(!fclose(file) && !fclose(again));
		pg_program_destroy(program);
	}
	pg_program_destroy(source_program);
	bytes[size] = 0;
	for (size_t cut = 0; cut <= size + 1; ++cut) {
		if (cut == size) continue;
		FILE *file = tmpfile();
		assert(file && fwrite(bytes, 1, cut, file) == cut);
		rewind(file);
		size_t count = 37;
		struct pg_synthesis_job *const *roots = NULL;
		assert(!pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots) && count == 37 && !roots);
		assert(!fclose(file));
	}
	free(bytes);
	assert(!fclose(image));
}

static void restored_inputs_only(void)
{
	struct pg_program *program = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->synthesis.ready);
	FILE *file = save(program, 0, NULL);
	pg_program_destroy(program);
	size_t count;
	struct pg_synthesis_job *const *roots;
	program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && !count && !program->synthesis.ready && !program->synthesis.steps);
	advance(program, 100000);
	assert(!program->synthesis.steps);
	FILE *again = save(program, 0, NULL);
	equal_files(file, again);
	assert(!fclose(file) && !fclose(again));
	pg_program_destroy(program);

	const char source[] = "main := #int_add #1 #2;";
	program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	file = save(program, 1, &program->root);
	pg_program_destroy(program);
	program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && count == 1 && !program->synthesis.steps);
	const struct pg_source_scope *ambient;
	const struct pg_syntax *syntax;
	assert(!pg_synthesis_source_input(&program->synthesis, roots[0], &ambient, &syntax));
	/* The REPL extends the restored lexical scope, not a fresh intrinsic scope. */
	struct pg_parser parser;
	const char extra[] = "later := \\x : #Int => #int_add x #1;";
	struct pg_synthesis_job *later = pg_program_source(program, ambient,
		extra, sizeof(extra) - 1, &parser);
	assert(later && !parser.error && !program->synthesis.steps);
	advance(program, 100000);
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(later) == PG_SYNTHESIS_DONE);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void fixed_limit_and_publication(void)
{
	const char source[] = "main := #0;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	FILE *file = save(program, 1, &program->root);
	size_t low = 0, high = PG_ARTIFACT_DEFAULT_LIMIT;
	while (low + 1 < high) {
		size_t limit = low + (high - low) / 2, count = 17;
		struct pg_synthesis_job *const *roots = NULL;
		rewind(file);
		struct pg_program *loaded = pg_artifact_read_file(file, limit, &count, &roots);
		if (loaded) {
			assert(count == 1 && !loaded->synthesis.steps);
			high = limit;
			pg_program_destroy(loaded);
		} else { assert(count == 17 && !roots); low = limit; }
	}
	/* Policy changes must not change the transported graph or run checks. */
	const size_t limits[] = {high, PG_ARTIFACT_DEFAULT_LIMIT, SIZE_MAX};
	for (size_t i = 0; i < sizeof(limits) / sizeof(*limits); ++i) {
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		struct pg_program *loaded = pg_artifact_read_file(file, limits[i], &count, &roots);
		assert(loaded && !loaded->synthesis.steps);
		FILE *copy = save(loaded, count, roots);
		equal_files(file, copy);
		assert(!fclose(copy));
		pg_program_destroy(loaded);
	}
	char path[] = "/tmp/a-program-publication-XXXXXX";
	int descriptor = mkstemp(path);
	assert(descriptor >= 0 && !close(descriptor));
	assert(!pg_artifact_save_file(path, program, 1, &program->root, PG_ARTIFACT_MATERIALIZED));
	/* A failed replacement must leave the previous artifact untouched. */
	assert(pg_artifact_save_file(NULL, program, 0, NULL, PG_ARTIFACT_MATERIALIZED));
	assert(pg_artifact_save_file(path, NULL, 0, NULL, PG_ARTIFACT_MATERIALIZED));
	assert(pg_artifact_save_file(path, program, 1, NULL, PG_ARTIFACT_MATERIALIZED));
	FILE *saved = fopen(path, "rb");
	assert(saved);
	equal_files(file, saved);
	assert(!fclose(saved) && !unlink(path) && !fclose(file));
	pg_program_destroy(program);
}

static void recompute_profile(const char *source, uint64_t budget)
{
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected_root = pg_program_select_name(program, program->root, name);
	assert(selected_root);
	struct pg_synthesis_job *selection[] = {program->root, selected_root, selected_root};
	advance(program, budget);
	size_t terms = program->graph.terms.count, proofs = program->typing.proofs.count;
	size_t occurrences = program->typing.occurrences.count, jobs = program->synthesis.jobs.count;
	uint64_t steps = program->synthesis.steps;
	FILE *file = tmpfile();
	assert(file && !pg_sources_write_inputs(file, &program->synthesis, 3, selection));
	assert(terms == program->graph.terms.count && proofs == program->typing.proofs.count);
	assert(occurrences == program->typing.occurrences.count && jobs == program->synthesis.jobs.count);
	assert(steps == program->synthesis.steps);
	/* Establish the eventual status independently of the exported progress. */
	advance(program, 100000);
	enum pg_synthesis_status expected = pg_synthesis_status(program->root);
	assert(expected == PG_SYNTHESIS_DONE || expected == PG_SYNTHESIS_REJECTED);
	pg_program_destroy(program);
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && count == 3 && roots[1] == roots[2] && !program->synthesis.steps);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *subject;
		assert(!pg_synthesis_materialized(roots[i], &subject) && !subject);
	}
	FILE *copy = save(program, count, roots);
	equal_files(file, copy);
	assert(!fclose(copy) && !fclose(file));
	advance(program, 100000);
	assert(pg_synthesis_status(roots[0]) == expected);
	assert(pg_synthesis_status(roots[1]) == expected);
	pg_program_destroy(program);
}

static void semantic_versions(void)
{
	struct pg_program *program = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(program);
	FILE *file = tmpfile();
	assert(file && !pg_retained_write_semantic(file, 0, NULL, &program->imported_effects,
		0, NULL, 0, NULL, NULL, NULL));
	for (unsigned version = 0; version <= 5; ++version) {
		if (version == 4) continue;
		assert(!fseek(file, 6, SEEK_SET) && fputc(version, file) != EOF);
		rewind(file);
		size_t count = 17, term_count = 19, semantic_count = 23;
		const struct pg_derivation_input *const *rules = NULL;
		const struct pg_term *const *terms = NULL;
		const struct pg_occurrence *const *semantic = NULL;
		size_t nodes = program->graph.terms.count, proofs = program->typing.proofs.count;
		assert(pg_retained_read_semantic(file, &program->typing, 4096, 4096,
			&program->imported_effects, NULL, NULL, &count, &rules, &term_count, &terms,
			&semantic_count, &semantic) == -1);
		assert(count == 17 && term_count == 19 && semantic_count == 23);
		assert(!rules && !terms && !semantic && !program->synthesis.steps);
		assert(nodes == program->graph.terms.count && proofs == program->typing.proofs.count);
	}
	assert(!fseek(file, 6, SEEK_SET) && fputc(4, file) != EOF);
	rewind(file);
	size_t count, term_count, semantic_count;
	const struct pg_derivation_input *const *rules;
	const struct pg_term *const *terms;
	const struct pg_occurrence *const *semantic;
	assert(!pg_retained_read_semantic(file, &program->typing, 4096, 4096,
		&program->imported_effects, NULL, NULL, &count, &rules, &term_count, &terms,
		&semantic_count, &semantic));
	assert(!count && !term_count && !semantic_count && !program->synthesis.steps);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void untrusted_input(void)
{
	const char *source = "main := #0 :: #Text;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	const struct pg_evidence *proof = pg_prove_universe(&program->typing,
		pg_prove_empty_context(&program->typing), 0);
	assert(proof);
	const struct pg_occurrence *forged = pg_evidence_subject(proof);
	struct pg_program *foreign = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(foreign && pg_synthesis_import_materialized(&foreign->synthesis, program->root, forged));
	assert(pg_synthesis_import_completion(&foreign->synthesis, program->root));
	pg_program_destroy(foreign);
	assert(!pg_synthesis_import_materialized(&program->synthesis, program->root, forged));
	assert(!pg_synthesis_import_completion(&program->synthesis, program->root));
	const struct pg_evidence *other = pg_prove_universe(&program->typing,
		pg_prove_empty_context(&program->typing), 1);
	assert(other && pg_synthesis_import_materialized(&program->synthesis, program->root, pg_evidence_subject(other)));
	assert(!pg_synthesis_result(program->root));
	FILE *file = save(program, 1, &program->root);
	pg_program_destroy(program);
	size_t count = 0;
	struct pg_synthesis_job *const *roots;
	program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && count == 1 && !program->synthesis.steps && !pg_synthesis_result(roots[0]));
	const struct pg_occurrence *view;
	assert(pg_synthesis_materialized(roots[0], &view) == 1 && inspect_semantic_root(view));
	assert(pg_synthesis_saved_complete(&program->synthesis, roots[0]));
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	assert(!pg_artifact_trusted_export(program, roots[0], name, &view));
	advance(program, 100000);
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_result(roots[0]));
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void materialized_validation(const char *source)
{
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *selected = pg_program_select_name(program, program->root, name);
	assert(selected);
	advance(program, 100000);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(proof);
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	const struct pg_evidence *unrelated = pg_prove_universe(&program->typing,
		pg_prove_empty_context(&program->typing), 42);
	assert(unrelated);
	const struct pg_occurrence *stored[] = {subject, pg_evidence_subject(unrelated)};
	struct pg_graph storage;
	struct pg_effect_inference effects;
	struct pg_declaration_io codec;
	assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&effects, &storage));
	assert(!pg_declaration_io_init(&codec, &program->typing));
	const struct pg_derivation_input *const *inputs;
	assert(!pg_synthesis_export_rules(&program->synthesis, 1, &selected, &storage, &effects, 1, &inputs));
	FILE *file = tmpfile();
	assert(file && !pg_retained_write_semantic(file, 1, inputs, &effects,
		0, NULL, 2, stored, &pg_declaration_graph_codec, &codec));
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects);
	pg_graph_destroy(&storage);
	pg_program_destroy(program);

	/* No source scope or intrinsic installation: check only the retained inputs. */
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_whnf_work evaluation;
	struct pg_synthesis synthesis;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_whnf_work_init(&evaluation, &graph));
	assert(!pg_synthesis_init(&synthesis, &typing, &evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	assert(!pg_effect_inference_init(&effects, &graph) && !pg_declaration_io_init(&codec, &typing));
	size_t count, term_count, semantic_count;
	const struct pg_term *const *terms;
	const struct pg_occurrence *const *subjects;
	rewind(file);
	assert(!pg_retained_read_semantic(file, &typing, 100000, 100000, &effects,
		&pg_declaration_graph_codec, &codec, &count, &inputs, &term_count, &terms, &semantic_count, &subjects));
	assert(count == 1 && semantic_count == 2 && !term_count && !typing.proofs.count && !synthesis.steps);
	struct pg_synthesis_job *checking = pg_synthesis_derivation(&synthesis, inputs[0]);
	assert(checking && !pg_synthesis_result(checking) && !synthesis.steps);
	solving = 1;
	pg_synthesis_advance(&synthesis, 100000);
	solving = 0;
	proof = pg_synthesis_result(checking);
	assert(proof && pg_evidence_subject(proof) == subjects[0] && synthesis.steps);
	assert(pg_evidence_subject(proof) != subjects[1] && !pg_evidence_for_subject(&typing, subjects[1], NULL));
	printf("materialized validation: %llu charged steps, exact typed subject, no source reconstruction\n",
		(unsigned long long)synthesis.steps);
	assert(!fclose(file));
	pg_declaration_io_destroy(&codec);
	pg_synthesis_destroy(&synthesis);
	pg_effect_inference_destroy(&effects);
	pg_whnf_work_destroy(&evaluation);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

static void rejected_sibling(void)
{
	const char *source = "ok := \\x:#Int => x; bad := #0 :: #Text;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "ok", .length = 2};
	struct pg_synthesis_job *checked = pg_program_select_name(program, program->root, name);
	assert(checked);
	struct pg_synthesis_job *local = NULL;
	while (!local || !pg_synthesis_result(local)) {
		assert(program->synthesis.steps < 100000);
		advance(program, 1);
		local = pg_synthesis_definition(program->root, name);
	}
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(checked));
	struct pg_synthesis_job *initial[] = {local, checked, program->root};
	FILE *file = save(program, 3, initial);
	pg_program_destroy(program);
	size_t count;
	struct pg_synthesis_job *const *roots;
	program = pg_sources_read(file, 100000, &count, &roots);
	assert(program && count == 3 && !program->synthesis.steps);
	const struct pg_occurrence *view;
	assert(pg_synthesis_materialized(roots[0], &view) == 1 && inspect_semantic_root(view));
	assert(pg_synthesis_saved_complete(&program->synthesis, roots[0]));
	assert(!pg_artifact_trusted_export(program, roots[2], name, &view));
	assert(!pg_artifact_trusted_export(program, roots[0], name, &view));
	FILE *before = save(program, count, roots);
	size_t proofs = program->typing.proofs.count, terms = program->graph.terms.count;
	inspect_semantic_function(view);
	assert(proofs == program->typing.proofs.count && terms == program->graph.terms.count);
	assert(!program->synthesis.steps);
	FILE *after = save(program, count, roots);
	equal_files(before, after);
	assert(!fclose(before) && !fclose(after));
	assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
	advance(program, 100000);
	assert(pg_synthesis_result(roots[0]));
	assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(roots[1]));
	assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void unrelated_evaluation(void)
{
	const char *source = "main := #0;";
	struct pg_program *program = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	advance(program, 100000);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	FILE *before = save(program, 1, &program->root);
	size_t jobs = program->evaluation.jobs.count;
	for (size_t i = 0; i < 32; ++i) {
		const struct pg_object *binder = pg_binder(&program->graph);
		const struct pg_term *value = pg_reference(&program->graph, pg_binder(&program->graph));
		const struct pg_term *identity = pg_lambda(&program->graph, binder, pg_reference(&program->graph, binder));
		const struct pg_term *term = pg_application(&program->graph, identity, value);
		struct pg_whnf_job *work = pg_whnf_request(&program->evaluation, &pg_pure_policy, term);
		assert(work);
		solving = 1;
		assert(pg_whnf_advance(work, 10000) == PG_EVAL_WHNF);
		solving = 0;
		assert(pg_whnf_result(work) == value);
	}
	assert(program->evaluation.jobs.count > jobs);
	FILE *after = save(program, 1, &program->root);
	equal_files(before, after);
	assert(!fclose(before) && !fclose(after));
	pg_program_destroy(program);
}

static FILE *effect_image(struct pg_program *program)
{
	struct pg_declaration_io codec;
	assert(!pg_declaration_io_init(&codec, &program->typing));
	FILE *file = tmpfile();
	assert(file && !pg_retained_write_semantic(file, 0, NULL, &program->imported_effects,
		0, NULL, 0, NULL, &pg_declaration_graph_codec, &codec));
	pg_declaration_io_destroy(&codec);
	rewind(file);
	return file;
}

static void effect_cycles(void)
{
	struct pg_program *program = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(program);
	const struct pg_effect_row *empty = pg_effect_row(&program->graph, 0, NULL);
	struct pg_effect_equation *equations[8];
	for (size_t i = 0; i < 8; ++i) {
		equations[i] = pg_effect_equation(&program->imported_effects, empty);
		assert(equations[i]);
	}
	for (size_t i = 0; i < 8; ++i) {
		assert(!pg_effect_dependency(&program->imported_effects, equations[i], empty, equations[(i + 1) % 8]));
		assert(!pg_effect_dependency(&program->imported_effects, equations[0], empty, equations[i]));
	}
	const struct pg_term *constant = pg_effect_reference(&program->graph, empty);
	assert(!pg_effect_contribution(&program->imported_effects, constant, empty, equations[0]));
	FILE *file = effect_image(program);
	assert(!pg_effect_contribution(&program->imported_effects, constant, empty, equations[0]));
	assert(!pg_effect_dependency(&program->imported_effects, equations[0], empty, equations[1]));
	FILE *again = effect_image(program);
	equal_files(file, again);
	assert(!fclose(again));
	pg_effect_inference_seal(&program->imported_effects);
	solving = 1;
	assert(!pg_effect_inference_advance(&program->imported_effects, 1));
	solving = 0;
	again = effect_image(program);
	equal_files(file, again);
	assert(!fclose(again));
	solving = 1;
	assert(pg_effect_inference_advance(&program->imported_effects, 100) == 1);
	solving = 0;
	again = effect_image(program);
	equal_files(file, again);
	assert(!fclose(again));
	rewind(file);
	for (size_t cycle = 0; cycle < 12; ++cycle) {
		pg_program_destroy(program);
		program = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
		assert(program);
		/* Unrelated allocations change addresses, not the saved definition order. */
		for (size_t i = 0; i < 19 * cycle; ++i) assert(pg_binder(&program->graph));
		struct pg_declaration_io codec;
		assert(!pg_declaration_io_init(&codec, &program->typing));
		size_t proofs = program->typing.proofs.count;
		size_t count, term_count, semantic_count;
		const struct pg_derivation_input *const *rules;
		const struct pg_term *const *terms;
		const struct pg_occurrence *const *semantic;
		assert(!pg_retained_read_semantic(file, &program->typing, 10000, 10000, &program->imported_effects,
			&pg_declaration_graph_codec, &codec, &count, &rules, &term_count, &terms, &semantic_count, &semantic));
		assert(!count && !term_count && !semantic_count && !program->synthesis.steps);
		assert(!program->imported_effects.sealed && program->typing.proofs.count == proofs);
		pg_declaration_io_destroy(&codec);
		FILE *again = effect_image(program);
		equal_files(file, again);
		assert(!fclose(file));
		file = again;
		rewind(file);
	}
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void limit_arguments(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	assert(!pg_wire_array(&graph, UINT64_MAX, sizeof(uint64_t)));
	assert(!pg_wire_array(&graph, 1, 0));
	assert(pg_wire_array(&graph, 0, sizeof(uint64_t)));
	uint64_t *array = pg_wire_array(&graph, 2, sizeof(*array));
	assert(array && !array[0] && !array[1]);
	pg_graph_destroy(&graph);
	size_t limit = 17;
	assert(!pg_artifact_limit_argument("none", &limit) && limit == SIZE_MAX);
	assert(!pg_artifact_limit_argument("1000000", &limit) && limit == PG_ARTIFACT_DEFAULT_LIMIT);
	assert(!pg_artifact_limit_argument("0001", &limit) && limit == 1);
	char maximum[3 * sizeof(size_t) + 1];
	assert(snprintf(maximum, sizeof(maximum), "%zu", SIZE_MAX) > 0);
	assert(!pg_artifact_limit_argument(maximum, &limit) && limit == SIZE_MAX);
	const char *invalid[] = {NULL, "", "0", "000", "-1", "+1", " 1", "1 ",
		"1x", "None", "unlimited", "184467440737095516160"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		limit = 17;
		assert(pg_artifact_limit_argument(invalid[i], &limit) && limit == 17);
	}
	assert(pg_artifact_limit_argument("none", NULL));
}

static void trusted_export(const char *source, int valid, int inputs_only, const char *provider)
{
	struct pg_program *program = provider ? pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK)
		: pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
	assert(program);
	if (provider) {
		struct pg_parser diagnostic;
		struct pg_synthesis_job *library = pg_program_source(program, program->scope,
			provider, strlen(provider), &diagnostic);
		assert(library && !diagnostic.error);
		const struct pg_source_scope *exports = pg_program_exports(program, program->scope, library);
		const struct pg_source_scope *scope = pg_synthesis_import_scope(&program->synthesis, program->scope, exports);
		program->root = pg_program_source(program, scope, source, strlen(source), &diagnostic);
		assert(program->root && !diagnostic.error);
	}
	assert(program->root);
	advance(program, 100000);
	assert((pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE) == valid);
	FILE *file = tmpfile();
	assert(file);
	if (inputs_only) assert(!pg_sources_write_inputs(file, &program->synthesis, 1, &program->root));
	else assert(!pg_sources_write(file, &program->synthesis, 1, &program->root));
	pg_program_destroy(program);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *const *roots;
	size_t count;
	for (size_t cycle = 0; cycle < 3; ++cycle) {
		rewind(file);
		program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
		assert(program && count == 1 && !program->synthesis.steps);
		assert(!pg_synthesis_result(roots[0]));
		size_t terms = program->graph.terms.count, proofs = program->typing.proofs.count;
		size_t occurrences = program->typing.occurrences.count, jobs = program->synthesis.jobs.count;
		const struct pg_occurrence *subject = NULL;
		int expected = valid && !inputs_only;
		assert(pg_artifact_trusted_export(program, roots[0], name, &subject) == expected);
		assert((subject != NULL) == expected);
		assert(pg_artifact_trusted_export(program, roots[0],
			(struct pg_token){.text = "absent", .length = 6}, &subject) == 0);
		assert((subject != NULL) == expected);
		assert(terms == program->graph.terms.count && proofs == program->typing.proofs.count);
		assert(occurrences == program->typing.occurrences.count && jobs == program->synthesis.jobs.count);
		assert(!program->synthesis.steps && !pg_synthesis_result(roots[0]));
		FILE *again = save(program, count, roots);
		equal_files(file, again);
		assert(!fclose(file));
		file = again;
		/* Explicit recomputation cannot retain a borrowed trust shortcut. */
		advance(program, 1);
		assert(!pg_artifact_trusted_export(program, roots[0], name, &subject));
		assert(!pg_synthesis_saved_complete(&program->synthesis, roots[0]));
		advance(program, 100000);
		assert((pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE) == valid);
		pg_program_destroy(program);
	}
	assert(!fclose(file));
}

static void completion_format(void)
{
	const char source[] = "main := #0;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	FILE *file = save(program, 1, &program->root);
	/* Locate the first producer's completion byte through the source header. */
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	assert(header[4]);
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t words[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &words[j]));
		assert(!fseek(file, (long)words[6], SEEK_CUR));
	}
	assert(!fseek(file, (long)(8 * header[2] + 56), SEEK_CUR));
	long flag = ftell(file);
	assert(flag >= 0 && fgetc(file) == 0);
	assert(!fseek(file, flag, SEEK_SET) && fputc(2, file) != EOF);
	rewind(file);
	size_t count = 91;
	struct pg_synthesis_job *const *roots = NULL;
	assert(!pg_sources_read(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots));
	assert(count == 91 && !roots);
	assert(!fseek(file, flag, SEEK_SET) && fputc(0, file) != EOF);
	assert(!fseek(file, 6, SEEK_SET) && fputc(65, file) != EOF);
	rewind(file);
	assert(!pg_sources_read(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots));
	assert(count == 91 && !roots);
	assert(!fclose(file));
	pg_program_destroy(program);
}

static void revalidation_budget(void)
{
	const char source[] = "main := #int_add #20 #22;";
	struct pg_program *program = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(program && program->root);
	advance(program, 100000);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	FILE *file = save(program, 1, &program->root);
	pg_program_destroy(program);
	const uint64_t budgets[][2] = {{0, 0}, {20, 0}, {0, 20}, {7, 3}, {3, 7}, {7, UINT64_MAX}, {UINT64_MAX, 7}};
	for (size_t i = 0; i < sizeof(budgets) / sizeof(*budgets); ++i) {
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
		assert(program && count == 1 && !program->synthesis.steps);
		uint64_t expected = budgets[i][0] < budgets[i][1] ? budgets[i][0] : budgets[i][1];
		uint64_t spent = UINT64_MAX;
		solving = expected != 0;
		assert(pg_artifact_revalidate(program, roots[0], budgets[i][0], budgets[i][1], &spent) == PG_SYNTHESIS_PENDING);
		solving = 0;
		assert(spent == expected && program->synthesis.steps == expected);
		assert(!pg_synthesis_result(roots[0]));
		if (!expected) {
			FILE *copy = save(program, count, roots);
			equal_files(file, copy);
			assert(!fclose(copy));
		}
		pg_program_destroy(program);
	}
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	program = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(program && count == 1);
	struct pg_program *foreign = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(foreign && foreign->root);
	uint64_t spent = 91;
	assert(pg_artifact_revalidate(program, foreign->root, 100, 100, &spent) == PG_SYNTHESIS_ERROR);
	assert(spent == 91 && !program->synthesis.steps && !foreign->synthesis.steps);
	assert(pg_artifact_revalidate(program, roots[0], 100, 100, NULL) == PG_SYNTHESIS_ERROR);
	pg_program_destroy(foreign);
	solving = 1;
	assert(pg_artifact_revalidate(program, roots[0], 100000, UINT64_MAX, &spent) == PG_SYNTHESIS_DONE);
	solving = 0;
	assert(spent && spent < 100000 && spent == program->synthesis.steps);
	uint64_t validation_spent = spent;
	/* A completed target costs nothing even if unrelated work is still ready. */
	assert(pg_artifact_revalidate(program, roots[0], 100000, 100000, &spent) == PG_SYNTHESIS_DONE);
	assert(!spent && program->synthesis.steps == validation_spent);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	struct pg_synthesis_job *result = pg_program_evaluate_name(program, roots[0], name, 0);
	assert(result);
	advance(program, 100000 - validation_spent);
	assert(program->synthesis.steps <= 100000 && pg_synthesis_status(result) == PG_SYNTHESIS_DONE);
	pg_program_destroy(program);
	assert(!fclose(file));
}

int main(void)
{
	early_wake();
	lexical_module_roots();
	shared_registration_inputs();
	direct_rule_import();
	limit_arguments();
	completion_format();
	revalidation_budget();
	trusted_export("main := #42; main :: #Int;", 1, 0, NULL);
	trusted_export("main := #42; main :: #Int;", 1, 1, NULL);
	trusted_export("main := \\x:#Int => x;", 1, 0, NULL);
	trusted_export("main := #42; main :: #Text;", 0, 0, NULL);
	trusted_export("main := #42; invalid := #0 :: @;", 0, 0, NULL);
	trusted_export("import value; main := value;", 1, 0, "value := #42;");
	trusted_export("import value; main := value;", 0, 0, "value := #42; invalid := #0 :: @;");
	for (uint64_t cut = 0; cut <= 200; ++cut) cycles(cut);
	cycles(100000);
	file_extent();
	fixed_limit_and_publication();
	const char *recompute[] = {
		"Nat := @{zero:*;succ:*->*;}; main := \\n:Nat => n @zero => Nat.zero @succ k => Nat.succ (*k);",
		"main := #int_add #1 #2;",
		"main := #1; invalid := #0 :: @;"
	};
	for (size_t i = 0; i < sizeof(recompute) / sizeof(*recompute); ++i) {
		recompute_profile(recompute[i], 0);
		recompute_profile(recompute[i], 100);
		recompute_profile(recompute[i], 100000);
	}
	restored_inputs_only();
	semantic_versions();
	untrusted_input();
	materialized_validation("main := @;");
	materialized_validation("main := \\A : @ => A;");
	materialized_validation("Nat := @{zero:*;succ:*->*;}; main := Nat.succ Nat.zero;");
	materialized_validation("Nat := @{zero:*;succ:*->*;}; main := \\n:Nat => n @zero => Nat.zero @succ k => Nat.succ (*k);");
	rejected_sibling();
	unrelated_evaluation();
	effect_cycles();
	puts("semantic source image: inert views, aliases, stable cycles, untrusted inputs and module obligations passed");
	return 0;
}
