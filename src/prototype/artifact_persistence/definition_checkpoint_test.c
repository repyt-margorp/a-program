#include "program.h"
#include "source_io.h"
#include "syntax_io.h"
#include "synthesis_source.h"
#include "artifact/schedule.h"
#include "artifact/file.h"
#include "wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Known-origin source-owner fixtures, not a public checkpoint file format.
 * Decode is inert. Completed children are then checked through ordinary Solve;
 * their saved status is compared, never imported as accepted evidence. */
static int solving;
static uint64_t checked_dispatches;
static size_t body_edges, module_cursors;
void __real_pg_synthesis_advance(struct pg_synthesis *, uint64_t);
void __wrap_pg_synthesis_advance(struct pg_synthesis *s, uint64_t steps)
{
	assert(solving);
	__real_pg_synthesis_advance(s, steps);
}

static void advance(struct pg_synthesis *s, uint64_t steps)
{
	solving = 1;
	pg_synthesis_advance(s, steps);
	solving = 0;
}

struct fixture {
	struct pg_program *program;
	const struct pg_syntax *syntax;
	struct pg_synthesis_job *registration, *unselected;
	struct pg_definition_frontier frontier;
	struct pg_synthesis_job **jobs;
	size_t count;
	uint64_t validation;
};

static void add_job(struct fixture *f, struct pg_synthesis_job *job)
{
	if (!job) return;
	for (size_t i = 0; i < f->count; ++i) if (f->jobs[i] == job) return;
	f->jobs[f->count++] = job;
}

static void collect(struct fixture *f)
{
	assert(!pg_synthesis_definition_frontier(&f->program->synthesis, f->registration, &f->frontier));
	struct pg_module_frontier module;
	assert(!pg_synthesis_module_frontier(&f->program->synthesis, f->program->root, &module));
	if (f->syntax->kind == PG_SYNTAX_QUALIFIED && module.position) f->unselected = module.previous;
	f->jobs = calloc(3 * f->frontier.count + 3, sizeof(*f->jobs));
	assert(f->jobs);
	add_job(f, f->program->root); add_job(f, f->registration);
	add_job(f, f->unselected);
	for (size_t i = 0; i < f->frontier.count; ++i) add_job(f, f->frontier.entries[i]);
	for (size_t i = 0; i < f->frontier.count; ++i) {
		const struct pg_source_scope *scope;
		struct pg_synthesis_job *term, *type;
		if (!pg_synthesis_definition_body(&f->program->synthesis, f->frontier.entries[i], &term)) add_job(f, term);
		if (!pg_synthesis_source_expect_input(&f->program->synthesis, f->frontier.entries[i], &scope, &term, &type)) {
			add_job(f, term); add_job(f, type);
		}
	}
}

static FILE *save(struct fixture *f)
{
	struct pg_synthesis *s = &f->program->synthesis;
	size_t proofs = f->program->typing.proofs.count, terms = f->program->graph.terms.count;
	FILE *file = tmpfile();
	assert(file && !pg_syntax_write(file, 1, &f->syntax));
	assert(!pg_wire_write_u64(file, f->frontier.indexed));
	assert(!pg_wire_write_u64(file, f->frontier.activated));
	assert(!pg_wire_write_u64(file, f->frontier.complete));
	struct pg_module_frontier module;
	assert(!pg_synthesis_module_frontier(s, f->program->root, &module));
	assert(!pg_wire_write_u64(file, module.position));
	if (f->unselected) {
		assert(!pg_wire_write_u64(file, f->unselected->status));
		if (f->unselected->status == PG_SYNTHESIS_PENDING) {
			assert(!pg_synthesis_module_frontier(s, f->unselected, &module));
			assert(!pg_wire_write_u64(file, module.position));
		}
	}
	for (size_t i = 0; i < f->frontier.count; ++i) {
		assert(!pg_wire_write_u64(file, !!f->frontier.entries[i]));
		if (!f->frontier.entries[i]) continue;
		assert(!pg_wire_write_u64(file, f->frontier.entries[i]->status));
		struct pg_synthesis_job *body = NULL;
		pg_synthesis_definition_body(s, f->frontier.entries[i], &body);
		assert(!pg_wire_write_u64(file, body != NULL));
	}
	assert(!pg_artifact_schedule_write(file, s, f->count, f->jobs));
	assert(proofs == f->program->typing.proofs.count && terms == f->program->graph.terms.count);
	rewind(file);
	return file;
}

static void equal_files(FILE *a, FILE *b)
{
	rewind(a); rewind(b);
	int x, y;
	do { x = fgetc(a); y = fgetc(b); assert(x == y); } while (x != EOF);
	assert(!ferror(a) && !ferror(b));
}

static const struct pg_syntax *definitions(const struct pg_syntax *syntax)
{
	return syntax->kind == PG_SYNTAX_QUALIFIED ? syntax->left : syntax;
}

static struct fixture start_with_sharing(const char *text, uint64_t steps, int share)
{
	struct fixture f = {.program = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK)};
	assert(f.program);
	struct pg_parser parser;
	pg_parser_init(&parser, &f.program->graph, text, strlen(text));
	f.syntax = pg_parser_program(&parser);
	if (!f.syntax || parser.error) fprintf(stderr, "parse fixture: %s: %s\n", text, parser.error);
	assert(f.syntax && !parser.error);
	if (share) {
		assert(f.syntax->kind == PG_SYNTAX_DEFINITIONS && f.syntax->item_count == 2);
		struct pg_syntax *copy = pg_alloc(&f.program->graph, sizeof(*copy));
		struct pg_syntax_item *items = pg_alloc(&f.program->graph, 2 * sizeof(*items));
		assert(copy && items);
		*copy = *f.syntax;
		memcpy(items, copy->items, 2 * sizeof(*items));
		items[1].expression = items[0].expression;
		copy->items = items; f.syntax = copy;
	}
	f.program->root = pg_synthesis_request(&f.program->synthesis, f.program->scope, f.syntax);
	assert(f.program->root);
	advance(&f.program->synthesis, 1 + steps);
	f.registration = pg_synthesis_prepare_module(&f.program->synthesis, f.program->root);
	assert(f.registration && f.registration->status == PG_SYNTHESIS_PENDING);
	collect(&f);
	return f;
}

static struct fixture start(const char *text, uint64_t steps)
{
	return start_with_sharing(text, steps, 0);
}

static void invalid_frontiers(struct fixture *f, struct pg_definition_frontier *view)
{
	struct pg_synthesis *s = &f->program->synthesis;
	struct pg_definition_frontier before, after;
	assert(!pg_synthesis_definition_frontier(s, f->registration, &before));
	struct pg_synthesis_job *ready = s->ready, *tail = s->ready_tail;
	struct pg_definition_frontier bad = *view;
	bad.indexed = view->count + 1;
	assert(pg_synthesis_definition_resume(s, f->registration, &bad));
	bad = *view; bad.activated = view->count + 1;
	assert(pg_synthesis_definition_resume(s, f->registration, &bad));
	bad = *view; bad.count++;
	assert(pg_synthesis_definition_resume(s, f->registration, &bad));
	bad = *view; bad.complete = 2;
	assert(pg_synthesis_definition_resume(s, f->registration, &bad));
	assert(pg_synthesis_definition_resume(s, f->program->root, view));
	if (view->count) {
		bad = *view; bad.entries = NULL;
		assert(pg_synthesis_definition_resume(s, f->registration, &bad));
		struct pg_synthesis_job **wrong = calloc(view->count, sizeof(*wrong));
		assert(wrong);
		memcpy(wrong, view->entries, view->count * sizeof(*wrong));
		bad = *view; bad.entries = wrong;
		for (size_t i = 0; i < view->indexed; ++i) {
			if (!wrong[i]) continue;
			struct pg_synthesis_job *original = wrong[i];
			wrong[i] = f->program->root;
			assert(pg_synthesis_definition_resume(s, f->registration, &bad));
			wrong[i] = NULL;
			assert(pg_synthesis_definition_resume(s, f->registration, &bad));
			wrong[i] = original;
		}
		free(wrong);
	}
	struct pg_program *other = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(other && pg_synthesis_definition_resume(&other->synthesis, f->registration, view));
	pg_program_destroy(other);
	assert(!pg_synthesis_definition_frontier(s, f->registration, &after));
	assert(before.count == after.count && before.indexed == after.indexed && before.activated == after.activated);
	assert(before.entries == after.entries && ready == s->ready && tail == s->ready_tail && !s->steps);
}

static void validation_queue(struct fixture *f, size_t count, struct pg_synthesis_job *const *targets)
{
	FILE *file = tmpfile();
	assert(file && fwrite("APGSCH\1", 1, 8, file) == 8);
	assert(!pg_wire_write_u64(file, f->count) && !pg_wire_write_u64(file, count) && !pg_wire_write_u64(file, 0));
	for (size_t i = 0; i < count; ++i) {
		size_t j = 0;
		while (j < f->count && f->jobs[j] != targets[i]) ++j;
		assert(j < f->count && !pg_wire_write_u64(file, j));
	}
	rewind(file);
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_read(file, &f->program->graph, 100000);
	assert(schedule && !pg_artifact_schedule_attach(&f->program->synthesis, schedule, f->count, f->jobs));
	assert(!fclose(file));
}

static void validate_children(struct fixture *f, const uint64_t *statuses, const uint64_t *bodies)
{
	struct pg_synthesis_job **targets = calloc(f->count, sizeof(*targets));
	assert(targets);
	size_t count = 0;
	for (size_t i = 0; i < f->count; ++i) {
		int wanted = 0;
		for (size_t j = 0; j < f->frontier.count; ++j) {
			struct pg_synthesis_job *entry = f->frontier.entries[j], *body = NULL, *term, *type;
			const struct pg_source_scope *scope;
			if (entry == f->jobs[i] && statuses[j] != PG_SYNTHESIS_PENDING) wanted = 1;
			if (bodies[j] && !pg_synthesis_definition_body(&f->program->synthesis, entry, &body) && body == f->jobs[i]) wanted = 1;
			if (!pg_synthesis_source_expect_input(&f->program->synthesis, entry, &scope, &term, &type) && type == f->jobs[i]) wanted = 1;
		}
		if (wanted) targets[count++] = f->jobs[i];
	}
	validation_queue(f, count, targets);
	free(targets);
	/* Explicit strict verification, charged to the same total budget. The
	 * root stays suspended; no completed entry status is copied from the file. */
	solving = 1;
	uint64_t none = UINT64_MAX;
	pg_artifact_revalidate(f->program, f->program->root, 100000, 0, &none);
	assert(!none && !f->program->synthesis.steps);
	pg_artifact_revalidate(f->program, f->program->root, 100000, 100000, &f->validation);
	solving = 0;
	assert(!f->program->synthesis.ready && f->validation == f->program->synthesis.steps);
	for (size_t i = 0; i < f->frontier.count; ++i)
		assert(f->frontier.entries[i]->status == (enum pg_synthesis_status)statuses[i]);
}

static void module_resume(struct fixture *f, struct pg_synthesis_job *module, size_t position)
{
	int selected = module == f->program->root && f->syntax->kind == PG_SYNTAX_QUALIFIED;
	struct pg_module_frontier view = {.position = position, .previous = f->registration};
	if (position) view.previous = selected ? f->unselected : f->frontier.entries[position - 1];
	struct pg_module_frontier bad = view;
	bad.position = f->frontier.count + 2;
	assert(pg_synthesis_module_resume(&f->program->synthesis, module, &bad));
	assert(!pg_synthesis_module_resume(&f->program->synthesis, module, &view));
	if (position) ++module_cursors;
}

static struct fixture load(FILE *file)
{
	struct fixture f = {.program = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK)};
	assert(f.program && !f.program->synthesis.ready);
	const struct pg_syntax *const *roots;
	size_t count;
	rewind(file);
	assert(!pg_syntax_read(file, &f.program->graph, 100000, &count, &roots) && count == 1);
	assert(!pg_syntax_validate(count, roots));
	f.syntax = roots[0];
	const struct pg_syntax *block = definitions(f.syntax);
	struct pg_definition_frontier view = {.count = block->item_count};
	uint64_t indexed, activated, complete, position, canonical_status = PG_SYNTHESIS_PENDING, canonical_position = 0;
	assert(!pg_wire_read_u64(file, &indexed) && !pg_wire_read_u64(file, &activated));
	assert(!pg_wire_read_u64(file, &complete) && complete <= 1 && !pg_wire_read_u64(file, &position));
	int canonical = f.syntax->kind == PG_SYNTAX_QUALIFIED && position;
	if (canonical) {
		assert(!pg_wire_read_u64(file, &canonical_status));
		if (canonical_status == PG_SYNTHESIS_PENDING) assert(!pg_wire_read_u64(file, &canonical_position));
		else assert(canonical_status == PG_SYNTHESIS_DONE || canonical_status == PG_SYNTHESIS_REJECTED
			|| canonical_status == PG_SYNTHESIS_UNSUPPORTED);
	}
	view.indexed = indexed; view.activated = activated; view.complete = complete;
	unsigned char *present = calloc(view.count + 1, 1);
	uint64_t *statuses = calloc(view.count + 1, sizeof(*statuses)), *bodies = calloc(view.count + 1, sizeof(*bodies));
	struct pg_synthesis_job **entries = calloc(view.count + 1, sizeof(*entries));
	assert(present && entries && statuses && bodies);
	for (size_t i = 0; i < view.count; ++i) {
		uint64_t flag;
		assert(!pg_wire_read_u64(file, &flag) && flag <= 1);
		present[i] = flag;
		if (flag) assert(!pg_wire_read_u64(file, &statuses[i]) && !pg_wire_read_u64(file, &bodies[i]));
	}
	const struct pg_artifact_schedule *schedule = pg_artifact_schedule_read(file, &f.program->graph, 100000);
	assert(schedule && fgetc(file) == EOF);
	struct pg_synthesis *s = &f.program->synthesis;
	f.program->root = pg_synthesis_request(s, f.program->scope, f.syntax);
	f.registration = pg_synthesis_prepare_module(s, f.program->root);
	assert(f.registration);
	if (canonical) {
		f.unselected = pg_synthesis_request(s, f.program->scope, block);
		assert(f.unselected && pg_synthesis_prepare_module(s, f.unselected) == f.registration);
	}
	const struct pg_source_scope *inner = pg_synthesis_definition_scope(s, f.program->scope, block);
	for (size_t i = 0; i < view.count; ++i) {
		if (!present[i]) continue;
		const struct pg_syntax_item *item = &block->items[i];
		if (item->operation == PG_TOKEN_ASSIGN)
			entries[i] = pg_synthesis_definition_request(s, f.program->scope, block, item->expression);
		else if (item->operation == PG_SYNTAX_IMPORT)
			entries[i] = pg_synthesis_request(s, f.program->scope, item->expression);
	}
	for (size_t i = 0; i < view.count; ++i) {
		const struct pg_syntax_item *item = &block->items[i];
		if (!present[i] || item->operation != PG_TOKEN_EXPECT) continue;
		struct pg_synthesis_job *term = NULL;
		for (size_t j = 0; j < view.count; ++j) {
			const struct pg_syntax_item *other = &block->items[j];
			if (other->operation != PG_TOKEN_ASSIGN) continue;
			if (other->name.length == item->name.length && !memcmp(other->name.text, item->name.text, item->name.length))
				term = entries[j];
		}
		assert(term);
		entries[i] = pg_synthesis_source_expect(s, inner, term, pg_synthesis_request(s, inner, item->expression));
	}
	view.entries = entries;
	/* Retained input attachment is independent of the registration cursor. */
	for (size_t i = 0; i < view.count; ++i) if (entries[i])
		assert(!pg_synthesis_retain_definition_input(s, f.program->root, i, entries[i]));
	invalid_frontiers(&f, &view);
	assert(!pg_synthesis_definition_resume(s, f.registration, &view));
	if (view.indexed) assert(pg_synthesis_definition_resume(s, f.registration, &view));
	for (size_t i = 0; i < view.count; ++i) if (bodies[i]) {
		struct pg_synthesis_job *previous, *body = pg_synthesis_request(s, inner, block->items[i].expression);
		assert(!pg_synthesis_definition_body(s, entries[i], &previous));
		if (!previous) {
			assert(pg_synthesis_definition_resume_body(s, entries[i], f.program->root));
			assert(!pg_synthesis_definition_resume_body(s, entries[i], body));
			++body_edges;
		} else assert(previous == body);
	}
	collect(&f);
	assert(!s->steps && !pg_synthesis_result(f.program->root));
	if (complete) {
		validate_children(&f, statuses, bodies);
		if (canonical && canonical_status != PG_SYNTHESIS_PENDING) {
			pg_synthesis_enqueue(s, f.unselected);
			uint64_t used;
			solving = 1;
			pg_artifact_revalidate(f.program, f.unselected, 100000 - f.validation, 100000 - f.validation, &used);
			solving = 0;
			f.validation += used;
			assert(f.unselected->status == (enum pg_synthesis_status)canonical_status);
		} else if (canonical) module_resume(&f, f.unselected, canonical_position);
		module_resume(&f, f.program->root, position);
	}
	assert(!pg_artifact_schedule_attach(s, schedule, f.count, f.jobs));
	assert(!pg_synthesis_definition_frontier(s, f.registration, &f.frontier));
	assert(s->steps == f.validation && !pg_synthesis_result(f.program->root));
	assert((f.registration->status == PG_SYNTHESIS_DONE) == complete);
	free(present); free(entries); free(statuses); free(bodies);
	return f;
}

static FILE *complete(struct fixture *f, enum pg_synthesis_status *status, uint64_t *spent)
{
	uint64_t before = f->program->synthesis.steps;
	assert(f->validation <= 100000);
	advance(&f->program->synthesis, 100000 - f->validation);
	*spent = f->program->synthesis.steps - before;
	assert(f->validation + *spent <= 100000);
	*status = pg_synthesis_status(f->program->root);
	assert(!f->program->synthesis.ready);
	FILE *file = tmpfile();
	assert(file && !pg_sources_write(file, &f->program->synthesis, 1, &f->program->root));
	return file;
}

static void destroy(struct fixture *f)
{
	free(f->jobs);
	pg_program_destroy(f->program);
}

static void check_roundtrip(struct fixture original, enum pg_synthesis_status expected)
{
	FILE *saved = save(&original);
	enum pg_synthesis_status status;
	uint64_t remaining;
	FILE *result = complete(&original, &status, &remaining);
	if (status != expected) fprintf(stderr, "definition result: actual=%d expected=%d\n", status, expected);
	assert(status == expected);
	destroy(&original);
	struct fixture restored = load(saved);
	checked_dispatches += restored.validation;
	FILE *copy = save(&restored);
	equal_files(saved, copy);
	advance(&restored.program->synthesis, 0);
	FILE *zero = save(&restored);
	equal_files(copy, zero);
	uint64_t resumed;
	FILE *actual = complete(&restored, &status, &resumed);
	assert(status == expected && resumed == remaining);
	equal_files(result, actual);
	destroy(&restored);
	assert(!fclose(saved) && !fclose(copy) && !fclose(zero) && !fclose(result) && !fclose(actual));
}

static void partition_registration(void)
{
	char text[2048];
	size_t used = 0;
	for (size_t i = 0; i < 64; ++i)
		used += (size_t)snprintf(text + used, sizeof(text) - used, "x%zu:=@;", i);
	assert(used < sizeof(text));
	struct fixture full = start(text, 19);
	FILE *expected = save(&full);
	destroy(&full);
	const uint64_t parts[] = {1, 10, 20};
	for (size_t i = 0; i < sizeof(parts) / sizeof(*parts); ++i) {
		struct fixture first = start(text, parts[i] - 1);
		FILE *file = save(&first);
		destroy(&first);
		struct fixture second = load(file);
		advance(&second.program->synthesis, 20 - parts[i]);
		free(second.jobs); second.count = 0;
		collect(&second);
		FILE *actual = save(&second);
		equal_files(expected, actual);
		assert(second.program->synthesis.steps == 20 - parts[i]);
		destroy(&second);
		assert(!fclose(file) && !fclose(actual));
	}
	assert(!fclose(expected));
	puts("registration: 1+19, 10+10, 20+0 equal uninterrupted 20 byte-for-byte");
}

static void registration_links(void)
{
	struct fixture f = start("import x; import x;", 0);
	struct pg_synthesis *s = &f.program->synthesis;
	struct pg_synthesis_job *first = pg_synthesis_request(s, f.program->scope, f.syntax->items[0].expression);
	struct pg_synthesis_job *second = pg_synthesis_request(s, f.program->scope, f.syntax->items[1].expression);
	struct pg_synthesis_job *entries[] = {first, second};
	struct pg_definition_frontier view = {.count = 2, .indexed = 2, .entries = entries};
	assert(first && second && first != second);
	assert(pg_synthesis_definition_resume(s, f.registration, &view));
	struct pg_definition_frontier unchanged;
	assert(!pg_synthesis_definition_frontier(s, f.registration, &unchanged) && !unchanged.indexed);
	entries[1] = first;
	assert(!pg_synthesis_definition_resume(s, f.registration, &view));
	assert(!pg_synthesis_definition_frontier(s, f.registration, &unchanged) && unchanged.indexed == 2);
	assert(unchanged.entries[0] == first && unchanged.entries[1] == first);
	destroy(&f);

	f = start("x:=@; x:=@;", 0); s = &f.program->synthesis;
	for (size_t i = 0; i < 2; ++i)
		entries[i] = pg_synthesis_definition_request(s, f.program->scope, f.syntax, f.syntax->items[i].expression);
	assert(pg_synthesis_definition_resume(s, f.registration, &view));
	assert(!pg_synthesis_definition_frontier(s, f.registration, &unchanged) && !unchanged.indexed);
	destroy(&f);
	puts("registration links: repeated import sharing retained; skipped duplicate assignment rejected");
}

static void module_guards(void)
{
	struct fixture f = start("first:=missing; second:=@;", 0);
	struct pg_synthesis *s = &f.program->synthesis;
	struct pg_synthesis_job *entries[2];
	for (size_t i = 0; i < 2; ++i)
		entries[i] = pg_synthesis_definition_request(s, f.program->scope, f.syntax, f.syntax->items[i].expression);
	struct pg_definition_frontier names = {.count = 2, .indexed = 2, .activated = 1, .complete = 1, .entries = entries};
	assert(pg_synthesis_definition_resume(s, f.registration, &names));
	names.activated = 2;
	assert(!pg_synthesis_definition_resume(s, f.registration, &names));
	struct pg_module_frontier cursor = {.position = 2, .previous = entries[0]}, unchanged;
	assert(pg_synthesis_module_resume(s, f.program->root, &cursor));
	cursor.previous = entries[1];
	assert(pg_synthesis_module_resume(s, f.program->root, &cursor));
	assert(!pg_synthesis_module_frontier(s, f.program->root, &unchanged));
	assert(!unchanged.position && unchanged.previous == f.registration);

	const struct pg_source_scope *scope = pg_synthesis_definition_scope(s, f.program->scope, f.syntax);
	struct pg_synthesis_job *body = pg_synthesis_request(s, scope, f.syntax->items[0].expression), *saved;
	assert(body);
	struct fixture other = start("foreign:=@;", 0);
	assert(pg_synthesis_definition_resume_body(s, entries[0], other.program->root));
	assert(pg_synthesis_definition_resume_body(&other.program->synthesis, entries[0], body));
	destroy(&other);
	assert(!pg_synthesis_definition_body(s, entries[0], &saved) && !saved);
	assert(!pg_synthesis_definition_resume_body(s, entries[0], body));
	assert(pg_synthesis_definition_resume_body(s, entries[0], body));
	free(f.jobs); f.count = 0;
	collect(&f);
	validation_queue(&f, 1, &body);
	advance(s, 10000);
	assert(body->status == PG_SYNTHESIS_REJECTED && !s->ready);
	validation_queue(&f, 1, entries);
	advance(s, 10000);
	assert(entries[0]->status == PG_SYNTHESIS_REJECTED && !s->ready);
	assert(f.program->root->status == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_module_resume(s, f.program->root, &cursor));
	assert(!pg_synthesis_module_frontier(s, f.program->root, &unchanged));
	assert(!unchanged.position && unchanged.previous == f.registration);
	/* Restoring the current failing entry is legal; skipping it is not. */
	cursor.position = 1; cursor.previous = entries[0];
	assert(!pg_synthesis_module_resume(s, f.program->root, &cursor));
	validation_queue(&f, 1, &f.program->root);
	advance(s, 10000);
	assert(f.program->root->status == PG_SYNTHESIS_REJECTED);
	assert(entries[1]->status == PG_SYNTHESIS_PENDING);
	destroy(&f);
	puts("module guards: pending/rejected siblings cannot be skipped; foreign body owners reject");
}

static size_t lifecycle(const char *text, enum pg_synthesis_status expected)
{
	size_t checked = 0;
	for (size_t cut = 0; cut < 10000; ++cut) {
		struct fixture f = start(text, 0);
		advance(&f.program->synthesis, cut);
		if (f.program->root->status != PG_SYNTHESIS_PENDING) {
			if (!checked) fprintf(stderr, "no lifecycle boundary: %s at %zu\n", text, cut);
			assert(f.program->root->status == expected);
			destroy(&f);
			assert(checked);
			return checked;
		}
		struct pg_definition_frontier view;
		int ready = !pg_synthesis_definition_frontier(&f.program->synthesis, f.registration, &view) && view.complete;
		for (size_t i = 0; ready && i < view.count; ++i) {
			struct pg_synthesis_job *entry = view.entries[i], *body = NULL;
			if (!entry) { ready = 0; break; }
			if (entry->status != PG_SYNTHESIS_PENDING) continue;
			ready = !pg_synthesis_definition_body(&f.program->synthesis, entry, &body)
				&& body && body->status != PG_SYNTHESIS_PENDING;
		}
		if (!ready) { destroy(&f); continue; }
		free(f.jobs); f.count = 0;
		collect(&f);
		for (struct pg_synthesis_job *job = f.program->synthesis.ready; job; job = job->next) {
			size_t i = 0;
			while (i < f.count && f.jobs[i] != job) ++i;
			if (i == f.count) ready = 0;
		}
		if (!ready) { destroy(&f); continue; }
		check_roundtrip(f, expected);
		++checked;
	}
	assert(!"source lifecycle did not complete");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--pending-namespace")) {
		lifecycle("Nat:=@{zero:*;succ:*->*;}; main:=Nat.zero; main::Nat;", PG_SYNTHESIS_DONE);
		return 0;
	}
	assert(argc == 1);
	const char *sources[] = {
		"a:=b; b:=@; main:=a;",
		"{{main:=@; other:=@;}}.main",
		"{{main:=@; other:=missing;}}.main",
		"{{main:=@; main::@;}}.main",
		"a:=b; b:=a; main:=a;",
		"{{main:=@; main:=@;}}.main",
		"{{main::@; main:=@;}}.main",
		"Nat:=@{zero:*;succ:*->*;}; id:=\\x:Nat=>x; main:=id Nat.zero;",
		"main:=\\x:@=>x;",
		"import absent; main:=@;",
		"{{import absent; main:=@;}}.main"
	};
	enum pg_synthesis_status statuses[] = {PG_SYNTHESIS_DONE, PG_SYNTHESIS_DONE,
		PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_PENDING,
		PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_DONE,
		PG_SYNTHESIS_DONE, PG_SYNTHESIS_UNSUPPORTED, PG_SYNTHESIS_UNSUPPORTED};
	size_t cuts[] = {5, 4, 4, 4, 5, 2, 4, 5, 3, 2, 2};
	size_t cases = 0;
	for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i)
		for (size_t cut = 0; cut < cuts[i]; ++cut) { check_roundtrip(start(sources[i], cut), statuses[i]); ++cases; }
	for (size_t cut = 0; cut < 4; ++cut) {
		check_roundtrip(start_with_sharing("a:=@;b:=@;", cut, 1), PG_SYNTHESIS_DONE);
		++cases;
	}
	printf("definition checkpoint: %zu source frontiers preserve remaining dispatches, diagnostics, and bytes\n", cases);
	partition_registration();
	registration_links();
	module_guards();
	size_t late = lifecycle("a:=@; b:=@; main:=@;", PG_SYNTHESIS_DONE);
	late += lifecycle("{{a:=@; main:=a;}}.main", PG_SYNTHESIS_DONE);
	late += lifecycle("{{main:=@; bad:=missing;}}.main", PG_SYNTHESIS_REJECTED);
	late += lifecycle("{{main:=@; main::@;}}.main", PG_SYNTHESIS_REJECTED);
	late += lifecycle("Nat:=@{zero:*;succ:*->*;}; main:=Nat.succ Nat.zero; main::Nat;", PG_SYNTHESIS_DONE);
	assert(body_edges && module_cursors);
	printf("source lifecycle: %zu owner frontiers, %zu body edges, %zu module cursors, %llu charged verification dispatches\n",
		late, body_edges, module_cursors, (unsigned long long)checked_dispatches);
	return 0;
}
