#include "emit.h"
#include "classifier.h"

/* Reuse exact admitted map, endpoint and binder readers. */
#include "../acc_frame/emit.c"

enum recipe_role { RECIPE_ARGUMENT, RECIPE_CAPTURE };
struct recipe_operand { enum recipe_role role; size_t slot; };
struct recipe_down {
	size_t captured_field,down_field;
	struct recipe_operand domain[2],result;
};
struct recipe_action { int quoted; size_t source,destination; };

static int recipe_reference(struct pg_c_indexed_operand operand, const struct pg_object *binder)
{
	operand=pg_c_indexed_bound(operand);
	return operand.term && operand.term->kind==PG_REFERENCE && operand.term->as.reference==binder;
}

static int recipe_family(struct pg_graph *storage, struct pg_c_indexed_operand operand,
	const struct pg_data_declaration *declaration)
{
	struct pg_c_indexed_operand head,args[16]; size_t count;
	return !pg_c_indexed_head(storage,operand,&head,&count,args) && !count
		&& pg_data_declaration_view(head.term->as.reference)==declaration;
}

static int recipe_index(struct pg_c_indexed_operand operand,
	const struct pg_object *argument, const struct pg_object *captured, struct recipe_operand *out)
{
	if (recipe_reference(operand,argument)) { *out=(struct recipe_operand){RECIPE_ARGUMENT,0}; return 0; }
	if (recipe_reference(operand,captured)) { *out=(struct recipe_operand){RECIPE_CAPTURE,0}; return 0; }
	return -1;
}

static int recipe_down_read(struct pg_graph *storage, const struct pg_c_indexed_entry *entries,
	struct recipe_down *out)
{
	const struct pg_c_indexed_view *acc=&entries[3].view;
	const struct pg_context *prefix=pg_data_declaration_parameters(acc->declaration);
	const struct pg_context *fields=pg_data_declaration_fields(acc->declaration,0);
	const struct pg_data_layout *layout=pg_data_declaration_layout(acc->declaration);
	size_t count;
	if (!prefix || acc->count!=2 || pg_data_layout_count(layout)!=1
		|| pg_context_extension_size(prefix->parent,NULL,&count) || count!=2
		|| pg_context_extension_size(fields,prefix,&count) || count!=2
		|| pg_context_extension_size(pg_data_declaration_indices(acc->declaration),prefix,&count) || count!=1) return -1;
	const struct pg_context *parameters[2]={prefix->parent->parent,prefix->parent};
	struct pg_c_indexed_binding bindings[2];
	const struct pg_c_indexed_binding *environment=acc->environment;
	for (size_t i=0; i<2; ++i) {
		bindings[i]=(struct pg_c_indexed_binding){environment,parameters[i]->binder,acc->arguments[i]};
		environment=bindings+i;
	}
	if (!recipe_family(storage,acc->arguments[0],entries[1].view.declaration)
		|| !recipe_family(storage,acc->arguments[1],entries[2].view.declaration)) return -1;
	const struct pg_context *captured=fields->parent,*down=fields;
	if (!recipe_family(storage,(struct pg_c_indexed_operand){captured->declared_type,environment},entries[1].view.declaration)) return -1;
	const struct pg_term *type,*domain,*codomain; const struct pg_object *argument,*edge;
	struct pg_c_indexed_operand operand=pg_c_indexed_bound((struct pg_c_indexed_operand){down->declared_type,environment});
	if (!pg_thunk_type_view(operand.term,&type) || !pg_pi_view(type,&domain,&argument,&codomain)
		|| !recipe_family(storage,(struct pg_c_indexed_operand){domain,environment},entries[1].view.declaration)
		|| !pg_pi_view(codomain,&domain,&edge,&type)) return -1;
	(void)edge;
	struct pg_c_indexed_operand head,args[16]; size_t arity;
	if (pg_c_indexed_head(storage,(struct pg_c_indexed_operand){domain,environment},&head,&arity,args)
		|| arity!=2 || pg_data_declaration_view(head.term->as.reference)!=entries[2].view.declaration) return -1;
	struct recipe_down result={.captured_field=0,.down_field=1};
	for (size_t i=0; i<arity; ++i)
		if (recipe_index(args[i],argument,captured->binder,result.domain+i)) return -1;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(type,&totality,&type) || totality!=PG_TOTALITY_TOTAL
		|| pg_c_indexed_head(storage,(struct pg_c_indexed_operand){type,environment},&head,&arity,args)
		|| head.term->as.reference!=prefix->binder || arity!=1
		|| recipe_index(args[0],argument,captured->binder,&result.result)
		|| result.result.role!=RECIPE_ARGUMENT) return -1;
	/* The closed target LT/down ABI represents this exact source argument order.
		* Other relations/orders remain refusals, not a new erasure convention. */
	if (result.domain[0].role!=RECIPE_ARGUMENT || result.domain[1].role!=RECIPE_CAPTURE) return -1;
	*out=result; return 0;
}

static int recipe_actions_read(struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries, struct recipe_action *actions)
{
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; unsigned seen=0;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration);
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *node=dag.first; node; node=node->next) {
		const struct pg_occurrence *field=node->key; const struct pg_term *family,*value;
		enum pg_identity_direction direction; int lift;
		if (!pg_identity_field_view(field->core,&family,&value,&direction,&lift)) continue;
		const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(typing,field,proof))) if (pg_evidence_rule(proof)==PG_IDENTITY_TRANSPORT) break;
		if (!proof || field->operand_count!=2 || field->operands[0]->core!=family || field->operands[1]->core!=value) continue;
		if (!pg_evidence_owned_by(proof,typing) || lift) goto done;
		struct pg_identity_boundary boundary; const struct pg_occurrence *formation=field->operands[0]->type;
		int found=0;
		for (size_t wrapper=0; formation && wrapper<256; ++wrapper,formation=formation->origin)
			if (pg_identity_boundary_view(formation,&boundary)) { found=1; break; }
		if (!found) goto done;
		struct boundary_descriptor map;
		if (descriptor_read(&boundary,field,direction,nat,&map)) goto done;
		size_t branch=map.direction ? 0 : map.right[map.count-2].successor ? 2 : 1;
		if ((seen&(1u<<branch)) || map.count!=(branch ? 10u : 6u) || map.depth!=(branch ? 12u : 10u)) goto done;
		struct pg_c_acc_endpoint left,right;
		if (pg_c_acc_endpoint_read(storage,boundary.left->core,nat,&left)
			|| pg_c_acc_endpoint_read(storage,boundary.right->core,nat,&right)
			|| left.declaration!=right.declaration || left.quoted!=right.quoted) goto done;
		int quoted=left.quoted && left.declaration==entries[3].view.declaration;
		if (!quoted && (left.quoted || left.declaration!=entries[2].view.declaration)) goto done;
		if (quoted!=map.direction) goto done;
		actions[branch]=(struct recipe_action){quoted,map.direction ? 0 : 1,map.direction ? 1 : 0};
		seen|=1u<<branch;
	}
	if (seen==7) status=0;
done:
	pg_dag_destroy(&dag); return status;
}

static void recipe_operand_write(FILE *out, const struct recipe_operand *operand)
{
	fprintf(out,"{%s,%zu}",operand->role==RECIPE_ARGUMENT ? "GA_ARGUMENT" : "GA_CAPTURE",operand->slot);
}

static int recipe_to(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	struct recipe_down down; struct recipe_action actions[3];
	if (recipe_down_read(storage,entries,&down) || recipe_actions_read(storage,typing,root,entries,actions)) return -1;
	fputs("/* Actual admitted Acc constructor/down classifier: Pi domain is\n"
		"\t* contravariant, pure TOTAL recursive result keeps its argument index.\n"
		"\t* These are bounded target recipes, not source action/Scope evidence. */\n",out);
	fprintf(out,"static const struct ga_down ga_down_recipe = {%zu,%zu,{",down.captured_field,down.down_field);
	for (size_t i=0; i<2; ++i) { if (i) fputc(',',out); recipe_operand_write(out,down.domain+i); }
	fputs("},",out); recipe_operand_write(out,&down.result); fputs("};\nstatic const struct ga_boundary ga_boundaries[] = {\n",out);
	for (size_t i=0; i<3; ++i)
		fprintf(out,"\t{&gt_boundaries[%zu],%s,%zu,%zu,%s},\n",i,actions[i].quoted ? "GA_QUOTED_ACC" : "GA_LT",
			actions[i].source,actions[i].destination,actions[i].quoted ? "&ga_down_recipe" : "NULL");
	fputs("};\n",out); return ferror(out) ? -1 : 0;
}

int pg_c_acc_recipe_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference, const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=descriptors_to(temporary,typing,root,reference,entries);
	if (!status) status=endpoints_to(temporary,typing,root,entries);
	if (!status) status=frame_to(temporary,typing,root,entries);
	if (!status) status=recipe_to(temporary,storage,typing,root,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
