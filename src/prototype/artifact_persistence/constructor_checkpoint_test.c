#include "program.h"
#include "synthesis_source.h"
#include "iadt.h"
#include "declaration_io.h"
#include "derivation_io.h"
#include "artifact/file.h"
#include "wire.h"
#include <assert.h>
#include <string.h>

/* Known-origin checkpoints: keep completed field construction as a typed map,
 * not as completed scope-worker history. No private field/body state is copied. */
static const char magic[8] = "APGCCT\1";
static int solving;
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

struct checkpoint {
	struct pg_program *program;
	struct pg_declaration_io objects;
	const struct pg_derivation_input *const *inputs;
	size_t ordinal;
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	struct checkpoint *c = owner;
	return pg_wire_write_u64(file, c->ordinal) || pg_derivation_inputs_write_inference(file,
		3, c->inputs, &c->program->imported_effects, codec, &c->objects);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner)
{
	(void)graph;
	struct checkpoint *c = owner;
	uint64_t ordinal;
	size_t count;
	if (pg_wire_read_u64(file, &ordinal) || ordinal > SIZE_MAX) return -1;
	c->ordinal = (size_t)ordinal;
	return pg_derivations_read_inference(file, &c->program->typing, limit, name_limit,
		&c->program->imported_effects, codec, &c->objects, &count, &c->inputs) || count != 3;
}

static FILE *save(struct checkpoint *c)
{
	FILE *file = tmpfile();
	size_t terms = c->program->graph.terms.count, proofs = c->program->typing.proofs.count;
	assert(file && !pg_graph_image_write(file, magic, &pg_declaration_graph_codec, &c->objects, write_payload, c));
	assert(terms == c->program->graph.terms.count && proofs == c->program->typing.proofs.count);
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

static FILE *result_file(struct checkpoint *c, struct pg_synthesis_job *job)
{
	FILE *file = tmpfile();
	const struct pg_evidence *proof = pg_synthesis_result(job);
	assert(proof);
	const struct pg_term *term = pg_evidence_subject(proof)->core;
	assert(file && !pg_graph_write_descriptors(file, 1, &term, &pg_declaration_graph_codec, &c->objects));
	return file;
}

static void field_order(struct pg_program *p, const struct pg_evidence *formation,
	const struct pg_object *constructor)
{
	const struct pg_data_schema *schema = pg_evidence_inductive_schema(formation);
	const struct pg_context *prefix = pg_evidence_context(pg_data_schema_parameters(schema));
	const struct pg_evidence *fields = pg_data_schema_fields(schema, constructor);
	size_t count;
	assert(!pg_context_extension_size(pg_evidence_context(fields), prefix, &count));
	const struct pg_data_layout *foreign = pg_data_layout(&p->graph, 1, &count);
	size_t proofs = p->typing.proofs.count, terms = p->graph.terms.count;
	assert(!pg_data_schema_field(NULL, constructor, 0));
	assert(!pg_data_schema_field(schema, NULL, 0));
	assert(!pg_data_schema_field(schema, pg_data_constructor(foreign, 0), 0));
	assert(!pg_data_schema_field(schema, constructor, count));
	assert(!pg_data_schema_field(schema, constructor, SIZE_MAX));
	for (size_t i = count; i; --i) {
		const struct pg_evidence *field = pg_data_schema_field(schema, constructor, i - 1);
		assert(field && pg_evidence_context(field) == pg_evidence_context(fields));
		for (size_t repeat = 0; repeat < 20; ++repeat)
			assert(pg_data_schema_field(schema, constructor, i - 1) == field);
		fields = pg_context_parent_input(&p->typing, fields);
	}
	assert(pg_evidence_context(fields) == prefix);
	assert(p->typing.proofs.count == proofs && p->graph.terms.count == terms);
}

static void reject_constant_field(struct pg_program *p, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters, const struct pg_evidence *scope)
{
	const struct pg_context_map *map = pg_evidence_context_map(scope);
	if (map->count == pg_evidence_context_map(parameters)->count + 1) return;
	struct pg_typing *typing = &p->typing;
	const struct pg_evidence *context = pg_evidence_premise(scope, 1);
	const struct pg_evidence *projected = pg_prove_substitution_projection(typing,
		pg_evidence_premise(parameters, 1), context);
	const struct pg_object *zero = pg_data_constructor(pg_data_schema_layout(pg_evidence_inductive_schema(formation)), 0);
	const struct pg_evidence *value = pg_prove_constructor(typing, formation, zero, projected, 0, NULL);
	assert(value);
	struct pg_graph scratch = {0};
	const struct pg_evidence *const *original = pg_substitution_images(typing, scope, &scratch);
	const struct pg_evidence **images = pg_alloc(&scratch, map->count * sizeof(*images));
	assert(original && images);
	memcpy(images, original, map->count * sizeof(*images));
	images[map->count - 1] = value;
	const struct pg_evidence *constant = pg_prove_substitution(typing,
		pg_evidence_premise(scope, 0), context, map->count, images);
	assert(constant && !pg_synthesis_constructor_from_scope(&p->synthesis, formation, constructor, parameters, constant));
	pg_graph_destroy(&scratch);
}

static void resume(FILE *file, FILE *expected, uint64_t remaining)
{
	struct checkpoint c = {.program = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK)};
	assert(c.program && !pg_declaration_io_init(&c.objects, &c.program->typing));
	size_t baseline_proofs = c.program->typing.proofs.count;
	rewind(file);
	assert(!pg_graph_image_read(file, magic, &c.program->graph, 100000, 100000,
		&pg_declaration_graph_codec, &c.objects, read_payload, &c));
	assert(!c.program->synthesis.steps && !c.program->synthesis.ready);
	assert(c.program->typing.proofs.count == baseline_proofs);
	FILE *copy = save(&c);
	equal_files(file, copy);
	assert(!fclose(copy));
	struct pg_synthesis *s = &c.program->synthesis;
	struct pg_synthesis_job *premises[3];
	for (size_t i = 0; i < 3; ++i) premises[i] = pg_synthesis_derivation(s, c.inputs[i]);
	uint64_t validation = 0;
	for (size_t i = 0; i < 3; ++i) {
		uint64_t used;
		solving = 1;
		pg_artifact_revalidate(c.program, premises[i], 100000 - validation, 100000 - validation, &used);
		solving = 0;
		validation += used;
		assert(pg_synthesis_result(premises[i]));
	}
	assert(!s->ready && s->steps == validation);
	const struct pg_evidence *formation = premises[0]->result, *parameters = premises[1]->result, *scope = premises[2]->result;
	const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
	assert(c.ordinal < pg_data_layout_count(layout));
	const struct pg_object *constructor = pg_data_constructor(layout, c.ordinal);
	field_order(c.program, formation, constructor);
	size_t proofs = c.program->typing.proofs.count, terms = c.program->graph.terms.count;
	struct pg_program *foreign = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK);
	assert(foreign && !pg_synthesis_constructor_from_scope(&foreign->synthesis, formation, constructor, parameters, scope));
	pg_program_destroy(foreign);
	assert(!pg_synthesis_constructor_from_scope(s, formation, constructor, parameters, parameters));
	assert(!pg_synthesis_constructor_from_scope(s, formation, constructor, scope, scope));
	if (c.ordinal) assert(!pg_synthesis_constructor_from_scope(s, formation, pg_data_constructor(layout, 0), parameters, scope));
	assert(c.program->typing.proofs.count == proofs && c.program->graph.terms.count == terms);
	reject_constant_field(c.program, formation, constructor, parameters, scope);
	proofs = c.program->typing.proofs.count; terms = c.program->graph.terms.count;
	struct pg_synthesis_job *job = pg_synthesis_constructor_from_scope(s, formation, constructor, parameters, scope);
	assert(job && job->status == PG_SYNTHESIS_PENDING && !pg_synthesis_result(job));
	assert(pg_synthesis_constructor_from_scope(s, formation, constructor, parameters, scope) == job);
	const struct pg_context_map *fields = pg_evidence_context_map(scope), *prefix = pg_evidence_context_map(parameters);
	size_t jobs = s->jobs.count;
	assert(pg_synthesis_constructor_value_at(s, formation, constructor, parameters,
		prefix->destination, fields->destination) == job);
	if (fields->destination != prefix->destination)
		assert(!pg_synthesis_constructor_value_at(s, formation, constructor, parameters,
			prefix->destination, prefix->destination));
	assert(s->jobs.count == jobs);
	assert(s->ready == job && s->ready_tail == job && !job->next);
	assert(s->steps == validation && c.program->typing.proofs.count == proofs && c.program->graph.terms.count == terms);
	const struct pg_evidence *borrowed;
	assert(pg_synthesis_constructor_ready_scope(s, job, &borrowed) == 1 && borrowed == scope);
	advance(s, 0);
	assert(s->steps == validation && job->status == PG_SYNTHESIS_PENDING);
	advance(s, 1);
	assert(!pg_synthesis_constructor_ready_scope(s, job, &borrowed));
	assert(!pg_synthesis_constructor_from_scope(s, formation, constructor, parameters, scope));
	advance(s, 100000 - validation - 1);
	assert(job->status == PG_SYNTHESIS_DONE && !s->ready && s->steps - validation == remaining);
	assert(!pg_synthesis_constructor_ready_scope(s, job, &borrowed));
	assert(!pg_synthesis_constructor_from_scope(s, formation, constructor, parameters, scope));
	FILE *result = result_file(&c, job);
	equal_files(expected, result);
	assert(!fclose(result));
	printf("constructor %zu: checked scope reuse, %llu validation + %llu remaining dispatches, exact Core\n",
		c.ordinal, (unsigned long long)validation, (unsigned long long)remaining);
	pg_declaration_io_destroy(&c.objects);
	pg_program_destroy(c.program);
}

static void constructor(const char *text, size_t ordinal)
{
	struct checkpoint c = {.program = pg_program_allocate_empty(PG_DEFINITION_IMPLICIT_THUNK), .ordinal = ordinal};
	struct pg_program *p = c.program;
	assert(p && !pg_declaration_io_init(&c.objects, &p->typing));
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &p->graph, text, strlen(text));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *family = pg_synthesis_request(&p->synthesis, p->scope, definition.expression);
	advance(&p->synthesis, 100000);
	assert(pg_synthesis_result(family) && !p->synthesis.ready);
	const struct pg_syntax *declaration = definition.expression;
	while (declaration->kind == PG_SYNTAX_LAMBDA) declaration = declaration->right;
	const struct pg_evidence *formation = NULL;
	for (size_t i = 0; i < p->synthesis.jobs.capacity; ++i) {
		for (struct pg_index_entry *entry = p->synthesis.jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *candidate = (void *)entry;
			const struct pg_source_scope *scope;
			const struct pg_syntax *syntax;
			if (!pg_synthesis_source_input(&p->synthesis, candidate, &scope, &syntax) && syntax == declaration)
				formation = pg_synthesis_result(candidate);
		}
	}
	assert(formation && pg_evidence_rule(formation) == PG_INDUCTIVE_FORM);
	const struct pg_evidence *prefix = pg_context_parent_input(&p->typing, pg_evidence_premise(formation, 0));
	const struct pg_evidence *parameters = pg_prove_substitution_projection(&p->typing, prefix, prefix);
	const struct pg_object *label = pg_data_constructor(pg_data_schema_layout(pg_evidence_inductive_schema(formation)), ordinal);
	field_order(p, formation, label);
	/* Another ordinary synthesis owner isolates just the field/value work from
	 * the source module's already completed exported members. */
	struct pg_synthesis isolated;
	assert(!pg_synthesis_init(&isolated, &p->typing, &p->evaluation, PG_DEFINITION_IMPLICIT_THUNK));
	struct pg_synthesis_job *scope_job = pg_synthesis_constructor_scope(&isolated,
		pg_synthesis_evidence(&isolated, formation), label, pg_synthesis_evidence(&isolated, parameters));
	advance(&isolated, 100000);
	assert(scope_job && scope_job->status == PG_SYNTHESIS_DONE && !isolated.ready);
	struct pg_synthesis_job *job = pg_synthesis_constructor_value(&isolated, formation, label, parameters);
	const struct pg_evidence *scope;
	assert(job && pg_synthesis_constructor_ready_scope(&isolated, job, &scope) == 1);
	assert(pg_synthesis_constructor_ready_scope(&p->synthesis, job, &scope) == -1);
	struct pg_synthesis_job *inputs[] = {pg_synthesis_evidence(&isolated, formation),
		pg_synthesis_evidence(&isolated, parameters), pg_synthesis_evidence(&isolated, scope)};
	assert(!pg_synthesis_export_rules(&isolated, 3, inputs, &p->graph,
		&p->imported_effects, 1, &c.inputs));
	FILE *saved = save(&c);
	uint64_t before = isolated.steps;
	advance(&isolated, 100000);
	uint64_t remaining = isolated.steps - before;
	assert(job->status == PG_SYNTHESIS_DONE && !isolated.ready);
	FILE *expected = result_file(&c, job);
	pg_synthesis_destroy(&isolated);
	pg_declaration_io_destroy(&c.objects);
	pg_program_destroy(p);
	resume(saved, expected, remaining);
	assert(!fclose(saved) && !fclose(expected));
}

int main(void)
{
	const char *sources[] = {
		"Nat:=@{zero:*;succ:*->*;pair:*->*->*;};",
		"Box:=\\A:@=>@{zero:*;succ:*->*;pair:A->*->*;};"
	};
	for (size_t source = 0; source < sizeof(sources) / sizeof(*sources); ++source)
		for (size_t i = 0; i < 3; ++i) constructor(sources[source], i);
	return 0;
}
