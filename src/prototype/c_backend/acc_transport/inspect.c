/* Reuse the sealed readonly owner/field inspection helpers. The new output
	* describes actual ordered map images; it does not check or execute Identity. */
#define main c42_inspection_main
#include "../acc_down/inspect.c"
#undef main

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
	size_t terms=program->graph.terms.count,objects=program->graph.objects.count;
	size_t proofs=program->typing.proofs.count,occurrences=program->typing.occurrences.count;
	size_t maps=program->typing.context_maps.count,contexts=program->typing.contexts.count;
	struct pg_dag dag; assert(!pg_dag_init(&dag,child,NULL) && !pg_dag_add(&dag,root));
	inspecting=1; size_t fields=0;
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
		for (size_t i=0; i<boundary.path_count; ++i) {
			printf("PATH%zu ",i); describe(stdout,boundary.paths[i]->core,boundary.paths[i]->context,0); putchar('\n');
		}
	}
	assert(program->graph.terms.count==terms && program->graph.objects.count==objects);
	assert(program->typing.proofs.count==proofs && program->typing.occurrences.count==occurrences);
	assert(program->typing.context_maps.count==maps && program->typing.contexts.count==contexts);
	inspecting=0; printf("FIELDS%zu readonly owners unchanged\n",fields);
	pg_dag_destroy(&dag); pg_program_destroy(program); return ferror(stdout) ? 2 : 0;
}
