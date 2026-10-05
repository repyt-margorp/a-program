#include "emit.h"

/* Retain the sealed source maps/endpoints/frame/down classifier readers. */
#include "../acc_recipe/emit.c"

enum index_field_kind { INDEX_NAT, INDEX_LT };
struct index_constructor {
	size_t count;
	enum index_field_kind fields[3];
	struct image indices[2];
};

static int index_constructors_read(struct pg_graph *storage,
	const struct pg_c_indexed_entry *entries, struct index_constructor *out)
{
	const struct pg_c_indexed_view *lt=&entries[2].view;
	const struct pg_data_declaration *declaration=lt->declaration;
	const struct pg_data_layout *layout=pg_data_declaration_layout(declaration),
		*nat=pg_data_declaration_layout(entries[1].view.declaration);
	const struct pg_context *prefix=pg_data_declaration_parameters(declaration),
		*indices=pg_data_declaration_indices(declaration);
	size_t count,depth;
	if (!prefix || prefix->parent || lt->count || pg_data_layout_count(layout)!=3
		|| pg_context_extension_size(indices,prefix,&count) || count!=2
		|| pg_context_extension_size(indices,NULL,&depth) || depth!=3) return -1;
	for (const struct pg_context *c=indices; c!=prefix; c=c->parent)
		if (!nominal_argument(storage,(struct pg_c_indexed_operand){c->declared_type,lt->environment},
			entries[1].view.declaration)) return -1;
	size_t scope_count,term_count; const struct pg_context *const *scopes;
	const struct pg_term *const *terms;
	if (pg_data_declaration_pack(declaration,storage,&scope_count,&scopes,&term_count,&terms)
		|| scope_count!=5 || term_count!=1+3*depth) return -1;
	struct index_constructor constructors[3];
	for (size_t branch=0; branch<3; ++branch) {
		const struct pg_context *fields=pg_data_declaration_fields(declaration,branch);
		if (scopes[branch+2]!=fields || pg_context_extension_size(fields,prefix,&count)
			|| count!=(branch ? 3u : 1u)) return -1;
		const struct pg_context *ordered[3]; const struct pg_context *c=fields;
		for (size_t i=count; i; --i,c=c->parent) ordered[i-1]=c;
		struct index_constructor result={.count=count};
		for (size_t i=0; i<count; ++i) {
			struct pg_c_indexed_operand operand={ordered[i]->declared_type,lt->environment};
			if (nominal_argument(storage,operand,entries[1].view.declaration)) result.fields[i]=INDEX_NAT;
			else {
				/* The prior field is the exact recursive LT applied to the two
					* earlier Nat binders, not a same-shaped nominal replacement. */
				struct pg_c_indexed_operand head,args[16]; size_t arity;
				if (i!=2 || pg_c_indexed_head(storage,operand,&head,&arity,args) || arity!=2
					|| !recipe_reference(head,prefix->binder)
					|| !recipe_reference(args[0],ordered[0]->binder)
					|| !recipe_reference(args[1],ordered[1]->binder)) return -1;
				result.fields[i]=INDEX_LT;
			}
			if (result.fields[i]!=(i==2 ? INDEX_LT : INDEX_NAT)) return -1;
		}
		const struct pg_term *const *images=terms+1+branch*depth;
		if (images[0]->kind!=PG_REFERENCE || images[0]->as.reference!=prefix->binder) return -1;
		for (size_t i=0; i<2; ++i) {
			struct image image;
			if (image_read(images[1+i],fields,nat,&image) || image.slot<1 || image.slot>count) return -1;
			--image.slot;
			if (result.fields[image.slot]!=INDEX_NAT) return -1;
			result.indices[i]=image;
		}
		constructors[branch]=result;
	}
	for (size_t i=0; i<3; ++i) out[i]=constructors[i];
	return 0;
}

static int index_constructors_to(FILE *out, struct pg_graph *storage,
	const struct pg_c_indexed_entry *entries)
{
	struct index_constructor constructors[3];
	if (index_constructors_read(storage,entries,constructors)) return -1;
	fputs("/* Actual admitted LT constructor field classifiers and ordered result\n"
		"\t* images drive finite Nat index checks; concrete storage remains manual. */\n"
		"static const struct gi_constructor gi_constructors[] = {\n",out);
	for (size_t branch=0; branch<3; ++branch) {
		const struct index_constructor *c=constructors+branch;
		fprintf(out,"\t{%zu,{",c->count);
		for (size_t i=0; i<c->count; ++i) fprintf(out,"%s%s",i ? "," : "",c->fields[i]==INDEX_NAT ? "GI_NAT" : "GI_LT");
		fputs("},",out); images_write(out,c->indices,2); fputs("},\n",out);
	}
	fputs("};\n",out); return ferror(out) ? -1 : 0;
}

int pg_c_lt_index_recipe_emit(FILE *out, struct pg_graph *storage,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !entries) return -1;
	/* Validation completes before the first output byte. */
	return index_constructors_to(out,storage,entries);
}

int pg_c_acc_index_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=pg_c_acc_recipe_emit(temporary,storage,typing,root,reference,entries);
	if (!status) status=index_constructors_to(temporary,storage,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
