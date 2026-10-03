#include "program.h"
#include "source_io.h"
#include "synthesis_source.h"
#include "artifact/file.h"

#include <assert.h>
#include <stdio.h>

/* A valid typed payload is not evidence for the source owner that claims it.
 * Reconnect the source owner; ordinary Solve must still establish its result. */
int main(void)
{
	const char source[] = "main := @;";
	struct pg_program *p = pg_program_create(source, sizeof(source) - 1, PG_DEFINITION_IMPLICIT_THUNK);
	assert(p && p->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	p->root = pg_program_select_name(p, p->root, name);
	assert(p->root);
	const struct pg_evidence *other = pg_prove_universe(&p->typing, pg_prove_empty_context(&p->typing), 42);
	assert(other && !pg_synthesis_import_materialized(&p->synthesis, p->root, pg_evidence_subject(other)));
	assert(!pg_synthesis_import_completion(&p->synthesis, p->root));
	assert(!p->synthesis.steps && !pg_synthesis_result(p->root));
	FILE *file = tmpfile();
	struct pg_synthesis_input input = {.pending = pg_synthesis_pending(p->root)};
	assert(file && !pg_sources_write(file, &p->synthesis, 1, &input));
	pg_program_destroy(p);
	rewind(file);
	size_t count = 0;
	struct pg_synthesis_job *const *roots = NULL;
	p = pg_artifact_read_file(file, PG_ARTIFACT_DEFAULT_LIMIT, &count, &roots);
	assert(p && count == 1 && p->root == roots[0]);
	assert(!p->synthesis.steps && pg_synthesis_status(p->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(p->root));
	const struct pg_occurrence *stored = NULL;
	assert(pg_synthesis_materialized(p->root, &stored) == 1);
	assert(stored->core == pg_universe(&p->graph, 42));
	assert(pg_synthesis_saved_complete(&p->synthesis, p->root));
	FILE *copy = tmpfile();
	input.pending = pg_synthesis_pending(p->root);
	size_t proofs = p->typing.proofs.count;
	assert(copy && !pg_sources_write(copy, &p->synthesis, 1, &input));
	assert(!p->synthesis.steps && p->typing.proofs.count == proofs);
	rewind(file); rewind(copy);
	int a, b;
	do { a = fgetc(file); b = fgetc(copy); assert(a == b); } while (a != EOF);
	assert(!ferror(file) && !ferror(copy) && !fclose(copy));
	pg_synthesis_advance(&p->synthesis, 100000);
	assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *result = pg_synthesis_result(p->root);
	assert(result && pg_evidence_owned_by(result, &p->typing));
	assert(pg_evidence_subject(result)->core == pg_universe(&p->graph, 0));
	assert(pg_evidence_classifier(result) == pg_universe(&p->graph, 1));
	assert(pg_evidence_subject(result)->core != stored->core);
	assert(!fclose(file));
	pg_program_destroy(p);
	puts("source claim: inert valid payload cannot replace ordinary source-owner result");
	return 0;
}
