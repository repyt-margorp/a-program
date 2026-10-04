#include "emit.h"
#include "classifier.h"
#include "computation.h"
#include "dag.h"
#include <string.h>

static int subject_child(void *unused, const void *key, size_t slot, const void **out)
{
	(void)unused; const struct pg_occurrence *s = key;
	if (slot < s->operand_count) { *out = s->operands[slot]; return *out ? 1 : 2; }
	slot -= s->operand_count;
	if (!slot) { *out = s->type; return *out ? 1 : 2; }
	if (slot == 1) { *out = s->origin; return *out ? 1 : 2; }
	slot -= 2; const struct pg_context_map *const *maps = pg_occurrence_maps(s);
	for (size_t i = 0; i <= s->map_count; ++i) {
		const struct pg_context_map *map = i ? maps[i-1] : s->map;
		if (!map) continue;
		if (slot < map->count) { *out = map->images[slot]; return *out ? 1 : 2; }
		slot -= map->count;
	}
	return 0;
}

static const struct pg_term *unary(const struct pg_term *term, const struct pg_object *operation)
{
	if (!term || term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *f = term->as.application.function;
	return f->kind == PG_REFERENCE && f->as.reference == operation ? term->as.application.argument : NULL;
}

/* Bind only known static source factory syntax. Unknown computations are
	* never evaluated to make a target head available. */
static int selected_lambda(struct pg_graph *storage, const struct pg_term *term, struct pg_c_indexed_operand *out)
{
	struct pg_c_indexed_operand operand = {term,NULL}, stack[16]; size_t count = 0;
	for (size_t step = 0; step < 1024 && operand.term; ++step) {
		operand = pg_c_indexed_bound(operand); const struct pg_term *t = operand.term, *inner, *body;
		if ((inner = unary(t,&pg_total_result_operation)) || (inner = unary(t,&pg_return_operation))) { operand.term=inner; continue; }
		if ((inner = unary(t,&pg_force_operation)) && (body = unary(inner,&pg_thunk_operation))) { operand.term=body; continue; }
		if (t->kind == PG_APPLICATION) {
			if (count == 16) return -1;
			stack[count++] = (struct pg_c_indexed_operand){t->as.application.argument,operand.environment}; operand.term=t->as.application.function; continue;
		}
		if (t->kind != PG_LAMBDA) return -1;
		if (!count) { *out=operand; return 0; }
		struct pg_c_indexed_binding *binding = pg_alloc(storage,sizeof(*binding)); if (!binding) return -1;
		*binding=(struct pg_c_indexed_binding){operand.environment,t->as.lambda.binder,stack[--count]};
		operand=(struct pg_c_indexed_operand){t->as.lambda.body,binding};
	}
	return -1;
}

static int family_arguments(struct pg_graph *storage, struct pg_c_indexed_operand type,
	const struct pg_data_declaration *declaration, size_t wanted, struct pg_c_indexed_operand *arguments)
{
	struct pg_c_indexed_operand head; size_t count;
	return !pg_c_indexed_head(storage,type,&head,&count,arguments)
		&& head.term->as.reference == pg_data_declaration_family(declaration) && count == wanted;
}

static int family(struct pg_graph *storage, struct pg_c_indexed_operand type,
	const struct pg_data_declaration *declaration, size_t wanted)
{
	struct pg_c_indexed_operand arguments[16];
	return family_arguments(storage,type,declaration,wanted,arguments);
}

static int reference(struct pg_c_indexed_operand operand, const struct pg_object *binder)
{
	operand=pg_c_indexed_bound(operand);
	return operand.term && operand.term->kind == PG_REFERENCE && operand.term->as.reference == binder;
}

static int indexed(struct pg_graph *storage, struct pg_c_indexed_operand type, size_t family_index,
	const struct pg_c_indexed_entry *entries, const struct pg_object *index, const struct pg_object *right)
{
	struct pg_c_indexed_operand a[16]; size_t wanted=family_index==3 ? 3 : 2;
	if (!family_arguments(storage,type,entries[family_index].view.declaration,wanted,a)) return 0;
	if (family_index==2) return reference(a[0],index) && reference(a[1],right);
	if (!family(storage,a[0],entries[1].view.declaration,0)) return 0;
	if (family_index==3 && !family(storage,a[1],entries[2].view.declaration,0)) return 0;
	return reference(a[wanted-1],index);
}

static int list_result(struct pg_graph *storage, struct pg_c_indexed_operand type, const struct pg_c_indexed_entry *entries)
{
	struct pg_c_indexed_operand a[16];
	return family_arguments(storage,type,entries[7].view.declaration,1,a) && family(storage,a[0],entries[1].view.declaration,0);
}

static int pure(const struct pg_term *type, const struct pg_term **result)
{
	enum pg_totality totality;
	return pg_pure_computation_type_view(type,&totality,result) && totality == PG_TOTALITY_TOTAL;
}

int pg_c_acc_fold_emit(FILE *source, FILE *header, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, size_t count, const struct pg_c_indexed_entry *entries)
{
	if (!source || !header || !storage || !typing || !root || !entries || count != 8) return -1;
	const char *names[] = {"bool","nat","lt","acc","sized","measured","partition","list"};
	for (size_t i = 0; i < 8; ++i) if (!entries[i].name || strcmp(entries[i].name,names[i]) || !entries[i].view.declaration) return -1;
	struct pg_c_indexed_operand selected;
	if (selected_lambda(storage,root->core,&selected)) return -1;
	const struct pg_term *lambda = selected.term;
	const struct pg_object *size_binder = lambda->as.lambda.binder;
	lambda = lambda->as.lambda.body; if (lambda->kind != PG_LAMBDA) return -1;
	const struct pg_object *access_binder = lambda->as.lambda.binder;
	const struct pg_term *type = root->classifier, *domain, *codomain; const struct pg_object *binder;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !family(storage,(struct pg_c_indexed_operand){domain,NULL},entries[1].view.declaration,0)) return -1;
	const struct pg_object *classifier_size=binder;
	type=codomain;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !indexed(storage,(struct pg_c_indexed_operand){domain,NULL},3,entries,classifier_size,NULL)) return -1;
	type=codomain;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !indexed(storage,(struct pg_c_indexed_operand){domain,NULL},4,entries,classifier_size,NULL)) return -1;
	if (!pure(codomain,&type) || !list_result(storage,(struct pg_c_indexed_operand){type,NULL},entries)) return -1;
	/* The selected classifier and lexical A argument are read independently.
		* The result type is never used to invent a source producer or coercion. */
	struct pg_dag dag; if (pg_dag_init(&dag,subject_child,NULL)) return -1;
	int status = -1; const struct pg_occurrence *fold = NULL; struct pg_elimination_inputs chosen = {0};
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *n=dag.first; n; n=n->next) {
		const struct pg_occurrence *s=n->key; if (!s->induction || s->induction->count != 1) continue;
		struct pg_elimination_inputs view; const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&view)) break;
		if (!proof || !family(storage,(struct pg_c_indexed_operand){pg_evidence_classifier(view.scrutinee),NULL},entries[3].view.declaration,3)) continue;
		if (fold) goto done;
		fold=s; chosen=view;
	}
	if (!fold || chosen.count != 1) goto done;
	if (!reference((struct pg_c_indexed_operand){pg_evidence_subject(chosen.scrutinee)->core,NULL},access_binder)
		|| !indexed(storage,(struct pg_c_indexed_operand){pg_evidence_classifier(chosen.scrutinee),NULL},3,entries,size_binder,NULL)) goto done;
	const struct pg_context *base=fold->context; size_t captures, suffix;
	if (pg_context_extension_size(base,NULL,&captures) || captures != 4
		|| base->binder != access_binder || !base->parent || base->parent->binder != size_binder
		|| pg_context_extension_size(fold->induction->clauses[0],base,&suffix) || suffix != 3) goto done;
	const struct pg_context *comparison=base->parent->parent, *element=comparison ? comparison->parent : NULL;
	uint64_t level;
	if (!element || element->parent || !pg_universe_level(element->declared_type,&level) || level) goto done;
	struct pg_term element_ref = {.kind=PG_REFERENCE,.as.reference=element->binder};
	struct pg_c_indexed_operand actual_element=pg_c_indexed_bound((struct pg_c_indexed_operand){&element_ref,selected.environment});
	if (!family(storage,actual_element,entries[1].view.declaration,0)) goto done;
	const struct pg_term *compare_type;
	if (!pg_thunk_type_view(comparison->declared_type,&compare_type)) goto done;
	for (size_t i=0; i<2; ++i) {
		if (!pg_pi_view(compare_type,&domain,&binder,&codomain)
			|| !family(storage,(struct pg_c_indexed_operand){domain,selected.environment},entries[1].view.declaration,0)) goto done;
		compare_type=codomain;
	}
	if (!pure(compare_type,&type) || !family(storage,(struct pg_c_indexed_operand){type,selected.environment},entries[0].view.declaration,0)) goto done;
	if (!pg_pi_view(fold->classifier,&domain,&binder,&codomain)
		|| !indexed(storage,(struct pg_c_indexed_operand){domain,selected.environment},4,entries,size_binder,NULL)
		|| !pure(codomain,&type) || !list_result(storage,(struct pg_c_indexed_operand){type,selected.environment},entries)) goto done;
	const struct pg_context *ih=fold->induction->clauses[0], *down=ih->parent, *current=down ? down->parent : NULL;
	if (!current || current->parent != base || !family(storage,(struct pg_c_indexed_operand){current->declared_type,NULL},entries[1].view.declaration,0)) goto done;
	if (!pg_thunk_type_view(down->declared_type,&type)) goto done;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !family(storage,(struct pg_c_indexed_operand){domain,NULL},entries[1].view.declaration,0)) goto done;
	const struct pg_object *down_y=binder; type=codomain;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !indexed(storage,(struct pg_c_indexed_operand){domain,NULL},2,entries,down_y,current->binder)) goto done;
	if (!pure(codomain,&type) || !indexed(storage,(struct pg_c_indexed_operand){type,NULL},3,entries,down_y,NULL)) goto done;
	if (!pg_thunk_type_view(ih->declared_type,&type)) goto done;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !family(storage,(struct pg_c_indexed_operand){domain,NULL},entries[1].view.declaration,0)) goto done;
	const struct pg_object *ih_y=binder; type=codomain;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !indexed(storage,(struct pg_c_indexed_operand){domain,NULL},2,entries,ih_y,current->binder)) goto done;
	type=codomain;
	if (!pg_pi_view(type,&domain,&binder,&codomain) || !indexed(storage,(struct pg_c_indexed_operand){domain,selected.environment},4,entries,ih_y,NULL)) goto done;
	if (!pure(codomain,&type) || !list_result(storage,(struct pg_c_indexed_operand){type,selected.environment},entries)) goto done;
	/* The source allocation has fields current, original down, then callable IH.
		* Generate only the traversal/sequence adapter, never a sorter stencil. */
	fputs("#ifndef __ACTUAL_ACC_FOLD_H__\n#define __ACTUAL_ACC_FOLD_H__\n#include \"indexed.h\"\n#include <stddef.h>\n\n"
		"/* Generated Acc Fold/IH plumbing from admitted quickSortAcc.\n\t* Clause bodies are supplied manually; no full generated sorting claim. */\n"
		"struct af_program;\nstruct af_result {\n\tint (*call)(void *, const struct iv_sized *, const struct iv_list **);\n\tvoid *context;\n\tconst struct iv_nat *index;\n};\n"
		"struct af_comparison {\n\tint (*call)(void *, const struct iv_nat *, const struct iv_nat *, const struct iv_bool **);\n\tvoid *context;\n};\n"
		"struct af_capture {\n\tconst void *element_type;\n\tstruct af_comparison comparison;\n};\n"
		"struct af_ih {\n\tstruct af_program *program;\n\tstruct iv_acc_c0_f1 original;\n\tconst struct iv_nat *current;\n};\n"
		"struct af_program {\n\tstruct af_capture capture;\n\tint (*clause)(struct af_program *, const struct iv_nat *, struct iv_acc_c0_f1, struct af_ih, struct af_result *);\n\tsize_t depth, limit;\n};\n"
		"/* Borrowed source-valid data/code/context; callers retain index consistency.\n\t* 1 null,2 malformed local tag/field,4 nested clause construction depth.\n\t* Provider failures propagate. Failure leaves result unchanged.\n\t* The depth guard does not bound a supplied result callback's execution. */\n"
		"int af_fold(struct af_program *, const struct iv_acc *, struct af_result *);\n"
		"int af_force_down(const struct af_ih *, const struct iv_nat *, const struct iv_lt *, struct af_result *);\n"
		"int af_apply(const struct af_result *, const struct iv_sized *, const struct iv_list **);\n#endif\n",header);
	fputs("#include \"fold.h\"\n\nint af_fold(struct af_program *program, const struct iv_acc *access, struct af_result *result)\n{\n"
		"\tif (!program || !access || !result || !program->clause) return 1;\n"
		"\tif (access->tag || !access->index0 || !access->fields.c0.field0 || !access->fields.c0.field1.call) return 2;\n"
		"\tif (program->depth >= program->limit) return 4;\n"
		"\tstruct af_ih ih = {program,access->fields.c0.field1,access->fields.c0.field0};\n"
		"\tstruct af_result next = {0};\n\t++program->depth;\n"
		"\tint status = program->clause(program,access->fields.c0.field0,access->fields.c0.field1,ih,&next);\n"
		"\t--program->depth;\n\tif (status) return status;\n\tif (!next.call || !next.index) return 2;\n\t*result = next;\n\treturn 0;\n}\n\n"
		"int af_force_down(const struct af_ih *ih, const struct iv_nat *y, const struct iv_lt *edge, struct af_result *result)\n{\n"
		"\tif (!ih || !ih->program || !ih->original.call || !y || !edge || !result) return 1;\n"
		"\tconst struct iv_acc *child = 0;\n\tint status = ih->original.call(ih->original.context,y,edge,&child);\n"
		"\tif (status) return status;\n\tif (!child) return 2;\n"
		"\t/* Original down returns Acc; its recursive Fold produces the callable. */\n"
		"\treturn af_fold(ih->program,child,result);\n}\n\n"
		"int af_apply(const struct af_result *result, const struct iv_sized *input, const struct iv_list **output)\n{\n"
		"\tif (!result || !input || !output || !result->call) return 1;\n"
		"\tconst struct iv_list *next = 0;\n\tint status = result->call(result->context,input,&next);\n"
		"\tif (status) return status;\n\tif (!next) return 2;\n\t*output = next;\n\treturn 0;\n}\n",source);
	status=ferror(source) || ferror(header) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}
