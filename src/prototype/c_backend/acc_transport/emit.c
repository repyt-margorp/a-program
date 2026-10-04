#include "emit.h"
#include "action.h"
#include "dag.h"

/* Reuse the sealed occurrence-edge reader, not a new source checker. */
#include "../acc_capture/emit.c"

struct image { size_t slot; int successor; };
struct boundary_descriptor {
	const struct pg_context *source,*destination;
	size_t count,depth,paths[2];
	struct image left[10],right[10];
	int direction;
};

static size_t slot_of(const struct pg_context *context, const struct pg_object *binder)
{
	size_t depth=0;
	for (const struct pg_context *c=context; c; c=c->parent) ++depth;
	for (const struct pg_context *c=context; c; c=c->parent) {
		--depth;
		if (c->binder==binder) return depth;
	}
	return SIZE_MAX;
}

static int image_read(const struct pg_term *t, const struct pg_context *context,
	const struct pg_data_layout *nat, struct image *out)
{
	int successor=0; size_t position;
	if (t->kind==PG_APPLICATION) {
		const struct pg_term *f=t->as.application.function;
		if (f->kind!=PG_REFERENCE || !pg_data_constructor_position(nat,f->as.reference,&position) || position!=1) return -1;
		successor=1; t=t->as.application.argument;
	}
	if (t->kind!=PG_REFERENCE || t->as.reference->kind!=PG_BINDER) return -1;
	size_t slot=slot_of(context,t->as.reference); if (slot==SIZE_MAX) return -1;
	*out=(struct image){slot,successor}; return 0;
}

static int descriptor_read(const struct pg_identity_boundary *b,
	const struct pg_occurrence *field, enum pg_identity_direction direction,
	const struct pg_data_layout *nat, struct boundary_descriptor *out)
{
	const struct pg_context_map *left=b->left_substitution,*right=b->right_substitution;
	if (!left || !right || left->source!=right->source || left->destination!=right->destination
		|| left->destination!=field->context || b->family->context!=left->source
		|| left->count!=right->count || b->path_count!=2 || left->count>10) return -1;
	struct boundary_descriptor result={.source=left->source,.destination=left->destination,.count=left->count,
		.direction=direction==PG_IDENTITY_RIGHT ? 1 : 0};
	for (const struct pg_context *c=result.destination; c; c=c->parent) ++result.depth;
	if (result.depth>12) return -1;
	for (size_t i=0; i<left->count; ++i) {
		if (left->images[i]->context!=left->destination || right->images[i]->context!=right->destination
			|| image_read(left->images[i]->core,left->destination,nat,result.left+i)
			|| image_read(right->images[i]->core,right->destination,nat,result.right+i)) return -1;
	}
	for (size_t i=0; i<2; ++i) {
		const struct pg_occurrence *path=b->paths[i]; struct image image;
		if (path->context!=result.destination || image_read(path->core,path->context,nat,&image) || image.successor) return -1;
		result.paths[i]=image.slot;
	}
	*out=result; return 0;
}

static void images_write(FILE *out, const struct image *images, size_t count)
{
	fputc('{',out);
	for (size_t i=0; i<count; ++i) fprintf(out,"%s{%zu,%d}",i ? "," : "",images[i].slot,images[i].successor);
	fputc('}',out);
}

static int descriptors_to(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (pg_alpha_equal(root->core,reference->core)!=1) return -1;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration);
	if (pg_data_layout_count(nat)!=2) return -1;
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) return -1;
	int status=-1; struct boundary_descriptor descriptors[3]; unsigned seen=0;
	if (pg_dag_add(&dag,root)) goto done;
	for (const struct pg_dag_node *node=dag.first; node; node=node->next) {
		const struct pg_occurrence *s=node->key; const struct pg_term *family,*value;
		enum pg_identity_direction direction; int lift;
		if (!pg_identity_field_view(s->core,&family,&value,&direction,&lift)) continue;
		const struct pg_evidence *proof=NULL;
		while ((proof=pg_evidence_for_subject(typing,s,proof))) if (pg_evidence_rule(proof)==PG_IDENTITY_TRANSPORT) break;
		if (!proof || s->operand_count!=2 || s->operands[0]->core!=family || s->operands[1]->core!=value) continue;
		if (!pg_evidence_owned_by(proof,typing) || lift) goto done;
		struct pg_identity_boundary boundary; const struct pg_occurrence *formation=s->operands[0]->type;
		int found=0;
		for (size_t wrappers=0; formation && wrappers<256; ++wrappers,formation=formation->origin)
			if (pg_identity_boundary_view(formation,&boundary)) { found=1; break; }
		if (!found) goto done;
		struct boundary_descriptor descriptor;
		if (descriptor_read(&boundary,s,direction,nat,&descriptor)) goto done;
		size_t branch=descriptor.direction ? 0 : descriptor.right[descriptor.count-2].successor ? 2 : 1;
		if ((seen&(1u<<branch)) || descriptor.count!=(branch ? 10u : 6u)
			|| descriptor.depth!=(branch ? 12u : 10u)) goto done;
		descriptors[branch]=descriptor; seen|=1u<<branch;
	}
	if (seen!=7) goto done;
	/* Scope tokens preserve actual sharing/distinction; they are target-local
		* identities, not serialized contexts or a new scope typing decision. */
	const struct pg_context *scopes[6]; size_t scope_count=0,source[3],destination[3];
	for (size_t i=0; i<3; ++i) for (size_t side=0; side<2; ++side) {
		const struct pg_context *context=side ? descriptors[i].destination : descriptors[i].source;
		size_t slot=0; while (slot<scope_count && scopes[slot]!=context) ++slot;
		if (slot==scope_count) scopes[scope_count++]=context;
		if (side) destination[i]=slot; else source[i]=slot;
	}
	fputs("/* Actual admitted ordered map images and path slots. Target scope tokens\n"
		"\t* preserve sharing/distinction; complete source context semantics are not\n"
		"\t* rechecked here. Manual bounded action interpretation remains separate. */\n",out);
	fputs("static const struct gt_scope gt_scopes[] = {\n",out);
	for (size_t i=0; i<scope_count; ++i) {
		size_t depth=0; for (const struct pg_context *c=scopes[i]; c; c=c->parent) ++depth;
		fprintf(out,"\t{%zu,%zu},\n",i,depth);
	}
	fputs("};\nstatic const struct gt_boundary gt_boundaries[] = {\n",out);
	for (size_t i=0; i<3; ++i) {
		const struct boundary_descriptor *d=descriptors+i;
		fprintf(out,"\t{&gt_scopes[%zu],&gt_scopes[%zu],%zu,%d,",source[i],destination[i],d->count,d->direction);
		images_write(out,d->left,d->count); fputc(',',out); images_write(out,d->right,d->count);
		fprintf(out,",{%zu,%zu}},\n",d->paths[0],d->paths[1]);
	}
	fputs("};\n",out); status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); return status;
}

int pg_c_acc_transport_emit(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=descriptors_to(temporary,typing,root,reference,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
