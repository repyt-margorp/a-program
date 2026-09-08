#include "source_io.h"
#include "syntax_io.h"
#include "dag.h"
#include "wire.h"
#include <string.h>

static const char magic[8] = "APGSRC\1";
enum environment_kind { ROOT, NAME, MODULE, NAMESPACE, IMPORTS };

struct environment {
	enum environment_kind kind;
	struct pg_token name;
	const struct pg_source_scope *parent, *target;
	const struct pg_syntax *syntax;
};

static int environment(const struct pg_synthesis *synthesis, const struct pg_source_scope *scope,
	struct environment *output)
{
	struct pg_source_environment input;
	if (pg_synthesis_environment_input(synthesis, scope, &input)) return -1;
	*output = (struct environment){.parent = input.parent, .name = input.name};
	struct pg_synthesis_job *job = input.producer ? input.producer : input.module;
	if (job) {
		output->kind = input.producer ? NAME : MODULE;
		return pg_synthesis_source_input(synthesis, job, &output->target, &output->syntax);
	}
	if (input.exports) { output->kind = NAMESPACE; output->target = input.exports; }
	else if (input.imports) { output->kind = IMPORTS; output->target = input.imports; }
	else if (input.parent) return -1;
	return 0;
}

static int child(void *owner, const void *key, size_t index, const void **result)
{
	if (index == 2) return 0;
	struct environment input;
	const struct pg_synthesis *const *synthesis = owner;
	if (environment(*synthesis, key, &input)) return -1;
	*result = index ? input.target : input.parent;
	return *result ? 1 : 2;
}

static uint64_t id(const struct pg_dag *dag, const void *key)
{
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? node->id : 0;
}

int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots)
{
	if (!file || !synthesis || (count && !roots)) return -1;
	struct pg_dag scopes = {0}, syntax = {0};
	int status = -1;
	/* Callbacks only inspect synthesis; no solver entry is invoked. */
	if (pg_dag_init(&scopes, child, &synthesis) || pg_dag_init(&syntax, NULL, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term;
		if (pg_synthesis_source_input(synthesis, roots[i], &scope, &term)
			|| pg_dag_add(&scopes, scope) || pg_dag_add(&syntax, term)) goto done;
	}
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		if (input.syntax && pg_dag_add(&syntax, input.syntax)) goto done;
	}
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, synthesis->definition_policy)
		|| pg_wire_write_u64(file, scopes.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		uint64_t words[] = {input.kind, id(&scopes, input.parent), id(&scopes, input.target),
			id(&syntax, input.syntax), input.name.kind, input.name.length};
		for (size_t i = 0; i < 6; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
		if (input.name.length && fwrite(input.name.text, 1, input.name.length, file) != input.name.length) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term;
		if (pg_synthesis_source_input(synthesis, roots[i], &scope, &term)) goto done;
		if (pg_wire_write_u64(file, id(&scopes, scope)) || pg_wire_write_u64(file, id(&syntax, term))) goto done;
	}
	if (syntax.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_syntax **terms = pg_alloc(&syntax.storage, syntax.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = syntax.first; node; node = node->next) terms[node->id - 1] = node->key;
	status = pg_syntax_write(file, syntax.count, terms);
done:
	pg_dag_destroy(&syntax); pg_dag_destroy(&scopes);
	return status;
}

struct record {
	uint64_t kind, parent, target, syntax;
	struct pg_token name;
};

struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots)
{
	if (!file || !count || !roots) return NULL;
	char header[8];
	uint64_t policy, n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &policy) || policy > PG_DEFINITION_EXPLICIT_THUNK) return NULL;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return NULL;
	if (n > limit || nr > (limit - n) / 2 || limit > SIZE_MAX / sizeof(struct record)) return NULL;
	struct pg_program *program = pg_program_allocate((enum pg_definition_policy)policy);
	if (!program) return NULL;
	struct pg_graph *graph = &program->graph;
	struct record *records = pg_alloc(graph, (size_t)n * sizeof(*records));
	const struct pg_source_scope **scopes = pg_alloc(graph, (size_t)n * sizeof(*scopes));
	uint64_t *ids = pg_alloc(graph, (size_t)nr * 2 * sizeof(*ids));
	struct pg_synthesis_job **jobs = pg_alloc(graph, (size_t)nr * sizeof(*jobs));
	if (!records || !scopes || !ids || !jobs) goto fail;
	size_t remaining = limit - (size_t)n - 2 * (size_t)nr;
	for (size_t i = 0; i < n; ++i) {
		uint64_t w[6];
		for (size_t j = 0; j < 6; ++j) if (pg_wire_read_u64(file, &w[j])) goto fail;
		if (w[0] > IMPORTS || w[1] > i || w[2] > i || w[4] > PG_TOKEN_ERROR || w[5] > remaining) goto fail;
		char *name = pg_alloc(graph, (size_t)w[5]);
		if (!name || fread(name, 1, (size_t)w[5], file) != w[5]) goto fail;
		remaining -= (size_t)w[5];
		records[i] = (struct record){w[0], w[1], w[2], w[3],
			{.kind = w[4], .text = name, .length = w[5], .text_length = w[5]}};
	}
	for (size_t i = 0; i < 2 * nr; ++i) if (pg_wire_read_u64(file, &ids[i])) goto fail;
	size_t nt;
	const struct pg_syntax *const *terms;
	if (pg_syntax_read(file, graph, limit, &nt, &terms) || pg_syntax_validate(nt, terms)) goto fail;
	if (fgetc(file) != EOF || ferror(file)) goto fail;
	for (size_t i = 0; i < n; ++i) {
		const struct record *r = &records[i];
		if (r->syntax > nt) goto fail;
		const struct pg_source_scope *parent = r->parent ? scopes[r->parent - 1] : NULL;
		const struct pg_source_scope *target = r->target ? scopes[r->target - 1] : NULL;
		struct pg_synthesis *s = &program->synthesis;
		if (r->kind == ROOT) {
			if (parent || target || r->syntax || r->name.kind || r->name.length) goto fail;
			scopes[i] = program->scope;
			continue;
		}
		if (!parent || !target) goto fail;
		if (r->kind == NAME || r->kind == MODULE) {
			if (!r->syntax) goto fail;
			struct pg_synthesis_job *job = pg_synthesis_request(s, target, terms[r->syntax - 1]);
			scopes[i] = r->kind == NAME ? pg_synthesis_name_job(s, parent, r->name, job)
				: pg_synthesis_module_namespace(s, parent, r->name, job);
		} else {
			if (r->syntax) goto fail;
			if (r->kind == IMPORTS) {
				if (r->name.kind || r->name.length) goto fail;
				scopes[i] = pg_synthesis_import_scope(s, parent, target);
			} else scopes[i] = pg_synthesis_namespace(s, parent, r->name, target);
		}
		if (!scopes[i]) goto fail;
	}
	for (size_t i = 0; i < nr; ++i) {
		if (!ids[2 * i] || ids[2 * i] > n || !ids[2 * i + 1] || ids[2 * i + 1] > nt) goto fail;
		jobs[i] = pg_synthesis_request(&program->synthesis, scopes[ids[2 * i] - 1], terms[ids[2 * i + 1] - 1]);
		if (!jobs[i]) goto fail;
	}
	program->root = nr ? jobs[0] : NULL;
	*count = (size_t)nr; *roots = jobs;
	return program;
fail:
	pg_program_destroy(program);
	return NULL;
}
