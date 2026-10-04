#include "emit.h"
#include "read.h"

/* Keep the sealed map reader and use the same admitted occurrence edges. */
#include "../acc_transport/emit.c"

struct endpoint_descriptor { int quoted; size_t count; struct image indices[2]; };
struct endpoint_pair { struct endpoint_descriptor left,right; };

static int nominal_argument(struct pg_graph *storage, struct pg_c_indexed_operand operand,
	const struct pg_data_declaration *expected)
{
	struct pg_c_indexed_operand head,args[16]; size_t count;
	return !pg_c_indexed_head(storage,operand,&head,&count,args) && !count
		&& pg_data_declaration_view(head.term->as.reference)==expected;
}

static int endpoint_read(struct pg_graph *storage, const struct pg_occurrence *point,
	const struct pg_context *context, const struct pg_c_indexed_entry *entries,
	int quoted, struct endpoint_descriptor *out)
{
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration);
	struct pg_c_acc_endpoint endpoint;
	if (!point || point->context!=context
		|| pg_c_acc_endpoint_read(storage,point->core,nat,&endpoint) || endpoint.quoted!=quoted) return -1;
	size_t skip=quoted ? 2 : 0, count=quoted ? 1 : 2;
	if (endpoint.declaration!=entries[quoted ? 3 : 2].view.declaration || endpoint.count!=skip+count) return -1;
	if (quoted && (!nominal_argument(storage,endpoint.arguments[0],entries[1].view.declaration)
		|| !nominal_argument(storage,endpoint.arguments[1],entries[2].view.declaration))) return -1;
	struct endpoint_descriptor result={.quoted=quoted,.count=count};
	for (size_t i=0; i<count; ++i) {
		struct pg_c_indexed_operand operand=pg_c_indexed_bound(endpoint.arguments[skip+i]);
		if (image_read(operand.term,context,nat,result.indices+i)) return -1;
	}
	*out=result; return 0;
}

static void endpoint_write(FILE *out, const struct endpoint_descriptor *endpoint)
{
	fprintf(out,"{%s,%zu,",endpoint->quoted ? "GE_ACC" : "GE_LT",endpoint->count);
	images_write(out,endpoint->indices,endpoint->count); fputc('}',out);
}

static int endpoints_to(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_c_indexed_entry *entries)
{
	struct pg_graph storage; if (pg_graph_init(&storage)) return -1;
	struct pg_dag dag; if (pg_dag_init(&dag,child,NULL)) { pg_graph_destroy(&storage); return -1; }
	int status=-1; struct endpoint_pair pairs[3]; unsigned seen=0;
	const struct pg_data_layout *nat=pg_data_declaration_layout(entries[1].view.declaration);
	if (pg_dag_add(&dag,root)) goto done;
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
		if (endpoint_read(&storage,boundary.left,field->context,entries,!branch,&pairs[branch].left)
			|| endpoint_read(&storage,boundary.right,field->context,entries,!branch,&pairs[branch].right)) goto done;
		seen|=1u<<branch;
	}
	if (seen!=7) goto done;
	fputs("/* Actual admitted endpoint index operands, after the explicit known Nat.succ\n"
		"\t* Match clause. No source equality, cancellation or type evidence is made. */\n"
		"static const struct ge_boundary ge_boundaries[] = {\n",out);
	for (size_t i=0; i<3; ++i) {
		fprintf(out,"\t{&gt_boundaries[%zu],",i); endpoint_write(out,&pairs[i].left);
		fputc(',',out); endpoint_write(out,&pairs[i].right); fputs("},\n",out);
	}
	fputs("};\n",out); status=ferror(out) ? -1 : 0;
done:
	pg_dag_destroy(&dag); pg_graph_destroy(&storage); return status;
}

int pg_c_acc_endpoints_emit(FILE *out, const struct pg_typing *typing,
	const struct pg_occurrence *root, const struct pg_occurrence *reference,
	const struct pg_c_indexed_entry *entries)
{
	if (!out || !typing || !root || !reference || !entries) return -1;
	FILE *temporary=tmpfile(); if (!temporary) return -1;
	int status=descriptors_to(temporary,typing,root,reference,entries);
	if (!status) status=endpoints_to(temporary,typing,root,entries);
	if (!status && !fseek(temporary,0,SEEK_SET)) {
		char buffer[4096]; size_t count;
		while ((count=fread(buffer,1,sizeof(buffer),temporary)))
			if (fwrite(buffer,1,count,out)!=count) { status=-1; break; }
		if (ferror(temporary) || ferror(out)) status=-1;
	} else status=-1;
	if (fclose(temporary)) status=-1;
	return status;
}
