#include "emit.h"

/* Reuse admitted field/map/endpoint readers and the actual callback selector. */
#include "../acc_endpoints/emit.c"

struct frame_descriptor { size_t count,slots[12]; };

static int frame_prefix(const struct pg_typing *typing, const struct pg_occurrence *root,
	const struct pg_dag *dag, const struct pg_data_layout *lt,
	const struct pg_object **objects, const struct pg_term **branches)
{
	const struct pg_term *lambda=carrier(root->core),*second;
	if (!lambda || lambda->kind!=PG_LAMBDA || (second=lambda->as.lambda.body)->kind!=PG_LAMBDA) return -1;
	const struct pg_occurrence *fold=NULL;
	for (const struct pg_dag_node *node=dag->first; node; node=node->next) {
		const struct pg_occurrence *s=node->key;
		if (!s->induction || s->induction->count!=1 || !s->context || s->context->binder!=second->as.lambda.binder) continue;
		size_t captures; if (pg_context_extension_size(s->context,NULL,&captures) || captures!=2) continue;
		const struct pg_evidence *proof=NULL; struct pg_elimination_inputs inputs;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (!pg_elimination_view(typing,proof,&inputs)) break;
		if (!proof || !pg_evidence_owned_by(proof,typing) || inputs.count!=1) continue;
		const struct pg_occurrence *scrutinee=pg_evidence_subject(inputs.scrutinee);
		if (scrutinee->core->kind!=PG_REFERENCE || scrutinee->core->as.reference!=second->as.lambda.binder) continue;
		if (fold) return -1;
		fold=s;
	}
	if (!fold || fold->operand_count!=4) return -1;
	const struct pg_context *ih=fold->induction->clauses[0],*raw=ih ? ih->parent : NULL,*current=raw ? raw->parent : NULL;
	if (!current || current->parent!=fold->context) return -1;
	objects[0]=lambda->as.lambda.binder; objects[1]=second->as.lambda.binder;
	objects[2]=current->binder; objects[3]=raw->binder; objects[4]=ih->binder;
	const struct pg_term *body=fold->operands[1]->core;
	for (size_t i=2; i<5; ++i) {
		if (body->kind!=PG_LAMBDA || body->as.lambda.binder!=objects[i]) return -1;
		body=body->as.lambda.body;
	}
	const struct pg_term *cb=callback(body,pg_data_matcher(lt),0); if (!cb) return -1;
	objects[5]=cb->as.lambda.binder; objects[6]=cb->as.lambda.body->as.lambda.binder;
	const struct pg_term *head,*args[16];
	if (spine(cb->as.lambda.body->as.lambda.body,&head,args)!=6 || head->kind!=PG_REFERENCE
		|| head->as.reference!=pg_data_matcher(lt) || args[0]->kind!=PG_REFERENCE || args[0]->as.reference!=objects[6]) return -1;
	for (size_t i=0; i<3; ++i) branches[i]=args[i+1];
	return 0;
}

static int frame_to(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; struct frame_descriptor descriptors[3]; unsigned seen=0;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration),
		*lt=pg_data_declaration_layout(entries[2].view.declaration);
	const struct pg_object *prefix[7]; const struct pg_term *branches[3];
	if (pg_dag_add(&dag,root) || frame_prefix(typing,root,&dag,lt,prefix,branches)) goto done;
	for (const struct pg_dag_node *node=dag.first; node; node=node->next) {
		const struct pg_occurrence *field=node->key; const struct pg_term *family,*value;
		enum pg_identity_direction direction; int lift;
		if (!pg_identity_field_view(field->core,&family,&value,&direction,&lift)) continue;
		const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(typing,field,proof)))
			if (pg_evidence_rule(proof)==PG_IDENTITY_TRANSPORT) break;
		if (!proof || field->operand_count!=2 || field->operands[0]->core!=family || field->operands[1]->core!=value) continue;
		if (!pg_evidence_owned_by(proof,typing) || lift) goto done;
		struct pg_identity_boundary boundary; const struct pg_occurrence *formation=field->operands[0]->type;
		int found=0;
		for (size_t wrappers=0; formation && wrappers<256; ++wrappers,formation=formation->origin)
			if (pg_identity_boundary_view(formation,&boundary)) { found=1; break; }
		if (!found) goto done;
		struct boundary_descriptor map;
		if (descriptor_read(&boundary,field,direction,nat,&map)) goto done;
		size_t branch=map.direction ? 0 : map.right[map.count-2].successor ? 2 : 1;
		if ((seen&(1u<<branch)) || map.count!=(branch ? 10u : 6u) || map.depth!=(branch ? 12u : 10u)) goto done;
		struct frame_descriptor result={.count=map.depth}; const struct pg_object *objects[12];
		for (size_t i=0; i<7; ++i) objects[i]=prefix[i];
		const struct pg_term *body=branches[branch];
		for (size_t i=7; i<result.count; ++i) {
			if (body->kind!=PG_LAMBDA) goto done;
			objects[i]=body->as.lambda.binder; body=body->as.lambda.body;
		}
		unsigned occupied=0;
		for (size_t i=0; i<result.count; ++i) {
			size_t slot=slot_of(field->context,objects[i]);
			if (slot>=result.count || (occupied&(1u<<slot))) goto done;
			result.slots[i]=slot; occupied|=1u<<slot;
		}
		if (occupied!=(1u<<result.count)-1 || result.slots[result.count-2]!=map.paths[0]
			|| result.slots[result.count-1]!=map.paths[1]) goto done;
		descriptors[branch]=result; seen|=1u<<branch;
	}
	if (seen!=7) goto done;
	const char *prefix_roles[]={"GF_PARAMETER","GF_PROOF","GF_CURRENT","GF_RAW_DOWN","GF_FOLDED_IH","GF_ARGUMENT","GF_EDGE"};
	fputs("/* Actual admitted parameter/Fold/callback/constructor binders locate\n"
		"\t* every frame slot. Role representations remain bounded target choices. */\n"
		"static const struct gf_frame gf_frames[] = {\n",out);
	for (size_t branch=0; branch<3; ++branch) {
		const struct frame_descriptor *d=descriptors+branch;
		fprintf(out,"\t{&gt_boundaries[%zu],%zu,{",branch,d->count);
		for (size_t i=0; i<d->count; ++i) {
			const char *role=i<7 ? prefix_roles[i] : i==d->count-2 ? "GF_PATH0" : i==d->count-1 ? "GF_PATH1"
				: i==7 ? "GF_CONSTRUCTOR_NAT0" : i==8 ? "GF_CONSTRUCTOR_NAT1" : "GF_CONSTRUCTOR_PRIOR";
			fprintf(out,"%s{%zu,%s}",i ? "," : "",d->slots[i],role);
		}
		fputs("}},\n",out);
	}
	fputs("};\n",out); status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}

int pg_c_acc_frame_emit(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=descriptors_to(temporary,typing,root,reference,entries);
	if (!status) status=endpoints_to(temporary,typing,root,entries);
	if (!status) status=frame_to(temporary,typing,root,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
