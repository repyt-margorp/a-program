#include "typing.h"
#include "classifier.h"
#include "computation.h"
#include <assert.h>

/* This downstream reader has no source, scheduler or evidence dependency. */
int inspect_semantic_root(const struct pg_occurrence *root)
{
	if (!root) return 0;
	assert(root->core);
	return 1;
}

/* Inspect a definition's suspended function without forcing it. */
void inspect_semantic_function(const struct pg_occurrence *root)
{
	assert(root && root->core->kind == PG_APPLICATION);
	const struct pg_term *head = root->core->as.application.function, *computation_type;
	assert(head->kind == PG_REFERENCE && head->as.reference == &pg_thunk_operation);
	assert(pg_thunk_type_view(root->classifier, &computation_type));
	assert(root->operand_count == 1 && !root->origin);
	assert(root->operands[0]->core == root->core->as.application.argument);
	assert(root->operands[0]->classifier == computation_type);
	root = root->operands[0];
	assert(root && root->core->kind == PG_LAMBDA);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	assert(pg_pi_view(root->classifier, &domain, &binder, &codomain));
	assert(domain && codomain && binder == root->core->as.lambda.binder);
	assert(root->type && root->type->core == root->classifier);
	const struct pg_occurrence *body = pg_occurrence_scoped_input(root, 0);
	assert(body && body->core == root->core->as.lambda.body);
	assert(body->context && body->context->parent == root->context);
	assert(body->context->binder == binder && body->context->declared_type == domain);
	assert(body->classifier == codomain);
}
