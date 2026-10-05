#include "emit.h"

/* Preserve source map/endpoint/frame/down/result-index readers unchanged. */
#include "../acc_indices/emit.c"

static int creation_to(FILE *out, struct pg_graph *storage,
	const struct pg_c_indexed_entry *entries)
{
	struct index_constructor constructors[3];
	if (index_constructors_read(storage,entries,constructors)) return -1;
	const struct pg_c_indexed_view *lt=&entries[2].view;
	const struct pg_context *prefix=pg_data_declaration_parameters(lt->declaration);
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration);
	struct image prior_indices[2][2];
	size_t prior_fields[3]={SIZE_MAX,SIZE_MAX,SIZE_MAX};
	for (size_t branch=0; branch<3; ++branch) {
		const struct pg_context *fields=pg_data_declaration_fields(lt->declaration,branch),*c=fields;
		const struct pg_context *ordered[3];
		for (size_t i=constructors[branch].count; i; --i,c=c->parent) ordered[i-1]=c;
		for (size_t i=0; i<constructors[branch].count; ++i) {
			if (constructors[branch].fields[i]!=INDEX_LT) continue;
			if (!branch || prior_fields[branch]!=SIZE_MAX) return -1;
			struct pg_c_indexed_operand head,args[16]; size_t count;
			if (pg_c_indexed_head(storage,(struct pg_c_indexed_operand){ordered[i]->declared_type,lt->environment},
				&head,&count,args) || count!=2 || !recipe_reference(head,prefix->binder)) return -1;
			for (size_t j=0; j<2; ++j) {
				struct image image;
				if (image_read(args[j].term,fields,nat,&image) || args[j].environment!=lt->environment
					|| image.slot<1 || image.slot>constructors[branch].count || image.successor) return -1;
				--image.slot;
				if (constructors[branch].fields[image.slot]!=INDEX_NAT) return -1;
				prior_indices[branch-1][j]=image;
			}
			prior_fields[branch]=i;
		}
		if ((branch!=0)!=(prior_fields[branch]!=SIZE_MAX)) return -1;
	}
	fputs("/* Actual admitted recursive LT field arguments drive creation checks. */\n"
		"static const struct gc_constructor gc_constructors[] = {\n"
		"\t{gi_constructors+0,SIZE_MAX,{{0,0},{0,0}}},\n",out);
	for (size_t branch=1; branch<3; ++branch) {
		fprintf(out,"\t{gi_constructors+%zu,%zu,",branch,prior_fields[branch]);
		images_write(out,prior_indices[branch-1],2); fputs("},\n",out);
	}
	fputs("};\n",out); return ferror(out) ? -1 : 0;
}

int pg_c_acc_create_emit(FILE *out, struct pg_graph *storage, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !storage || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=pg_c_acc_index_emit(temporary,storage,typing,root,reference,entries);
	if (!status) status=creation_to(temporary,storage,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
