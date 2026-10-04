/* Reuse the sealed readonly owner/field inspection helpers. The new output
	* describes actual endpoint types; it does not check or execute Identity. */
#define main c42_inspection_main
#include "../acc_down/inspect.c"
#undef main
#include "read.h"
#include "classifier.h"

static void must_refuse(struct pg_graph *storage, const struct pg_term *term,
	const struct pg_data_layout *nat)
{
	struct pg_c_acc_endpoint endpoint; unsigned char before[sizeof(endpoint)];
	memset(&endpoint,0x5a,sizeof(endpoint)); memcpy(before,&endpoint,sizeof(endpoint));
	assert(pg_c_acc_endpoint_read(storage,term,nat,&endpoint)==-1);
	assert(!memcmp(before,&endpoint,sizeof(endpoint)));
}

static size_t endpoint_refusals(struct pg_graph *storage, const struct pg_term *term,
	const struct pg_data_layout *nat, const struct pg_c_acc_endpoint *endpoint)
{
	struct pg_c_indexed_operand head,args[16],constructor,fields[16]; size_t count,field_count;
	assert(!pg_c_indexed_head(storage,(struct pg_c_indexed_operand){term,NULL},&head,&count,args));
	assert(pg_data_layout_view(head.term->as.reference)==nat && count==3);
	assert(!pg_c_indexed_head(storage,args[0],&constructor,&field_count,fields) && field_count==1);
	/* Raw private negative shapes are not admitted source programs. In
		* particular an unknown predecessor does not authorize the Succ clause. */
	const struct pg_term *scrutinees[]={pg_c_indexed_bound(fields[0]).term,
		pg_reference(storage,pg_data_constructor(nat,0))};
	for (size_t variant=0; variant<2; ++variant) {
		const struct pg_term *changed=head.term;
		for (size_t i=0; i<count; ++i) {
			assert(!args[i].environment);
			changed=pg_application(storage,changed,i ? args[i].term : scrutinees[variant]); assert(changed);
		}
		must_refuse(storage,changed,nat);
	}
	if (!endpoint->quoted) return 2;
	const struct pg_term *bare=pg_reference(storage,pg_data_declaration_family(endpoint->declaration)); assert(bare);
	for (size_t i=0; i<endpoint->count; ++i) {
		struct pg_c_indexed_operand operand=pg_c_indexed_bound(endpoint->arguments[i]);
		assert(operand.term->kind==PG_REFERENCE);
		for (const struct pg_c_indexed_binding *binding=operand.environment; binding; binding=binding->parent)
			assert(binding->binder!=operand.term->as.reference);
		bare=pg_application(storage,bare,operand.term); assert(bare);
	}
	const struct pg_effect_row *empty=pg_effect_row(storage,0,NULL); assert(empty);
	const struct pg_term *unspecified=pg_thunk_type(storage,pg_computation_type(storage,PG_TOTALITY_UNSPECIFIED,empty,bare));
	assert(unspecified); must_refuse(storage,unspecified,nat);
	/* An opaque private row label tests structural refusal only, not a source
		* operation signature or a newly admitted effectful contract. */
	const struct pg_object *label=pg_data_declaration_family(endpoint->declaration);
	const struct pg_effect_row *nonempty=pg_effect_row(storage,1,&label); assert(nonempty);
	const struct pg_term *effectful=pg_thunk_type(storage,pg_computation_type(storage,PG_TOTALITY_TOTAL,nonempty,bare));
	assert(effectful); must_refuse(storage,effectful,nat); return 4;
}

static size_t context_depth(const struct pg_context *context)
{
	size_t count=0;
	for (; context; context=context->parent) ++count;
	return count;
}

static void describe(FILE *out, const struct pg_term *term,
	const struct pg_context *context, unsigned depth)
{
	if (!term || depth==12) { fputs("...",out); return; }
	if (term->kind==PG_REFERENCE) {
		const struct pg_object *object=term->as.reference;
		if (object->kind==PG_BINDER) {
			size_t slot=context_depth(context);
			for (const struct pg_context *c=context; c; c=c->parent) {
				--slot;
				if (c->binder==object) { fprintf(out,"b%zu",slot); return; }
			}
		}
		fprintf(out,"%s@%p",object->owner ? object->owner->name : "local",(const void *)object);
		return;
	}
	if (term->kind==PG_LAMBDA) {
		fputs("lambda(",out); describe(out,term->as.lambda.body,context,depth+1); fputc(')',out); return;
	}
	fputc('(',out); describe(out,term->as.application.function,context,depth+1);
	fputc(' ',out); describe(out,term->as.application.argument,context,depth+1); fputc(')',out);
}

static void map_images(const char *side, const struct pg_context_map *map)
{
	printf("%s count%zu source-depth%zu destination-depth%zu\n",side,map->count,
		context_depth(map->source),context_depth(map->destination));
	for (size_t i=0; i<map->count; ++i) {
		printf("%s image%zu judgement%d context-is-destination%d ",side,i,map->images[i]->judgement,
			map->images[i]->context==map->destination);
		describe(stdout,map->images[i]->core,map->destination,0); putchar('\n');
	}
}

int main(int argc, char **argv)
{
	assert(argc==3); FILE *file=fopen(argv[1],"rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *program=pg_artifact_read_file(file,SIZE_MAX,&count,&roots);
	assert(!fclose(file) && program && count);
	const struct pg_occurrence *root=select_subject(program,roots[0],argv[2]);
	const struct pg_occurrence *nat_root=select_subject(program,roots[0],"nat_type");
	struct pg_graph storage; assert(!pg_graph_init(&storage));
	struct pg_c_indexed_view nat_view; assert(!pg_c_indexed_view_read(&storage,nat_root->core,&nat_view));
	const struct pg_data_layout *nat=pg_data_declaration_layout(nat_view.declaration);
	size_t terms=program->graph.terms.count,objects=program->graph.objects.count;
	size_t proofs=program->typing.proofs.count,occurrences=program->typing.occurrences.count;
	size_t maps=program->typing.context_maps.count,contexts=program->typing.contexts.count;
	struct pg_dag dag; assert(!pg_dag_init(&dag,child,NULL) && !pg_dag_add(&dag,root));
	inspecting=1; size_t fields=0,refusals=0;
	for (const struct pg_dag_node *node=dag.first; node; node=node->next) {
		const struct pg_occurrence *s=node->key; const struct pg_term *family,*value;
		enum pg_identity_direction direction; int lift;
		if (!pg_identity_field_view(s->core,&family,&value,&direction,&lift)) continue;
		const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(&program->typing,s,proof)))
			if (pg_evidence_rule(proof)==PG_IDENTITY_TRANSPORT) break;
		if (!proof || s->operand_count!=2 || s->operands[0]->core!=family || s->operands[1]->core!=value) continue;
		assert(pg_evidence_owned_by(proof,&program->typing));
		struct pg_identity_boundary boundary; size_t wrappers;
		assert(direct_boundary(s->operands[0]->type,&boundary,&wrappers));
		assert(boundary.left_substitution && boundary.right_substitution);
		++fields;
		printf("FIELD%zu %s lift%d wrappers%zu paths%zu scope-source-shared%d scope-destination-shared%d field-depth%zu\n",
			fields,direction==PG_IDENTITY_RIGHT ? "right" : "left",lift,wrappers,boundary.path_count,
			boundary.left_substitution->source==boundary.right_substitution->source,
			boundary.left_substitution->destination==boundary.right_substitution->destination,context_depth(s->context));
		printf("FAMILY "); describe(stdout,boundary.family->core,boundary.family->context,0); putchar('\n');
		printf("VALUE "); describe(stdout,value,s->context,0); putchar('\n');
		map_images("LEFT",boundary.left_substitution); map_images("RIGHT",boundary.right_substitution);
		const struct pg_occurrence *endpoints[]={boundary.left,boundary.right};
		for (size_t side=0; side<2; ++side) {
			const struct pg_occurrence *point=endpoints[side];
			printf("ENDPOINT%s judgement%d context-depth%zu ",side ? "RIGHT" : "LEFT",point->judgement,context_depth(point->context));
			describe(stdout,point->core,point->context,0); putchar('\n');
			if (point->type) { printf("ENDPOINT_TYPE "); describe(stdout,point->type->core,point->type->context,0); putchar('\n'); }
			struct pg_c_acc_endpoint endpoint; int status=pg_c_acc_endpoint_read(&storage,point->core,nat,&endpoint);
			assert(!status); refusals+=endpoint_refusals(&storage,point->core,nat,&endpoint);
			printf("TARGET_ENDPOINT read%d",status);
			if (!status) {
				printf(" quoted%d arguments%zu constructors%zu",endpoint.quoted,endpoint.count,
					pg_data_layout_count(pg_data_declaration_layout(endpoint.declaration)));
				for (size_t j=0; j<endpoint.count; ++j) {
					struct pg_c_indexed_operand argument=pg_c_indexed_bound(endpoint.arguments[j]);
					printf(" ARG%zu=",j); describe(stdout,argument.term,point->context,0);
				}
			}
			putchar('\n');
		}
		for (size_t i=0; i<boundary.path_count; ++i) {
			printf("PATH%zu ",i); describe(stdout,boundary.paths[i]->core,boundary.paths[i]->context,0); putchar('\n');
		}
	}
	assert(program->graph.terms.count==terms && program->graph.objects.count==objects);
	assert(program->typing.proofs.count==proofs && program->typing.occurrences.count==occurrences);
	assert(program->typing.context_maps.count==maps && program->typing.contexts.count==contexts);
	assert(fields==3 && refusals==16);
	inspecting=0; printf("FIELDS%zu private-refusals%zu readonly owners unchanged\n",fields,refusals);
	pg_dag_destroy(&dag); pg_graph_destroy(&storage); pg_program_destroy(program); return ferror(stdout) ? 2 : 0;
}
