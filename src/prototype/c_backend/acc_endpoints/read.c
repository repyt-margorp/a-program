#include "read.h"
#include "classifier.h"

int pg_c_acc_endpoint_read(struct pg_graph *storage, const struct pg_term *term,
	const struct pg_data_layout *nat, struct pg_c_acc_endpoint *out)
{
	if (!storage || !term || !nat || !out) return -1;
	struct pg_c_indexed_operand operand={term,NULL}; int quoted=0;
	for (size_t step=0; step<64; ++step) {
		operand=pg_c_indexed_bound(operand);
		const struct pg_term *computation,*value;
		if (pg_thunk_type_view(operand.term,&computation)) {
			enum pg_totality totality;
			if (quoted || !pg_pure_computation_type_view(computation,&totality,&value)
				|| totality!=PG_TOTALITY_TOTAL) return -1;
			quoted=1; operand.term=value; continue;
		}
		struct pg_c_indexed_operand head,args[16]; size_t count;
		if (pg_c_indexed_head(storage,operand,&head,&count,args)) return -1;
		const struct pg_data_layout *layout=pg_data_layout_view(head.term->as.reference);
		if (!layout) {
			const struct pg_data_declaration *declaration=pg_data_declaration_view(head.term->as.reference);
			if (!declaration) return -1;
			struct pg_c_acc_endpoint result={.declaration=declaration,.quoted=quoted,.count=count};
			for (size_t i=0; i<count; ++i) result.arguments[i]=args[i];
			*out=result; return 0;
		}
		/* This is an explicit source Match on its constructor, not an equality
			* test or a predecessor inferred from equal machine integers. */
		if (layout!=nat || count!=pg_data_layout_count(layout)+1) return -1;
		struct pg_c_indexed_operand constructor,fields[16]; size_t arity,position,field_count;
		const struct pg_data_layout *constructor_layout;
		if (pg_c_indexed_head(storage,args[0],&constructor,&field_count,fields)
			|| !pg_data_constructor_view(constructor.term->as.reference,&constructor_layout,&position,&arity)
			|| constructor_layout!=nat || position!=1 || arity!=1 || field_count!=arity) return -1;
		struct pg_c_indexed_operand branch=pg_c_indexed_bound(args[position+1]);
		if (branch.term->kind!=PG_LAMBDA) return -1;
		struct pg_c_indexed_binding *binding=pg_alloc(storage,sizeof(*binding)); if (!binding) return -1;
		*binding=(struct pg_c_indexed_binding){branch.environment,branch.term->as.lambda.binder,fields[0]};
		operand=(struct pg_c_indexed_operand){branch.term->as.lambda.body,binding};
	}
	return -1;
}
