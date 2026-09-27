#include "synthesis_schema.h"
#include "iadt.h"

struct constructor_result_work { struct pg_synthesis_job *left, *right; };
struct schema_work {
	struct pg_index names;
	struct pg_synthesis_job *left, *right;
	const struct pg_data_declaration *nominal_input;
	const struct pg_data_schema *schema;
	struct source_constructor *members;
	size_t indexed, checked;
};

static void constructor_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void data_schema_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void schema_destroy(struct pg_synthesis_job *);
static const struct pg_synthesis_work_class CONSTRUCTOR_JOB[1] = {{
	.size = sizeof(struct constructor_result_work), .advance = constructor_step}};
static const struct pg_synthesis_work_class DATA_SCHEMA_JOB[1] = {{
	.size = sizeof(struct schema_work), .advance = data_schema_step, .destroy = schema_destroy}};

static void schema_destroy(struct pg_synthesis_job *job)
{
	struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	pg_index_destroy(&local->names);
}

struct pg_synthesis_job *pg_synthesis_data_schema(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration)
{
	return pg_synthesis_data_schema_at(synthesis, parameters, declaration, NULL);
}

struct pg_synthesis_job *pg_synthesis_data_schema_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration,
	const struct pg_data_declaration *allocation)
{
	if (!parameters || parameters->owner != synthesis->owner_key) return NULL;
	if (!declaration || declaration->kind != PG_SYNTAX_DECLARATION) return NULL;
	const void *inputs[] = {parameters, declaration};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, DATA_SCHEMA_JOB, 2, inputs);
	if (!job || !allocation) return job;
	struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	if (local->nominal_input) return local->nominal_input == allocation ? job : NULL;
	if (local->left)
		return pg_data_schema_declaration(local->schema) == allocation ? job : NULL;
	local->nominal_input = allocation;
	return job;
}

const struct pg_data_schema *pg_synthesis_schema_result(const struct pg_synthesis_job *job)
{
	if (!job || job->role != DATA_SCHEMA_JOB || job->status != PG_SYNTHESIS_DONE) return NULL;
	const struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	return local->schema;
}

const struct source_constructor *pg_synthesis_schema_members(const struct pg_synthesis_job *job)
{
	if (!pg_synthesis_schema_result(job)) return NULL;
	const struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	return local->members;
}

const struct pg_data_declaration *pg_synthesis_schema_allocation(const struct pg_synthesis_job *job)
{
	if (!job || job->role != DATA_SCHEMA_JOB) return NULL;
	const struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	return local->nominal_input ? local->nominal_input : pg_data_schema_declaration(local->schema);
}

static void constructor_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct constructor_result_work *local = pg_synthesis_work_state(job, CONSTRUCTOR_JOB);
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = job->inputs[2];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (pg_synthesis_await(synthesis, job, scope->context_job)) return;
	if (!pg_synthesis_scope_context(scope)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (!local->left) {
		if (syntax->kind == PG_SYNTAX_LAMBDA) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->left = pg_synthesis_telescope(synthesis, scope, syntax);
		pg_synthesis_subscribe(synthesis, job, local->left, 0);
		return;
	}
	if (local->left->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->left->status); return; }
	struct pg_synthesis_job *indices = (struct pg_synthesis_job *)job->inputs[1];
	if (pg_synthesis_await(synthesis, job, indices)) return;
	if (!local->right) {
		local->right = pg_synthesis_data_result(synthesis, pg_synthesis_telescope_scope(local->left),
			pg_synthesis_scope_context(scope), indices->result, pg_synthesis_telescope_body(local->left));
		pg_synthesis_subscribe(synthesis, job, local->right, 0);
		return;
	}
	job->result = local->right->result;
	pg_synthesis_finish(synthesis, job, local->right->status);
}

/* Recomputed field annotations can have fresh binders inside their types.
 * Keep the saved declaration immutable, and transport the independently
 * synthesized result through a checked variable substitution instead. */
static const struct pg_evidence *schema_result_context(struct pg_typing *typing,
	const struct pg_evidence *result, const struct pg_evidence *indices,
	const struct pg_context *target)
{
	const struct pg_evidence *declared = pg_evidence_premise(result, 0);
	if (pg_evidence_context(declared) != pg_evidence_context(indices))
		result = pg_prove_substitution_compose(typing,
			pg_prove_telescope_correspondence(typing, indices, declared), result);
	if (!result) return NULL;
	const struct pg_evidence *source = pg_evidence_premise(result, 1);
	if (pg_evidence_context(source) == target) return result;
	return pg_prove_substitution_compose(typing, result,
		pg_prove_telescope_correspondence(typing, source, pg_prove_context_alpha(typing, source, target)));
}

/* Compile direct typed index projections once per constructor. These paths
 * describe source argument insertion; they never authorize a kernel rule. */
static enum pg_synthesis_status constructor_index_paths(struct pg_synthesis *synthesis,
	const struct pg_context *prefix, const struct pg_context *fields,
	struct source_constructor *member)
{
	if (!member->implicit_count) return PG_SYNTHESIS_DONE;
	size_t count;
	if (pg_context_extension_size(fields, prefix, &count) || member->implicit_count > count)
		return PG_SYNTHESIS_ERROR;
	if (count > SIZE_MAX / sizeof(const struct pg_context *) ||
		member->implicit_count > SIZE_MAX / sizeof(struct constructor_index_path)) return PG_SYNTHESIS_ERROR;
	struct pg_graph scratch = {0};
	const struct pg_context **ordered = pg_alloc(&scratch, count * sizeof(*ordered));
	struct constructor_index_path *paths = pg_alloc(synthesis->typing->graph,
		member->implicit_count * sizeof(*paths));
	if (!ordered || !paths) { pg_graph_destroy(&scratch); return PG_SYNTHESIS_ERROR; }
	for (size_t i = count; i; --i, fields = fields->parent) ordered[i - 1] = fields;
	for (size_t i = 0; i < member->implicit_count; ++i)
		paths[i] = (struct constructor_index_path){.binder = ordered[i]->binder, .field = SIZE_MAX};
	for (size_t field = member->implicit_count; field < count; ++field) {
		const struct pg_term *type = ordered[field]->declared_type, *head = type;
		size_t arity = 0, index_count;
		while (head->kind == PG_APPLICATION) { ++arity; head = head->as.application.function; }
		if (head->kind != PG_REFERENCE) continue;
		const struct pg_data_declaration *family = pg_data_declaration_view(head->as.reference);
		if (family) {
			if (pg_context_extension_size(pg_data_declaration_indices(family),
				pg_data_declaration_parameters(family), &index_count)) continue;
		} else if (prefix && prefix->indices && head->as.reference == prefix->binder) {
			if (pg_context_extension_size(prefix->indices, prefix->parent, &index_count)) continue;
		} else continue;
		if (index_count > arity) continue;
		for (size_t offset = 0; offset < index_count; ++offset, type = type->as.application.function) {
			const struct pg_term *argument = type->as.application.argument;
			if (argument->kind != PG_REFERENCE) continue;
			for (size_t i = 0; i < member->implicit_count; ++i) {
				if (paths[i].binder != argument->as.reference || paths[i].field != SIZE_MAX) continue;
				paths[i].field = field;
				paths[i].index = index_count - offset - 1;
			}
		}
	}
	enum pg_synthesis_status status = PG_SYNTHESIS_DONE;
	for (size_t i = 0; i < member->implicit_count; ++i)
		if (paths[i].field == SIZE_MAX) status = PG_SYNTHESIS_REJECTED;
	pg_graph_destroy(&scratch);
	member->indices = paths;
	return status;
}

int pg_synthesis_visit_rejected_index_paths(const struct pg_synthesis *synthesis,
	int (*visit)(void *, struct pg_token, struct pg_token, size_t, size_t), void *owner)
{
	if (!synthesis || !visit) return -1;
	for (size_t bucket = 0; bucket < synthesis->jobs.capacity; ++bucket)
		for (const struct pg_index_entry *entry = synthesis->jobs.buckets[bucket]; entry; entry = entry->next) {
			const struct pg_synthesis_job *job = (const void *)entry;
			if (job->role != DATA_SCHEMA_JOB || job->status != PG_SYNTHESIS_REJECTED) continue;
			const struct schema_work *state = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
			const struct pg_syntax *constructors = pg_synthesis_telescope_body(state->right);
			if (!constructors || state->checked == constructors->item_count || !state->members) continue;
			const struct source_constructor *member = &state->members[state->checked];
			if (!member->indices) continue;
			const struct pg_syntax *telescope = member->telescope;
			for (size_t i = 0; i < member->implicit_count; ++i, telescope = telescope->right) {
				const struct constructor_index_path *path = &member->indices[i];
				size_t field = path->field == SIZE_MAX ? SIZE_MAX : path->field - member->implicit_count;
				int status = visit(owner, constructors->items[state->checked].name,
					telescope->token, field, path->index);
				if (status) return status;
			}
		}
	return 0;
}

static void data_schema_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct schema_work *local = pg_synthesis_work_state(job, DATA_SCHEMA_JOB);
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = job->inputs[1];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (pg_synthesis_await(synthesis, job, scope->context_job)) return;
	if (!pg_synthesis_scope_context(scope)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (!local->left) {
		local->left = local->nominal_input ? pg_synthesis_telescope_at(synthesis, scope, syntax->left,
			pg_data_declaration_parameters(local->nominal_input), pg_data_declaration_indices(local->nominal_input))
			: pg_synthesis_telescope(synthesis, scope, syntax->left);
		if (!local->left) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		local->right = pg_synthesis_telescope_structure(synthesis, scope, syntax->left);
		pg_synthesis_subscribe(synthesis, job, local->right, 0);
		return;
	}
	if (local->right->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->right->status); return; }
	const struct pg_syntax *constructors = pg_synthesis_telescope_body(local->right);
	if (!local->names.capacity) {
		if (constructors->kind != PG_SYNTAX_CONSTRUCTORS) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (pg_index_init(&local->names) != 0) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		size_t count = constructors->item_count;
		if (count > SIZE_MAX / sizeof(*local->members)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		local->members = pg_alloc(synthesis->typing->graph, count * sizeof(*local->members));
		if (count && !local->members) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (local->indexed < constructors->item_count) {
		size_t i = local->indexed++;
		const struct pg_syntax_item *item = &constructors->items[i];
		struct source_constructor *member = &local->members[i];
		member->telescope = pg_syntax_constructor_telescope(synthesis->typing->graph,
			syntax, i, &member->implicit_count);
		if (!member->telescope) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (local->nominal_input) {
			const struct pg_context *prefix = pg_data_declaration_parameters(local->nominal_input);
			if (i >= pg_data_layout_count(pg_data_declaration_layout(local->nominal_input))) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			if (!pg_synthesis_telescope_at(synthesis, scope, member->telescope, prefix,
				pg_data_declaration_fields(local->nominal_input, i))) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
		}
		int status = pg_synthesis_register_name(synthesis, &local->names, item->name);
		if (status) { pg_synthesis_finish(synthesis, job, status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
		const void *inputs[] = {scope, local->left, member->telescope};
		struct pg_synthesis_job *producer = pg_synthesis_work_request(synthesis, CONSTRUCTOR_JOB, 3, inputs);
		if (!producer) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		member->producer = producer;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (local->checked == constructors->item_count) {
		if (pg_synthesis_await(synthesis, job, local->left)) return;
		const struct pg_evidence *indices = local->left->result;
		if (local->nominal_input) indices = pg_prove_context_alpha(synthesis->typing, indices,
			pg_data_declaration_indices(local->nominal_input));
		if (!indices) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		const struct pg_data_signature *signature = pg_data_signature(synthesis->typing, pg_synthesis_scope_context(scope), indices);
		if (!signature) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_graph temporary = {0};
		const struct pg_evidence **results = NULL;
		if (local->checked <= SIZE_MAX / sizeof(*results))
			results = pg_alloc(&temporary, local->checked * sizeof(*results));
		if (local->checked && !results) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		for (size_t i = 0; i < local->checked; ++i) {
			results[i] = local->members[i].producer->result;
			if (local->nominal_input) results[i] = schema_result_context(synthesis->typing, results[i], indices,
				pg_data_declaration_fields(local->nominal_input, i));
		}
		local->schema = local->nominal_input
			? pg_data_schema_check(synthesis->typing, local->nominal_input, signature, local->checked, results)
			: pg_data_schema(synthesis->typing, signature, local->checked, results);
		pg_graph_destroy(&temporary);
		pg_synthesis_finish(synthesis, job, local->schema ? PG_SYNTHESIS_DONE
			: local->nominal_input ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
		return;
	}
	struct source_constructor *member = &local->members[local->checked];
	struct pg_synthesis_job *producer = member->producer;
	if (pg_synthesis_await(synthesis, job, producer)) return;
	enum pg_synthesis_status recovery = constructor_index_paths(synthesis,
		pg_evidence_context(pg_synthesis_scope_context(scope)),
		pg_evidence_context(pg_evidence_premise(producer->result, 1)), member);
	if (recovery != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, recovery); return; }
	++local->checked;
	pg_synthesis_enqueue(synthesis, job);
}
