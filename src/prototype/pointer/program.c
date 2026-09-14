#include "program.h"
#include "host.h"
#include "derivation.h"

#include <stdlib.h>
#include <string.h>

struct export_scope {
	struct pg_index_entry index;
	const struct pg_source_scope *parent;
	const struct pg_source_scope *ambient;
	const struct pg_syntax *syntax;
	const struct pg_source_scope *result;
};

static const struct pg_source_scope *host_functions(struct pg_program *p,
	const struct pg_source_scope *scope, struct pg_synthesis_job *context)
{
	const char *widths[] = {"Int32", "Int64"};
	for (size_t width = 0; width < 2; ++width) {
		const struct pg_object *host = pg_host_type(widths[width]);
		struct pg_synthesis_job *contexts[3] = {context}, *types[3], *signatures[2];
		for (size_t depth = 0; depth < 3; ++depth) {
			struct pg_derivation_input type = {.rule = PG_HOST_TYPE_FORM, .count = 1, .parameters.constant = host};
			types[depth] = pg_synthesis_rule(&p->synthesis, &type, &contexts[depth], NULL, NULL);
			if (depth == 2) break;
			struct pg_derivation_input extend = {.rule = PG_CONTEXT_EXTEND, .count = 2,
				.parameters.binder = pg_binder(&p->graph)};
			struct pg_synthesis_job *premises[] = {contexts[depth], types[depth]};
			contexts[depth + 1] = pg_synthesis_rule(&p->synthesis, &extend, premises, NULL, NULL);
		}
		for (size_t arity = 1; arity <= 2; ++arity) {
			struct pg_derivation_input returned = {.rule = PG_RETURN_TYPE_FORM, .count = 1,
				.parameters.totality = PG_TOTALITY_TOTAL, .parameters.effects = pg_effect_row(&p->graph, 0, NULL)};
			struct pg_synthesis_job *signature = pg_synthesis_rule(&p->synthesis, &returned, &types[arity], NULL, NULL);
			for (size_t depth = arity; depth; --depth) {
				struct pg_derivation_input pi = {.rule = PG_PI_FORM, .count = 2};
				struct pg_synthesis_job *premises[] = {contexts[depth], signature};
				signature = pg_synthesis_rule(&p->synthesis, &pi, premises, NULL, NULL);
			}
			signatures[arity - 1] = signature;
		}
		const struct pg_object *function;
		for (size_t i = 0; (function = pg_host_function(i)); ++i) {
			const struct pg_object *type;
			size_t arity;
			if (!pg_host_function_view(function, &type, &arity)) return NULL;
			if (type != host) continue;
			struct pg_derivation_input intro = {.rule = PG_HOST_FUNCTION_INTRO, .count = 1,
				.parameters.constant = function};
			struct pg_synthesis_job *value = pg_synthesis_rule(&p->synthesis, &intro, &signatures[arity - 1], NULL, NULL);
			const char *name = pg_host_function_name(function);
			scope = pg_synthesis_name_job(&p->synthesis, scope,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name), .text_length = strlen(name)}, value);
			if (!scope) return NULL;
		}
	}
	return scope;
}

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
	diagnostic->allow_legacy_intrinsic_dot = program->allow_legacy_intrinsic_dot;
	const struct pg_syntax *syntax = pg_parser_program(diagnostic);
	return syntax ? pg_synthesis_request(&program->synthesis, scope, syntax) : NULL;
}

struct pg_program *pg_program_allocate(enum pg_definition_policy policy)
{
	struct pg_program *program = calloc(1, sizeof(*program));
	if (!program) return NULL;
	if (pg_graph_init(&program->graph) != 0) goto fail;
	if (pg_index_init(&program->exports) != 0) goto fail;
	if (pg_typing_init(&program->typing, &program->graph) != 0) goto fail;
	if (pg_classifiers_init(&program->classifiers, &program->graph) != 0) goto fail;
	if (pg_whnf_work_init(&program->evaluation, &program->graph) != 0) goto fail;
	if (pg_effect_inference_init(&program->imported_effects, &program->graph) != 0) goto fail;
	if (pg_synthesis_init(&program->synthesis, &program->typing, &program->classifiers,
		&program->evaluation, policy) != 0) goto fail;
	program->scope = pg_synthesis_root(&program->synthesis);
	if (!program->scope) goto fail;
	const struct pg_source_scope *hosts = program->scope;
	const char *names[] = {"Int", "Int32", "Int64", "Text"};
	const struct pg_evidence *empty = pg_prove_empty_context(&program->typing);
	struct pg_synthesis_job *context = pg_synthesis_evidence(&program->synthesis, empty);
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
		struct pg_derivation_input input = {.rule = PG_HOST_TYPE_FORM, .count = 1,
			.parameters.constant = pg_host_type(names[i])};
		struct pg_synthesis_job *type = pg_synthesis_rule(&program->synthesis, &input, &context, NULL, NULL);
		struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = names[i],
			.length = strlen(names[i]), .text_length = strlen(names[i])};
		hosts = pg_synthesis_name_job(&program->synthesis, hosts, name, type);
		if (!hosts) goto fail;
	}
	hosts = host_functions(program, hosts, context);
	if (!hosts) goto fail;
	program->scope = pg_synthesis_namespace(&program->synthesis, program->scope,
		(struct pg_token){.kind = '#'}, hosts);
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

const struct pg_source_scope *pg_program_exports(struct pg_program *program,
	const struct pg_source_scope *parent, struct pg_synthesis_job *module)
{
	if (!program || !parent) return NULL;
	struct pg_source_environment environment;
	if (pg_synthesis_environment_input(&program->synthesis, parent, &environment)) return NULL;
	const struct pg_source_scope *ambient;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(&program->synthesis, module, &ambient, &syntax)) return NULL;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	uint64_t hash = (uint64_t)(uintptr_t)parent;
	hash = hash * UINT64_C(1099511628211) ^ (uint64_t)(uintptr_t)ambient;
	hash = hash * UINT64_C(1099511628211) ^ (uint64_t)(uintptr_t)syntax;
	for (struct pg_index_entry *p = pg_index_candidates(&program->exports, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		struct export_scope *entry = (struct export_scope *)p;
		if (entry->parent == parent && entry->ambient == ambient && entry->syntax == syntax)
			return entry->result;
	}
	struct export_scope *entry = pg_alloc(&program->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->parent = parent;
	entry->ambient = ambient;
	entry->syntax = syntax;
	/* Namespace lookup preserves source polarity and whole-module checking. */
	struct pg_token module_name = {.kind = PG_TOKEN_IDENT, .text = "module", .length = 6, .text_length = 6};
	const struct pg_source_scope *scope = pg_synthesis_module_namespace(&program->synthesis,
		pg_synthesis_root(&program->synthesis), module_name, module);
	struct pg_syntax *module_reference = pg_alloc(&program->graph, sizeof(*module_reference));
	if (!scope || !module_reference) return NULL;
	*module_reference = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = module_name};
	for (size_t i = 0; i < syntax->item_count; ++i) {
		const struct pg_syntax_item *item = &syntax->items[i];
		if (item->operation != PG_TOKEN_ASSIGN) continue;
		struct pg_syntax *member = pg_alloc(&program->graph, sizeof(*member));
		struct pg_syntax *selection = pg_alloc(&program->graph, sizeof(*selection));
		if (!member || !selection) return NULL;
		*member = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = item->name};
		*selection = (struct pg_syntax){.kind = PG_SYNTAX_QUALIFIED, .left = module_reference, .right = member};
		struct pg_synthesis_job *selected = pg_synthesis_request(&program->synthesis, scope, selection);
		if (!selected) return NULL;
		parent = pg_synthesis_name_job(&program->synthesis, parent, item->name, selected);
		if (!parent) return NULL;
	}
	entry->result = parent;
	return pg_index_insert(&program->exports, &entry->index, hash) ? NULL : parent;
}

struct pg_synthesis_job *pg_program_normalize(struct pg_program *program,
	const struct pg_evidence *proof, int full)
{
	if (!program || !pg_evidence_owned_by(proof, &program->typing)) return NULL;
	if (!pg_evidence_subject(proof)) return NULL;
	const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
	if (!context) return NULL;
	if (pg_evidence_context(proof) != pg_evidence_context(context)) return NULL;
	return pg_synthesis_evaluate_jobs(&program->synthesis,
		pg_synthesis_evidence(&program->synthesis, context), pg_synthesis_evidence(&program->synthesis, proof),
		full ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
}

struct pg_synthesis_job *pg_program_evaluate_name(struct pg_program *program,
	struct pg_synthesis_job *module, struct pg_token name, int full)
{
	if (!program) return NULL;
	const struct pg_source_scope *exports = pg_program_exports(program,
		pg_synthesis_root(&program->synthesis), module);
	struct pg_synthesis_job *subject = pg_synthesis_named_input(&program->synthesis, exports, name);
	if (!subject) return NULL;
	return pg_synthesis_evaluate_jobs(&program->synthesis,
		pg_synthesis_evidence(&program->synthesis, pg_prove_empty_context(&program->typing)),
		subject, full ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
}

void pg_program_destroy(struct pg_program *program)
{
	if (!program) return;
	pg_index_destroy(&program->exports);
	pg_synthesis_destroy(&program->synthesis);
	pg_effect_inference_destroy(&program->imported_effects);
	pg_whnf_work_destroy(&program->evaluation);
	pg_classifiers_destroy(&program->classifiers);
	pg_typing_destroy(&program->typing);
	pg_graph_destroy(&program->graph);
	free(program);
}
