/* Diagnostic-only access to the owners' actual state, not copied layouts. */
#include "conversion.c"
#include "synthesis_conversion.c"
#include "program.h"
#include "eval_internal.h"

#include <inttypes.h>
#include <stdio.h>

static void shape(const struct pg_term *term, unsigned depth)
{
	if (!term) { fputs("null", stdout); return; }
	if (!depth) { printf("...%p", (const void *)term); return; }
	switch (term->kind) {
	case PG_REFERENCE:
		printf("%s:%p", term->as.reference->kind == PG_BINDER ? "var" :
			term->as.reference->owner->name, (const void *)term->as.reference);
		break;
	case PG_LAMBDA:
		printf("lambda(%p,", (const void *)term->as.lambda.binder);
		shape(term->as.lambda.body, depth - 1); putchar(')'); break;
	case PG_APPLICATION:
		fputs("app(", stdout); shape(term->as.application.function, depth - 1);
		putchar(','); shape(term->as.application.argument, depth - 1); putchar(')'); break;
	}
}

static void snapshot(struct pg_program *p)
{
	printf("steps=%" PRIu64 " terms=%zu requests=%zu root=%d\n",
		p->synthesis.steps, p->graph.terms.count, p->synthesis.jobs.count,
		pg_synthesis_status(p->root));
	size_t ready = 0, conversions = 0;
	for (struct pg_synthesis_job *j = p->synthesis.ready; j; j = j->next) {
		++ready;
		if (j->role != CONVERSION_JOB) continue;
		++conversions;
		struct conversion_work *local = pg_synthesis_work_state(j, CONVERSION_JOB);
		struct pg_conversion_state *s = local->comparison.state;
		if (!s || s->steps < 100000) continue;
		printf("comparison=%p strong=%d steps=%" PRIu64 " tasks=%zu\n", (void *)j,
			s->strong, s->steps, pg_conversion_task_count(&local->comparison));
		fputs("left=", stdout); shape(s->left, 4); putchar('\n');
		fputs("right=", stdout); shape(s->right, 4); putchar('\n');
		fputs("normalizing=", stdout); shape(s->normalizing, 6); putchar('\n');
		const struct pg_term *head = pg_whnf_result(s->normalization.whnf);
		fputs("whnf=", stdout); shape(head, 7); putchar('\n');
		printf("neutral=%d rigid=%d\n", head ? neutral(head) : -1, head ? rigid_head(head) : -1);
		struct pg_nf_job *nf = s->normalization.nf;
		if (nf) {
			printf("nf=%p steps=%" PRIu64 " depth=%zu\n", (void *)nf, nf->steps, nf->depth);
			struct pg_nf_job *top = nf->depth ? nf->stack[nf->depth - 1] : nf;
			fputs("nf-top=", stdout); shape(top->request.input, 6); putchar('\n');
			if (top->head) {
				printf("head-steps=%" PRIu64 " ready=%d frames=%p\n", top->head->steps,
					top->head->machine.head_ready, (void *)top->head->machine.frames);
				fputs("head-input=", stdout); shape(top->head->request.input, 5); putchar('\n');
			}
		}
	}
	printf("ready=%zu comparisons=%zu\n", ready, conversions);
	fflush(stdout);
}

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	FILE *f = fopen(argv[1], "rb");
	if (!f) return 2;
	if (fseek(f, 0, SEEK_END)) { fclose(f); return 2; }
	long length = ftell(f);
	if (length < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return 2; }
	char *source = malloc((size_t)length + 1);
	if (!source) { fclose(f); return 2; }
	int failed = fread(source, 1, (size_t)length, f) != (size_t)length;
	if (fclose(f)) failed = 1;
	if (failed) { free(source); return 2; }
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	if (!p) { free(source); return 2; }
	p->allow_legacy_intrinsic_dot = 1;
	p->root = pg_program_source(p, p->scope, source, (size_t)length, &p->parser);
	free(source);
	if (!p->root) { pg_program_destroy(p); return 2; }
	for (unsigned i = 0; i < 12 && p->synthesis.ready; ++i) {
		pg_synthesis_advance(&p->synthesis, 1000000);
		snapshot(p);
	}
	pg_program_destroy(p);
	return 0;
}
