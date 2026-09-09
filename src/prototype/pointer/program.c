#include "program.h"

#include <stdlib.h>
#include <string.h>

struct pg_synthesis_job *pg_program_source(struct pg_program *program,
	const struct pg_source_scope *scope, const char *source, size_t length,
	struct pg_parser *diagnostic)
{
	if (!diagnostic) return NULL;
	memset(diagnostic, 0, sizeof(*diagnostic));
	if (!program || !scope || (!source && length)) return NULL;
	char *copy = pg_alloc(&program->graph, length ? length : 1);
	if (!copy) return NULL;
	if (length) memcpy(copy, source, length);
	pg_parser_init(diagnostic, &program->graph, copy, length);
	const struct pg_syntax *syntax = pg_parser_program(diagnostic);
	return syntax ? pg_synthesis_request(&program->synthesis, scope, syntax) : NULL;
}

struct pg_program *pg_program_allocate(enum pg_definition_policy policy)
{
	struct pg_program *program = calloc(1, sizeof(*program));
	if (!program) return NULL;
	if (pg_graph_init(&program->graph) != 0) goto fail;
	if (pg_typing_init(&program->typing, &program->graph) != 0) goto fail;
	if (pg_classifiers_init(&program->classifiers, &program->graph) != 0) goto fail;
	if (pg_whnf_work_init(&program->evaluation, &program->graph) != 0) goto fail;
	if (pg_effect_inference_init(&program->imported_effects, &program->graph) != 0) goto fail;
	if (pg_synthesis_init(&program->synthesis, &program->typing, &program->classifiers,
		&program->evaluation, policy) != 0) goto fail;
	program->scope = pg_synthesis_root(&program->synthesis);
	if (!program->scope) goto fail;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}

struct pg_program *pg_program_create(const char *source, size_t length,
	enum pg_definition_policy policy)
{
	if (!source && length) return NULL;
	struct pg_program *program = pg_program_allocate(policy);
	if (!program) return NULL;
	program->root = pg_program_source(program, program->scope, source, length, &program->parser);
	if (program->parser.error) return program;
	if (!program->root) goto fail;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}

struct pg_synthesis_job *pg_program_normalize(struct pg_program *program,
	const struct pg_evidence *proof, int full)
{
	if (!program || !pg_evidence_owned_by(proof, &program->typing)) return NULL;
	const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
	if (!context) return NULL;
	if (pg_evidence_context(proof) != pg_evidence_context(context)) return NULL;
	const struct pg_term *content;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
		pg_thunk_type_view(pg_evidence_classifier(proof), &content))
		proof = pg_prove_force(&program->typing, proof);
	return full ? pg_synthesis_nf(&program->synthesis, context, proof)
		: pg_synthesis_normalize(&program->synthesis, context, proof);
}

void pg_program_destroy(struct pg_program *program)
{
	if (!program) return;
	pg_synthesis_destroy(&program->synthesis);
	pg_effect_inference_destroy(&program->imported_effects);
	pg_whnf_work_destroy(&program->evaluation);
	pg_classifiers_destroy(&program->classifiers);
	pg_typing_destroy(&program->typing);
	pg_graph_destroy(&program->graph);
	free(program);
}
