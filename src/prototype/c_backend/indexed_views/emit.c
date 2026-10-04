#include "emit.h"
#include "classifier.h"
#include "typing.h"
#include <string.h>

struct telescope {
	size_t count;
	const struct pg_context *fields[32];
};
struct emitter {
	FILE *output;
	struct pg_graph *storage;
	size_t count;
	const struct pg_c_indexed_entry *entries;
};

static int telescope(const struct pg_context *context, const struct pg_context *prefix, struct telescope *out)
{
	if (pg_context_extension_size(context,prefix,&out->count) || out->count > 32) return -1;
	for (size_t i = out->count; i; context = context->parent) out->fields[--i] = context;
	return 0;
}

static int token_type(const struct pg_term *type)
{
	uint64_t level; const struct pg_term *domain, *codomain; const struct pg_object *binder;
	for (size_t step = 0; step < 32; ++step) {
		if (pg_universe_level(type,&level)) return level == 0;
		if (!pg_pi_view(type,&domain,&binder,&codomain)) return 0;
		type = codomain;
	}
	return 0;
}

static int type_entry(struct emitter *e, size_t own, struct pg_c_indexed_operand operand, size_t *selected)
{
	struct pg_c_indexed_operand head, arguments[16]; size_t count;
	if (pg_c_indexed_head(e->storage,operand,&head,&count,arguments)) return -1;
	const struct pg_object *object = head.term->as.reference;
	const struct pg_context *prefix = pg_data_declaration_parameters(e->entries[own].view.declaration);
	const struct pg_data_declaration *declaration = pg_data_declaration_view(object);
	for (size_t i = 0; i < e->count; ++i) {
		const struct pg_c_indexed_view *v = &e->entries[i].view; size_t indices;
		const struct pg_context *parameters = pg_data_declaration_parameters(v->declaration);
		if (pg_context_extension_size(pg_data_declaration_indices(v->declaration),parameters,&indices)) return -1;
		if (i == own && object == prefix->binder) {
			if (count != indices) return -1;
			*selected = i; return 0;
		}
		if (declaration != v->declaration) continue;
		if (count != v->count + indices) return -1;
		struct telescope params;
		if (telescope(parameters->parent,NULL,&params) || params.count != v->count) return -1;
		for (size_t j = 0; j < v->count; ++j) {
			if (!token_type(params.fields[j]->declared_type)) continue;
			/* Match the exact selected closed type/family, not a new instance
				* inferred from a runtime value parameter or expected type. */
			struct pg_c_indexed_operand a = pg_c_indexed_bound(arguments[j]);
			struct pg_c_indexed_operand b = pg_c_indexed_bound(v->arguments[j]);
			if (!a.term || !b.term || a.term->kind != PG_REFERENCE || b.term->kind != PG_REFERENCE
				|| a.term->as.reference != b.term->as.reference) return -1;
		}
		*selected = i; return 0;
	}
	return -1;
}

static int pointer_type(struct emitter *e, size_t own, struct pg_c_indexed_operand operand)
{
	size_t selected;
	if (type_entry(e,own,operand,&selected)) return -1;
	fprintf(e->output,"const struct iv_%s *",e->entries[selected].name); return 0;
}

static int symbolic(struct emitter *e, size_t own, const struct telescope *fields,
	const struct telescope *arguments, const struct pg_term *term, unsigned depth)
{
	if (!term || depth == 64) return -1;
	FILE *out = e->output;
	if (term->kind == PG_APPLICATION) {
		if (symbolic(e,own,fields,arguments,term->as.application.function,depth+1)) return -1;
		fputc('(',out);
		if (symbolic(e,own,fields,arguments,term->as.application.argument,depth+1)) return -1;
		fputc(')',out); return 0;
	}
	if (term->kind != PG_REFERENCE) return -1;
	const struct pg_object *object = term->as.reference;
	const struct pg_context *prefix = pg_data_declaration_parameters(e->entries[own].view.declaration);
	if (object == prefix->binder) { fputs("self",out); return 0; }
	struct telescope params;
	if (telescope(prefix->parent,NULL,&params)) return -1;
	for (size_t i = 0; i < params.count; ++i) if (object == params.fields[i]->binder) {
		fprintf(out,"parameter%zu",i); return 0;
	}
	for (size_t i = 0; i < fields->count; ++i) if (object == fields->fields[i]->binder) {
		fprintf(out,"field%zu",i); return 0;
	}
	for (size_t i = 0; i < arguments->count; ++i) if (object == arguments->fields[i]->binder) {
		fprintf(out,"argument%zu",i); return 0;
	}
	for (size_t i = 0; i < e->count; ++i) {
		const struct pg_data_layout *layout = pg_data_declaration_layout(e->entries[i].view.declaration);
		size_t position;
		if (pg_data_constructor_position(layout,object,&position)) {
			fprintf(out,"%s_c%zu",e->entries[i].name,position); return 0;
		}
		if (object == pg_data_declaration_family(e->entries[i].view.declaration)) {
			fputs(e->entries[i].name,out); return 0;
		}
	}
	return -1;
}

static int callable(struct emitter *e, size_t own, size_t constructor, size_t field,
	const struct pg_term *type, const struct telescope *fields)
{
	const struct pg_c_indexed_view *v = &e->entries[own].view;
	const struct pg_object *self = pg_data_declaration_parameters(v->declaration)->binder;
	const struct pg_term *domain, *codomain; const struct pg_object *binder;
	struct telescope arguments = {0}; struct pg_context contexts[16];
	if (pg_data_recursive_field(type,self) != 1 || !pg_thunk_type_view(type,&type)) return -1;
	while (pg_pi_view(type,&domain,&binder,&codomain)) {
		if (arguments.count == 16) return -1;
		size_t n = arguments.count++;
		contexts[n] = (struct pg_context){.binder = binder,.declared_type = domain};
		arguments.fields[n] = &contexts[n]; type = codomain;
	}
	enum pg_totality totality;
	if (!arguments.count || !pg_pure_computation_type_view(type,&totality,&type) || totality != PG_TOTALITY_TOTAL) return -1;
	FILE *out = e->output;
	fprintf(out,"/* %s c%zu field%zu: borrowed synchronous callable Self;\n",e->entries[own].name,constructor,field);
	for (size_t i = 0; i < arguments.count; ++i) {
		fprintf(out,"\t* argument%zu: ",i);
		if (symbolic(e,own,fields,&arguments,arguments.fields[i]->declared_type,0)) return -1;
		fputs(";\n",out);
	}
	fputs("\t* result index: ",out);
	if (symbolic(e,own,fields,&arguments,type,0)) return -1;
	fprintf(out,". No source membership check is generated. */\nstruct iv_%s_c%zu_f%zu {\n\tint (*call)(void *context",e->entries[own].name,constructor,field);
	for (size_t i = 0; i < arguments.count; ++i) {
		fputs(", ",out);
		if (pointer_type(e,own,(struct pg_c_indexed_operand){arguments.fields[i]->declared_type,v->environment})) return -1;
		fprintf(out,"argument%zu",i);
	}
	fputs(", ",out);
	if (pointer_type(e,own,(struct pg_c_indexed_operand){type,v->environment})) return -1;
	fputs("*result);\n\tvoid *context;\n};\n\n",out); return 0;
}

static int declaration(struct emitter *e, size_t own)
{
	const struct pg_c_indexed_entry *entry = &e->entries[own];
	const struct pg_c_indexed_view *v = &entry->view;
	const struct pg_context *prefix = pg_data_declaration_parameters(v->declaration);
	const struct pg_data_layout *layout = pg_data_declaration_layout(v->declaration);
	struct telescope params, indices, fields, empty = {0};
	if (!prefix || telescope(prefix->parent,NULL,&params) || params.count != v->count
		|| telescope(pg_data_declaration_indices(v->declaration),prefix,&indices)) return -1;
	size_t constructors = pg_data_layout_count(layout);
	if (!constructors || constructors > 16) return -1;
	FILE *out = e->output;
	for (size_t c = 0; c < constructors; ++c) {
		if (telescope(pg_data_declaration_fields(v->declaration,c),prefix,&fields)) return -1;
		for (size_t f = 0; f < fields.count; ++f) {
			const struct pg_term *type;
			if (pg_thunk_type_view(fields.fields[f]->declared_type,&type)
				&& callable(e,own,c,f,fields.fields[f]->declared_type,&fields)) return -1;
		}
	}
	fprintf(out,"struct iv_%s {\n\tconst void *self_identity;\n",entry->name);
	for (size_t i = 0; i < params.count; ++i) {
		fputc('\t',out);
		if (token_type(params.fields[i]->declared_type)) fputs("const void *",out);
		else if (pointer_type(e,own,(struct pg_c_indexed_operand){params.fields[i]->declared_type,v->environment})) return -1;
		fprintf(out,"parameter%zu;\n",i);
	}
	for (size_t i = 0; i < indices.count; ++i) {
		fputc('\t',out);
		if (pointer_type(e,own,(struct pg_c_indexed_operand){indices.fields[i]->declared_type,v->environment})) return -1;
		fprintf(out,"index%zu;\n",i);
	}
	fputs("\tunsigned tag;\n\tunion {\n",out);
	size_t nc, nt; const struct pg_context *const *contexts; const struct pg_term *const *images;
	if (pg_data_declaration_pack(v->declaration,e->storage,&nc,&contexts,&nt,&images)) return -1;
	size_t width = params.count + 1 + indices.count;
	if (nc != constructors + 2 || nt != 1 + constructors * width) return -1;
	for (size_t c = 0; c < constructors; ++c) {
		if (telescope(pg_data_declaration_fields(v->declaration,c),prefix,&fields)) return -1;
		fprintf(out,"\t\t/* c%zu result [",c);
		for (size_t i = 0; i < width; ++i) {
			if (i) fputs(", ",out);
			if (symbolic(e,own,&fields,&empty,images[1+c*width+i],0)) return -1;
		}
		fputs("] in source parameter/Self/index order. */\n\t\tstruct {\n",out);
		if (!fields.count) fputs("\t\t\tunsigned char empty;\n",out);
		for (size_t f = 0; f < fields.count; ++f) {
			const struct pg_term *inner;
			int thunk = pg_thunk_type_view(fields.fields[f]->declared_type,&inner);
			if (!thunk) {
				fprintf(out,"\t\t\t/* field%zu: ",f);
				if (symbolic(e,own,&fields,&empty,fields.fields[f]->declared_type,0)) return -1;
				fputs(". */\n",out);
			}
			fputs("\t\t\t",out);
			if (thunk) fprintf(out,"struct iv_%s_c%zu_f%zu ",entry->name,c,f);
			else if (pointer_type(e,own,(struct pg_c_indexed_operand){fields.fields[f]->declared_type,v->environment})) return -1;
			fprintf(out,"field%zu;\n",f);
		}
		fprintf(out,"\t\t} c%zu;\n",c);
	}
	fputs("\t} fields;\n};\n\n",out); return 0;
}

int pg_c_indexed_emit(FILE *output, struct pg_graph *storage, size_t count, const struct pg_c_indexed_entry *entries)
{
	if (!output || !storage || !entries || !count || count > 16) return -1;
	struct emitter e = {output,storage,count,entries};
	for (size_t i = 0; i < count; ++i) {
		const char *name = entries[i].name;
		if (!name || !*name || !entries[i].view.declaration) return -1;
		for (const char *p = name; *p; ++p) if ((*p < 'a' || *p > 'z') && *p != '_') return -1;
		for (size_t j = 0; j < i; ++j) if (!strcmp(name,entries[j].name) || entries[i].view.declaration == entries[j].view.declaration) return -1;
	}
	fputs("#ifndef __ACC_INDEXED_DECLARATIONS_H__\n#define __ACC_INDEXED_DECLARATIONS_H__\n\n/* Generated from existing admitted descriptors, not generated QuickSort.\n\t* Source type/family tokens, value parameters and indices remain distinct.\n\t* All pointees/context/code must remain live; callbacks are synchronous.\n\t* The private status/result convention is not an A Program public ABI. */\n",output);
	for (size_t i = 0; i < count; ++i) fprintf(output,"struct iv_%s;\n",entries[i].name);
	fputc('\n',output);
	for (size_t i = 0; i < count; ++i) if (declaration(&e,i)) return -1;
	fputs("#endif\n",output); return ferror(output) ? -1 : 0;
}
