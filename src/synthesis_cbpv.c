#include "synthesis_source.h"
#include "computation.h"
#include "derivation.h"
#include "synthesis_effect.h"
#include "effect_inference.h"
#include <string.h>

/* The source owner holds only its actual unfinished sequence cursor. */
struct source_block_work {
	size_t next, end;
	const struct pg_source_scope *scope;
	const struct pg_source_scope **scopes;
	const struct pg_synthesis_sequence_frame *frames;
	struct pg_synthesis_job *tail;
	struct pg_index names;
};
static void source_block_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void source_block_destroy(struct pg_synthesis_job *);
static struct pg_synthesis_projection source_block_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_input source_block_output(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class SOURCE_BLOCK[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct source_block_work),
	.source_key = 1, .advance = source_block_step, .destroy = source_block_destroy,
	.project = source_block_projection, .output = source_block_output}};

const struct pg_syntax *pg_synthesis_block_syntax(const struct pg_syntax *syntax)
{
	if (!syntax) return NULL;
	if (syntax->kind == PG_SYNTAX_BLOCK) return syntax;
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_BLOCK) return syntax->left;
	return NULL;
}

size_t pg_synthesis_block_end(const struct pg_syntax *syntax)
{
	const struct pg_syntax *block = pg_synthesis_block_syntax(syntax);
	if (!block) return 0;
	if (syntax == block) return block->item_count;
	struct pg_token selected = syntax->right->token;
	for (size_t i = 0; i < block->item_count; ++i) {
		struct pg_token name = block->items[i].name;
		if (name.length && name.length == selected.length && !memcmp(name.text, selected.text, name.length)) return i + 1;
	}
	return 0;
}

struct pg_synthesis_job *pg_synthesis_source_block(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!synthesis || !scope || scope->owner != synthesis->owner_key || !pg_synthesis_block_syntax(syntax)) return NULL;
	return pg_synthesis_work_request(synthesis, SOURCE_BLOCK, 2, (const void *[]){scope, syntax});
}

const struct pg_source_scope *pg_synthesis_block_scope(const struct pg_synthesis_job *job, size_t index)
{
	const struct source_block_work *local = pg_synthesis_work_state(job, SOURCE_BLOCK);
	return local && local->scopes && index < local->end ? local->scopes[index] : NULL;
}

static void source_block_destroy(struct pg_synthesis_job *job)
{
	struct source_block_work *local = pg_synthesis_work_state(job, SOURCE_BLOCK);
	pg_index_destroy(&local->names);
}

static struct pg_synthesis_projection source_block_projection(const struct pg_synthesis_job *job)
{
	const struct source_block_work *local = pg_synthesis_work_state(job, SOURCE_BLOCK);
	struct pg_synthesis_job *rule = local->frames ? NULL : local->tail;
	return (struct pg_synthesis_projection){rule,
		job->status == PG_SYNTHESIS_PENDING && !rule, 0};
}

static struct pg_synthesis_input source_block_output(const struct pg_synthesis_job *job)
{
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(source_block_projection(job).rule)};
}

/* The compatibility policy quotes functions at value boundaries. Returning
 * computations keep their ordinary sequencing; no result value is assumed. */
int pg_synthesis_prepare_value_argument(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_input context, struct pg_synthesis_job **input)
{
	if (synthesis->definition_policy != PG_DEFINITION_IMPLICIT_THUNK) return 0;
	if (pg_synthesis_await_preparation(synthesis, job, *input, NULL)) return 1;
	struct pg_synthesis_job *normalized = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(*input)}).pending);
	struct pg_synthesis_structure shape = pg_synthesis_classifier_structure(synthesis, normalized);
	if (pg_synthesis_await_structure(synthesis, job, shape)) return 1;
	const struct pg_term *type = pg_synthesis_type_structure_result(shape), *domain, *body;
	const struct pg_object *binder;
	if (pg_pi_view(type, &domain, &binder, &body) && !pg_synthesis_logical_family_signature(type)) {
		*input = pg_synthesis_plain_rule_inputs(synthesis, PG_THUNK_INTRO, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(normalized)});
		if (!*input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	}
	return 0;
}

static void source_block_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct source_block_work *block = pg_synthesis_work_state(job, SOURCE_BLOCK);
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = pg_synthesis_block_syntax(job->inputs[1]);
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (!block->scope) {
		block->scope = scope;
		if (pg_index_init(&block->names) != 0) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		block->end = pg_synthesis_block_end(job->inputs[1]);
		if (!block->end) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (block->end > SIZE_MAX / sizeof(*block->scopes)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		block->scopes = pg_alloc(synthesis->typing->graph, block->end * sizeof(*block->scopes));
		if (!block->scopes) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (block->tail) {
		if (!block->frames) {
			pg_synthesis_forward(synthesis, job, block->tail);
			return;
		}
		const struct pg_synthesis_sequence_frame *frame = block->frames;
		struct pg_synthesis_input context = frame->context;
		if (!frame->binds) {
			if (pg_synthesis_await_preparation(synthesis, job, frame->input, NULL)) return;
			if (pg_synthesis_input_value_kind((struct pg_synthesis_input){.pending = pg_synthesis_pending(frame->input)}) != 0) {
				if (pg_synthesis_await(synthesis, job, frame->input)) return;
				const struct pg_evidence *proof = pg_synthesis_result(frame->input);
				if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE_TYPE) proof = pg_prove_type_value(synthesis->typing, proof);
				if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE) {
					block->frames = frame->parent;
					pg_synthesis_enqueue(synthesis, job);
					return;
				}
			}
			struct pg_synthesis_job *input = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(frame->input)}).pending);
			context = (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_result_context(synthesis,
				context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(input)}, pg_binder(synthesis->typing->graph)))};
			struct pg_synthesis_input premises[] = {context, {.pending = pg_synthesis_pending(block->tail)}};
			block->tail = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		}
		if (!context.pending || !block->tail) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(block->tail)});
		block->tail = pg_synthesis_sequence(synthesis, pg_synthesis_rule_input(synthesis, pg_pending_job(context.pending), 0),
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(frame->input)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(continuation)});
		if (!block->tail) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		block->frames = frame->parent;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	const struct pg_syntax_item *item = &syntax->items[block->next];
	block->scopes[block->next] = block->scope;
	struct pg_synthesis_job *input = pg_synthesis_request(synthesis, block->scope, item->expression);
	if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (item->annotation) {
		input = pg_synthesis_binding_expect(synthesis, block->scope, (struct pg_synthesis_input){.pending = pg_synthesis_pending(input)},
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_request(synthesis, block->scope, item->annotation))});
	}
	if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (item->name.length && block->next + 1 < block->end)
		if (pg_synthesis_prepare_value_argument(synthesis, job, block->scope->context, &input)) return;
	int name_status = pg_synthesis_register_name(synthesis, &block->names, item->name);
	if (name_status) { pg_synthesis_finish(synthesis, job, name_status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
	++block->next;
	struct pg_synthesis_job *normalized = NULL;
	if (block->next == block->end || item->name.length) {
		struct pg_synthesis_input body = pg_synthesis_body(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(input)}, (struct pg_synthesis_input){0});
		normalized = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, block->scope->context, body).pending);
		if (!normalized) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (block->next == block->end) block->tail = normalized;
	else {
		struct pg_synthesis_sequence_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
		if (!frame) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		*frame = (struct pg_synthesis_sequence_frame){input, block->scope->context, block->frames, item->name.length != 0};
		block->frames = frame;
		if (frame->binds) {
			const struct pg_object *binder = pg_synthesis_source_binder(synthesis,
				block->scope, syntax, block->next - 1, NULL);
			frame->context = (struct pg_synthesis_input){.pending =
				pg_synthesis_pending(pg_synthesis_result_context(synthesis, frame->context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(normalized)}, binder))};
			block->scope = pg_synthesis_bind(synthesis, block->scope, item->name, binder, frame->context);
			if (!block->scope) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
	}
	pg_synthesis_enqueue(synthesis, job);
}

/* Type positions may advance pure computations or use their checked total
 * result. Expected types never supply missing synthesis. */
const struct pg_evidence *pg_synthesis_type_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job **returned, const struct pg_evidence *context,
	const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) != PG_JUDGEMENT_COMPUTATION) return proof;
	if (!*returned) {
		*returned = pg_synthesis_return(synthesis, context, proof);
		pg_synthesis_subscribe(synthesis, job, *returned, 0);
		return NULL;
	}
	struct pg_synthesis_job *producer = *returned;
	if (producer->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, producer->status); return NULL; }
	*returned = NULL;
	return producer->result;
}

struct source_quote_work {
	struct pg_synthesis_job *operand, *rule;
};
static void source_quote_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection source_quote_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_input source_quote_output(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class SOURCE_QUOTE[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct source_quote_work),
	.source_key = 1, .advance = source_quote_step, .project = source_quote_projection,
	.output = source_quote_output}};

struct pg_synthesis_job *pg_synthesis_source_quote(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!synthesis || !scope || scope->owner != synthesis->owner_key || !syntax || syntax->kind != PG_SYNTAX_QUOTE) return NULL;
	return pg_synthesis_work_request(synthesis, SOURCE_QUOTE, 2, (const void *[]){scope, syntax});
}

struct pg_synthesis_job *pg_synthesis_quote_operand(const struct pg_synthesis_job *job)
{
	const struct source_quote_work *local = pg_synthesis_work_state(job, SOURCE_QUOTE);
	return local ? local->operand : NULL;
}

static struct pg_synthesis_projection source_quote_projection(const struct pg_synthesis_job *job)
{
	const struct source_quote_work *local = pg_synthesis_work_state(job, SOURCE_QUOTE);
	return (struct pg_synthesis_projection){local->rule,
		job->status == PG_SYNTHESIS_PENDING && !local->rule, 1};
}

static struct pg_synthesis_input source_quote_output(const struct pg_synthesis_job *job)
{
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(source_quote_projection(job).rule)};
}

static void source_quote_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct source_quote_work *local = pg_synthesis_work_state(job, SOURCE_QUOTE);
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = job->inputs[1];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (local->rule) { pg_synthesis_forward(synthesis, job, local->rule); return; }
	if (!local->operand) local->operand = pg_synthesis_request(synthesis, scope, syntax->left);
	if (!local->operand) goto error;
	if (pg_synthesis_await_preparation(synthesis, job, local->operand, NULL)) return;
	const struct pg_derivation_input *construction;
	int kind = pg_synthesis_input_kind((struct pg_synthesis_input){.pending = pg_synthesis_pending(local->operand)}, &construction);
	if (kind < 0 && local->operand->status == PG_SYNTHESIS_PENDING) {
		pg_synthesis_await(synthesis, job, local->operand);
		return;
	}
	struct pg_synthesis_job *operand = local->operand;
	if (kind == 1 && construction && construction->rule == PG_THUNK_INTRO) {
		/* Discovering U must not await its child. The actual producer still
		 * checks that child; this preserves open Match/effect equations. */
		local->rule = operand;
	} else if (kind == 1) {
		operand = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, scope->context,
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(operand)}).pending);
		struct pg_synthesis_structure shape = pg_synthesis_classifier_structure(synthesis, operand);
		if (!pg_synthesis_structure_valid(shape)) goto error;
		if (pg_synthesis_await_structure(synthesis, job, shape)) return;
		const struct pg_term *content;
		if (!pg_thunk_type_view(pg_synthesis_type_structure_result(shape), &content)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
			return;
		}
		local->rule = operand;
	} else {
		struct pg_synthesis_input value = {.pending = pg_synthesis_pending(operand)};
		const struct pg_evidence *proof = pg_synthesis_result(operand);
		if (proof && pg_evidence_judgement(proof) == PG_JUDGEMENT_TYPE_FAMILY)
			value = pg_synthesis_family_function(synthesis, (struct pg_synthesis_input){.checked = proof});
		local->rule = pg_synthesis_plain_rule_inputs(synthesis, PG_THUNK_INTRO, NULL, 1, &value);
	}
	if (!local->rule) goto error;
	pg_synthesis_forward(synthesis, job, local->rule);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* Raw descriptions close pending effect equations; only ordinary rule checking
 * produces Evidence. Both sequencing and handling use the same Fold graph. */
struct computation_structure_work {
	const struct pg_term *result;
	struct pg_synthesis_structure left, right, clause;
	size_t next;
	struct pg_comparison comparison;
};
static void computation_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void computation_structure_destroy(struct pg_synthesis_job *);
static void computation_structure_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static const struct pg_term *computation_structure_result(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class TERM_STRUCTURE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct computation_structure_work), .advance = computation_structure_step,
	.destroy = computation_structure_destroy, .completed = computation_structure_completed,
	.structure = computation_structure_result}};
static const struct pg_synthesis_work_class CLASSIFIER_STRUCTURE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct computation_structure_work), .advance = computation_structure_step,
	.destroy = computation_structure_destroy, .completed = computation_structure_completed,
	.structure = computation_structure_result}};

static struct computation_structure_work *computation_structure_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, pg_synthesis_work_role(job));
}

static void computation_structure_destroy(struct pg_synthesis_job *job)
{
	pg_comparison_destroy(&computation_structure_work(job)->comparison);
}

static void computation_structure_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int first)
{
	(void)synthesis; (void)first;
	computation_structure_destroy(job);
}

static const struct pg_term *computation_structure_result(const struct pg_synthesis_job *job)
{
	return computation_structure_work(job)->result;
}

int pg_synthesis_cbpv_type_rule(enum pg_evidence_rule rule)
{
	switch (rule) {
	case PG_RETURN_TYPE_FORM: case PG_THUNK_TYPE_FORM: case PG_RETURN_CONTENT: return 1;
	default: return 0;
	}
}

int pg_synthesis_cbpv_structure_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input dependency, int classifier, struct pg_synthesis_structure *result)
{
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	const struct pg_derivation_input *input = pg_synthesis_plain_derivation(producer);
	if (!input) return 0;
	if (input->rule == PG_EFFECT_SUBSUMPTION) {
		*result = classifier ? pg_synthesis_type_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, 1))
			: pg_synthesis_term_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, 0));
		return 1;
	}
	if (classifier && input->rule == PG_HANDLER_ELIM) {
		*result = pg_synthesis_type_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, 2));
		return 1;
	}
	switch (input->rule) {
	case PG_RETURN_INTRO: case PG_THUNK_INTRO: case PG_FORCE_ELIM:
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM:
		break;
	default:
		if (classifier || !pg_synthesis_cbpv_type_rule(input->rule)) return 0;
	}
	*result = pg_synthesis_structure_input(synthesis, dependency,
		classifier ? CLASSIFIER_STRUCTURE_JOB : TERM_STRUCTURE_JOB, classifier, 0);
	return 1;
}

struct compound_inputs {
	struct pg_synthesis *synthesis;
	struct pg_synthesis_job *producer;
	const struct pg_handler_signature *signature;
};

static struct pg_operation_clause compound_clause(const void *owner, size_t i)
{
	const struct compound_inputs *inputs = owner;
	struct pg_synthesis_structure body = pg_synthesis_term_structure_input(inputs->synthesis,
		pg_synthesis_rule_input(inputs->synthesis, inputs->producer, 5 + 3 * i));
	return (struct pg_operation_clause){pg_handler_signature_label(inputs->signature, i),
		pg_synthesis_type_structure_result(body)};
}

static void compound_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer, const struct pg_derivation_input *input)
{
	struct computation_structure_work *local = computation_structure_work(job);
	int handling = input->rule == PG_HANDLER_ELIM;
	size_t count = handling ? pg_handler_signature_count(input->parameters.handler) : 0;
	if (handling && (!count || count > (SIZE_MAX - 3) / 3 || input->count != 3 + 3 * count)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (!pg_synthesis_structure_valid(local->left)) {
		size_t offset = input->rule == PG_REQUEST_INTRO ? 2 : 0;
		local->left = pg_synthesis_term_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, offset));
		local->right = pg_synthesis_term_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, offset + 1));
	}
	struct pg_synthesis_structure parts[] = {local->left, local->right};
	for (size_t i = 0; i < 2; ++i) {
		if (!pg_synthesis_structure_valid(parts[i])) { pg_synthesis_finish(synthesis, job, handling ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_UNSUPPORTED); return; }
		if (pg_synthesis_await_structure(synthesis, job, parts[i])) return;
	}
	if (local->next < count) {
		if (!pg_synthesis_structure_valid(local->clause)) local->clause = pg_synthesis_term_structure_input(synthesis,
			pg_synthesis_rule_input(synthesis, producer, 5 + 3 * local->next));
		if (pg_synthesis_await_structure(synthesis, job, local->clause)) return;
		++local->next;
		local->clause = (struct pg_synthesis_structure){0};
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	const struct pg_term *left = pg_synthesis_type_structure_result(local->left);
	const struct pg_term *right = pg_synthesis_type_structure_result(local->right);
	if (input->rule == PG_REQUEST_INTRO)
		local->result = pg_computation_request(synthesis->typing->graph, input->parameters.operation_label, left, right);
	else {
		const struct compound_inputs inputs = {synthesis, producer, input->parameters.handler};
		local->result = pg_computation_fold_inputs(synthesis->typing->graph, left, right, count, &inputs, compound_clause);
	}
	pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE
		: handling ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_ERROR);
}

/* Zero leaves a nonconstant/non-F continuation to its ordinary producer.
 * The same budgeted scan serves both Fold and Request classifier queries. */
static int continuation_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_term *type, enum pg_totality totality, const struct pg_term *row)
{
	struct computation_structure_work *local = computation_structure_work(job);
	const struct pg_term *domain, *codomain, *following, *result;
	const struct pg_object *binder;
	if (!pg_pi_view(type, &domain, &binder, &codomain)) return 0;
	enum pg_comparison_status scan = pg_synthesis_independence(synthesis, job, &local->comparison, codomain, binder);
	if (scan == PG_COMPARISON_PENDING || scan == PG_COMPARISON_ERROR) return 1;
	if (scan != PG_COMPARISON_EQUAL) return 0;
	enum pg_totality next_totality;
	if (!pg_computation_type_spine_view(codomain, &next_totality, &following, &result)) return 0;
	if (next_totality < totality) totality = next_totality;
	local->result = pg_computation_type_spine(synthesis->typing->graph, totality,
		pg_effect_join_term(synthesis->typing->graph, row, following), result);
	if (!local->result) return 0;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return 1;
}

static void computation_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct computation_structure_work *local = computation_structure_work(job);
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	int classifier = pg_synthesis_work_role(job) == CLASSIFIER_STRUCTURE_JOB;
	const struct pg_evidence *proof = pg_synthesis_input_result(dependency);
	if (proof) {
		const struct pg_occurrence *subject = pg_evidence_subject(proof);
		local->result = !subject ? NULL : classifier ? subject->classifier : subject->core;
		pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	const struct pg_derivation_input *input = pg_synthesis_plain_derivation(producer);
	if (!input) goto unsupported;
	if (!classifier) switch (input->rule) {
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM:
		compound_structure_step(synthesis, job, producer, input); return;
	default: break;
	}
	if (!pg_synthesis_structure_valid(local->left)) {
		size_t ordinal = 0;
		int type = pg_synthesis_cbpv_type_rule(input->rule);
		if (classifier) switch (input->rule) {
		case PG_REQUEST_INTRO: ordinal = 3; break;
		default: break;
		}
		struct pg_synthesis_input premise = pg_synthesis_rule_input(synthesis, producer, ordinal);
		local->left = type ? pg_synthesis_type_structure_input(synthesis, premise)
			: classifier ? pg_synthesis_classifier_structure_input(synthesis, premise)
			: pg_synthesis_term_structure_input(synthesis, premise);
	}
	if (!pg_synthesis_structure_valid(local->left)) goto unsupported;
	if (pg_synthesis_await_structure(synthesis, job, local->left)) return;
	const struct pg_term *left = pg_synthesis_type_structure_result(local->left);
	const struct pg_term *row, *value;
	enum pg_totality totality;
	if (classifier) switch (input->rule) {
	case PG_REQUEST_INTRO: {
		const struct pg_object *label = input->parameters.operation_label;
		if (!label) goto accepted;
		const struct pg_effect_row *effects = pg_effect_row(synthesis->typing->graph, 1, &label);
		if (!continuation_structure_step(synthesis, job, left,
			PG_TOTALITY_TOTAL, pg_effect_reference(synthesis->typing->graph, effects))) goto accepted;
		return;
	}
	case PG_FOLD_ELIM:
		if (!pg_synthesis_structure_valid(local->right)) local->right = pg_synthesis_classifier_structure_input(synthesis,
			pg_synthesis_rule_input(synthesis, producer, 1));
		if (pg_synthesis_await_structure(synthesis, job, local->right)) return;
		if (!pg_computation_type_spine_view(left, &totality, &row, &value)) goto accepted;
		if (!continuation_structure_step(synthesis, job, pg_synthesis_type_structure_result(local->right), totality, row)) goto accepted;
		return;
	case PG_FORCE_ELIM:
		if (!pg_thunk_type_view(left, &local->result)) goto unsupported;
		break;
	case PG_THUNK_INTRO: local->result = pg_thunk_type(synthesis->typing->graph, left); break;
	case PG_RETURN_INTRO:
		local->result = pg_computation_type(synthesis->typing->graph, input->parameters.totality,
			pg_effect_row(synthesis->typing->graph, 0, NULL), left); break;
	default: goto unsupported;
	} else switch (input->rule) {
	case PG_RETURN_TYPE_FORM:
		if (producer->inputs[1]) {
			const struct pg_effect_inference *effects = producer->inputs[1];
			row = pg_reference(synthesis->typing->graph,
				pg_effect_equation_parameter(effects, producer->inputs[2]));
		} else row = pg_effect_reference(synthesis->typing->graph, input->parameters.effects);
		if (!row) goto unsupported;
		local->result = pg_computation_type_spine(synthesis->typing->graph, input->parameters.totality, row, left);
		break;
	case PG_THUNK_TYPE_FORM: local->result = pg_thunk_type(synthesis->typing->graph, left); break;
	case PG_RETURN_CONTENT:
		if (!pg_computation_type_spine_view(left, &totality, &row, &local->result)) goto unsupported;
		break;
	case PG_RETURN_INTRO: local->result = pg_application(synthesis->typing->graph, pg_reference(synthesis->typing->graph, &pg_return_operation), left); break;
	case PG_THUNK_INTRO: local->result = pg_application(synthesis->typing->graph, pg_reference(synthesis->typing->graph, &pg_thunk_operation), left); break;
	case PG_FORCE_ELIM: local->result = pg_application(synthesis->typing->graph, pg_reference(synthesis->typing->graph, &pg_force_operation), left); break;
	default: goto unsupported;
	}
	pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return;
accepted:
	if (producer->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, producer, 0); return; }
	pg_synthesis_finish(synthesis, job, producer->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : producer->status);
	return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

struct pg_synthesis_job *pg_synthesis_result_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_input context, struct pg_synthesis_input computation, const struct pg_object *binder)
{
	if (!pg_synthesis_input_owned(synthesis, context)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, computation) || !binder) return NULL;
	struct pg_pending *formation = pg_synthesis_classifier_in(synthesis, context, computation);
	if (!formation) return NULL;
	struct pg_synthesis_job *domain = pg_synthesis_plain_rule_inputs(synthesis, PG_RETURN_CONTENT, NULL, 1, &(struct pg_synthesis_input){.pending = formation});
	if (!domain) return NULL;
	struct pg_synthesis_input premises[] = {context, {.pending = pg_synthesis_pending(domain)}};
	struct pg_derivation_input input = {.rule = PG_CONTEXT_EXTEND, .parameters.binder = binder, .count = 2};
	return pg_synthesis_rule_inputs(synthesis, &input, premises, NULL, NULL);
}

struct contents_work {
	struct pg_synthesis_job *dependency;
	unsigned stage;
};
struct body_work {
	struct pg_synthesis_input rule;
};
struct sequence_work {
	struct pg_synthesis_job *fold, *rule;
	struct pg_comparison comparison;
	unsigned stage;
};

static void contents_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void body_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_input body_output(const struct pg_synthesis_job *);
static struct pg_synthesis_input body_rule(struct pg_synthesis *, struct pg_synthesis_input, int);
static void sequence_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void sequence_destroy(struct pg_synthesis_job *);
static void sequence_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static struct pg_synthesis_projection body_project(const struct pg_synthesis_job *);
static struct pg_synthesis_projection sequence_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class RETURN_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct contents_work), .advance = contents_step}};
static const struct pg_synthesis_work_class THUNK_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct contents_work), .advance = contents_step}};
static const struct pg_synthesis_work_class BODY_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct body_work), .advance = body_step, .project = body_project,
	.output = body_output}};
static const struct pg_synthesis_work_class SEQUENCE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct sequence_work), .advance = sequence_step, .project = sequence_project,
	.destroy = sequence_destroy, .completed = sequence_completed, .output = pg_synthesis_projected_output}};

static struct body_work *body_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, BODY_JOB);
}

static struct pg_synthesis_input body_output(const struct pg_synthesis_job *job)
{
	return body_work(job)->rule;
}

static struct sequence_work *sequence_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, SEQUENCE_JOB);
}

static struct pg_synthesis_projection body_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_input rule = body_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = pg_pending_job(rule.pending),
		.preparing = !rule.checked && !rule.pending, .value_kind = 0};
}

static struct pg_synthesis_projection sequence_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = sequence_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = rule, .preparing = !rule, .value_kind = 0};
}

static void sequence_destroy(struct pg_synthesis_job *job)
{
	pg_comparison_destroy(&sequence_work(job)->comparison);
}

static void sequence_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int first)
{
	(void)synthesis; (void)first;
	sequence_destroy(job);
}

struct pg_synthesis_input pg_synthesis_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input, struct pg_synthesis_input context)
{
	if (!pg_synthesis_input_owned(synthesis, input)) return (struct pg_synthesis_input){0};
	if (context.checked || context.pending)
		if (!pg_synthesis_input_owned(synthesis, context)) return (struct pg_synthesis_input){0};
	const void *inputs[] = {input.checked, input.pending, context.checked, context.pending};
	struct pg_synthesis_job *existing = pg_synthesis_work_find(synthesis, BODY_JOB, 4, inputs);
	if (existing) return (struct pg_synthesis_input){.pending = pg_synthesis_pending(existing)};
	int ready = !context.checked && !context.pending;
	if (!ready) ready = pg_synthesis_typed_input(synthesis,
		pg_synthesis_input_result(context), pg_synthesis_input_result(input));
	int kind = pg_synthesis_input_value_kind(input);
	if (ready && kind >= 0)
		return body_rule(synthesis, input, kind);
	return (struct pg_synthesis_input){.pending =
		pg_synthesis_pending(pg_synthesis_work_request(synthesis, BODY_JOB, 4, inputs))};
}

struct pg_synthesis_input pg_synthesis_body_input(const struct pg_synthesis_job *job)
{
	return body_work(job) ? pg_synthesis_work_dependency(job, 0) : (struct pg_synthesis_input){0};
}

static struct pg_synthesis_job *request_evaluation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof, const struct pg_synthesis_work_class *role)
{
	if (!proof) return NULL;
	enum pg_evidence_judgement judgement = role == THUNK_JOB ? PG_JUDGEMENT_VALUE : PG_JUDGEMENT_COMPUTATION;
	if (pg_evidence_judgement(proof) != judgement) return NULL;
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
	const void *inputs[] = {context, proof};
	return pg_synthesis_work_request(synthesis, role, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_return(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *computation)
{
	return request_evaluation(synthesis, context, computation, RETURN_JOB);
}

struct pg_synthesis_job *pg_synthesis_unthunk(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *value)
{
	return request_evaluation(synthesis, context, value, THUNK_JOB);
}

static const struct pg_evidence *contents(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *proof)
{
	return pg_synthesis_work_role(job) == THUNK_JOB ? pg_prove_thunk_computation(synthesis->typing, proof)
		: pg_prove_return_value(synthesis->typing, proof);
}

static void contents_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct contents_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (!local->dependency) {
		local->dependency = pg_synthesis_normalize(synthesis, job->inputs[0], job->inputs[1]);
		pg_synthesis_subscribe(synthesis, job, local->dependency, 0);
		return;
	}
	if (local->dependency->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->dependency->status); return; }
	if (!local->stage) {
		const struct pg_evidence *term = local->dependency->result;
		job->result = contents(synthesis, job, term);
		if (job->result) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE); return; }
		local->dependency = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, (struct pg_synthesis_input){.checked = job->inputs[0]}, (struct pg_synthesis_input){.checked = term}).pending);
		local->stage = 1;
		pg_synthesis_subscribe(synthesis, job, local->dependency, 0);
		return;
	}
	job->result = contents(synthesis, job, local->dependency->result);
	if (!job->result && pg_synthesis_work_role(job) == RETURN_JOB) {
		job->result = pg_prove_total_pure_value(synthesis->typing, local->dependency->result);
	}
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static struct pg_synthesis_input body_rule(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input, int kind)
{
	if (kind == 2) input = (struct pg_synthesis_input){.pending =
		pg_synthesis_pending(pg_synthesis_plain_rule_inputs(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &input))};
	if (kind > 0) {
		struct pg_derivation_input returned = {.rule = PG_RETURN_INTRO,
			.parameters.totality = PG_TOTALITY_TOTAL, .count = 1};
		input = (struct pg_synthesis_input){.pending =
			pg_synthesis_pending(pg_synthesis_rule_inputs(synthesis, &returned, &input, NULL, NULL))};
	}
	return input;
}

int pg_synthesis_body_resume(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return -1;
	struct body_work *local = body_work(job);
	if (!local || local->rule.checked || local->rule.pending || job->status != PG_SYNTHESIS_PENDING) return -1;
	struct pg_synthesis_input input = pg_synthesis_body_input(job);
	if (pg_synthesis_work_project(pg_pending_job(input.pending)).preparing) return -1;
	int kind = pg_synthesis_input_value_kind(input);
	if (kind < 0) return -1;
	local->rule = body_rule(synthesis, input, kind);
	return local->rule.checked || local->rule.pending ? 0 : -1;
}

static void body_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct body_work *local = body_work(job);
	struct pg_synthesis_input input = pg_synthesis_body_input(job);
	if (!local->rule.checked && !local->rule.pending) {
		if (input.pending && pg_synthesis_await_preparation(synthesis, job, pg_pending_job(input.pending), NULL)) return;
		int kind = pg_synthesis_input_value_kind(input);
		if (kind < 0) {
			if (pg_synthesis_await_input(synthesis, job, input)) return;
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
			return;
		}
		local->rule = body_rule(synthesis, input, kind);
		if (!local->rule.checked && !local->rule.pending) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	struct pg_synthesis_input context = pg_synthesis_work_dependency(job, 2);
	if (context.checked || context.pending) {
		if (pg_synthesis_await_input(synthesis, job, input)) return;
		if (pg_synthesis_await_input(synthesis, job, context)) return;
		const struct pg_evidence *proof = pg_synthesis_input_result(input);
		const struct pg_evidence *scope = pg_synthesis_input_result(context);
		if (!scope || pg_evidence_judgement(scope) != PG_JUDGEMENT_CONTEXT) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (!proof || pg_evidence_context(proof) != pg_evidence_context(scope)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
	}
	/* Context validation belongs here. The adapted rule owns checking, its
	 * result and its pending status; discovery must not mirror them. */
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
}

struct pg_synthesis_job *pg_synthesis_sequence(struct pg_synthesis *synthesis,
	struct pg_synthesis_input context, struct pg_synthesis_input input, struct pg_synthesis_input continuation)
{
	if (!pg_synthesis_input_owned(synthesis, context)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, input)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, continuation)) return NULL;
	const void *inputs[] = {context.checked, context.pending, input.checked, input.pending,
		continuation.checked, continuation.pending};
	return pg_synthesis_work_request(synthesis, SEQUENCE_JOB, 6, inputs);
}

/* 1 selects FOLD; 0 suspends; -1 leaves conversion/finite-return checking to
 * the ordinary producers. Completed negative choices are not rescanned. */
static int fixed_sequence_fold(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_term *input, const struct pg_term *continuation)
{
	struct sequence_work *local = sequence_work(job);
	const struct pg_term *row, *value, *domain, *codomain, *following, *result;
	const struct pg_object *binder;
	enum pg_totality first, next;
	if (local->stage == 2) return -1;
	if (!pg_computation_type_spine_view(input, &first, &row, &value)) goto not_fixed;
	if (!pg_pi_view(continuation, &domain, &binder, &codomain)) goto not_fixed;
	struct pg_comparison *work = &local->comparison;
	if (!local->stage && domain == value) local->stage = 1;
	if (!local->stage) {
		if (!work->state && pg_comparison_init(work, domain, value, NULL, NULL)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 0;
		}
		enum pg_comparison_status scan = pg_synthesis_probe_advance(synthesis, job, &local->comparison);
		if (scan == PG_COMPARISON_PENDING || scan == PG_COMPARISON_ERROR) return 0;
		if (scan != PG_COMPARISON_EQUAL) goto not_fixed;
		pg_comparison_destroy(work);
		local->stage = 1;
		pg_synthesis_enqueue(synthesis, job); return 0;
	}
	enum pg_comparison_status scan = pg_synthesis_independence(synthesis, job, &local->comparison, codomain, binder);
	if (scan == PG_COMPARISON_PENDING || scan == PG_COMPARISON_ERROR) return 0;
	pg_comparison_destroy(work);
	if (scan != PG_COMPARISON_EQUAL) goto not_fixed;
	if (pg_computation_type_spine_view(codomain, &next, &following, &result)) return 1;
	const struct pg_effect_row *closed = pg_effect_row_view(row);
	if (first == PG_TOTALITY_TOTAL && closed && !pg_effect_count(closed)) return 1;
not_fixed:
	pg_comparison_destroy(&local->comparison);
	local->stage = 2;
	return -1;
}

static void sequence_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct sequence_work *local = sequence_work(job);
	struct pg_synthesis_input scope = pg_synthesis_work_dependency(job, 0);
	if (!local->rule && !local->fold) {
		struct pg_synthesis_input argument = pg_synthesis_work_dependency(job, 2);
		if (argument.pending && pg_synthesis_await_preparation(synthesis, job, pg_pending_job(argument.pending), NULL)) return;
		int kind = pg_synthesis_input_value_kind(argument);
		if (kind < 0) {
			if (pg_synthesis_await_input(synthesis, job, argument)) return;
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
			return;
		}
		if (kind > 0) {
			if (kind == 2) argument = (struct pg_synthesis_input){.pending =
				pg_synthesis_pending(pg_synthesis_plain_rule_inputs(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &argument))};
			local->rule = pg_synthesis_application(synthesis, scope, pg_synthesis_work_dependency(job, 4), argument);
			if (!local->rule) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		} else {
			struct pg_synthesis_input normalized = pg_synthesis_normalize_classifier(synthesis, scope, argument);
			if (!normalized.checked && !normalized.pending) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_synthesis_input premises[] = {normalized, pg_synthesis_work_dependency(job, 4)};
			local->fold = pg_synthesis_plain_rule_inputs(synthesis, PG_FOLD_ELIM, NULL, 2, premises);
			if (!local->fold) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
	}
	if (local->rule && local->rule != local->fold) { pg_synthesis_forward(synthesis, job, local->rule); return; }
	struct pg_synthesis_input normalized = pg_synthesis_rule_input(synthesis, local->fold, 0);
	/* A checked Fold supersedes provisional choice, not the Context checks below. */
	if (!local->rule && pg_synthesis_result(local->fold)) local->rule = local->fold;
	if (!local->rule) {
		struct pg_synthesis_structure shapes[] = {
			pg_synthesis_classifier_structure_input(synthesis, normalized),
			pg_synthesis_classifier_structure_input(synthesis, pg_synthesis_work_dependency(job, 4))};
		for (size_t i = 0; i < 2; ++i) {
			if (!pg_synthesis_structure_valid(shapes[i])) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (pg_synthesis_structure_status(shapes[i]) == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, shapes[i].pending, 0); return; }
			if (pg_synthesis_structure_status(shapes[i]) == PG_SYNTHESIS_ERROR) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		int choice = fixed_sequence_fold(synthesis, job,
			pg_synthesis_type_structure_result(shapes[0]), pg_synthesis_type_structure_result(shapes[1]));
		if (!choice) return;
		if (choice > 0) local->rule = local->fold;
	}
	if (pg_synthesis_await_input(synthesis, job, scope)) return;
	if (pg_synthesis_await_input(synthesis, job, pg_synthesis_work_dependency(job, 2))) return;
	if (pg_synthesis_await_input(synthesis, job, pg_synthesis_work_dependency(job, 4))) return;
	const struct pg_evidence *context = pg_synthesis_input_result(scope);
	const struct pg_evidence *input = pg_synthesis_input_result(pg_synthesis_work_dependency(job, 2));
	const struct pg_evidence *continuation = pg_synthesis_input_result(pg_synthesis_work_dependency(job, 4));
	if (!context || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT || !input || !continuation) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (pg_evidence_context(input) != pg_evidence_context(context) ||
		pg_evidence_context(continuation) != pg_evidence_context(context)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (pg_evidence_judgement(input) != PG_JUDGEMENT_COMPUTATION) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (pg_synthesis_await_input(synthesis, job, normalized)) return;
	input = pg_synthesis_input_result(normalized);
	if (local->fold->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, local->fold, 0); return; }
	if (local->rule || local->fold->status != PG_SYNTHESIS_REJECTED) {
		local->rule = local->fold;
		pg_synthesis_forward(synthesis, job, local->rule); return;
	}
	const struct pg_term *content, *domain, *body;
	const struct pg_object *binder;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	const struct pg_term *codomain = pg_pi_constant_codomain(pg_evidence_classifier(continuation));
	if (pg_computation_type_view(pg_evidence_classifier(input), &totality, &effects, &content) &&
		pg_effect_count(effects) && pg_pi_view(codomain, &domain, &binder, &body)) {
		/* Preserve the effectful prefix outside the returned function.
		 * The source adapter uses only checked APP/THUNK/RETURN/FOLD. */
		binder = pg_binder(synthesis->typing->graph);
		struct pg_synthesis_job *extended = pg_synthesis_result_context(synthesis, scope, normalized, binder);
		struct pg_synthesis_job *variable = pg_synthesis_plain_rule_inputs(synthesis, PG_VARIABLE, binder, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(extended)});
		struct pg_synthesis_input premises[] = {{.pending = pg_synthesis_pending(extended)}, pg_synthesis_work_dependency(job, 4)};
		struct pg_synthesis_job *projected = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		struct pg_synthesis_job *applied = pg_synthesis_application(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(extended)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(projected)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(variable)});
		struct pg_synthesis_job *quoted = pg_synthesis_plain_rule_inputs(synthesis, PG_THUNK_INTRO, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(applied)});
		premises[0] = normalized;
		premises[1] = (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_lambda_body(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(extended)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(quoted)}))};
		local->rule = pg_synthesis_plain_rule_inputs(synthesis, PG_FOLD_ELIM, NULL, 2, premises);
		pg_synthesis_forward(synthesis, job, local->rule);
		return;
	}
	struct pg_synthesis_job *argument = pg_synthesis_return(synthesis, context, input);
	local->rule = pg_synthesis_application(synthesis, (struct pg_synthesis_input){.checked = context}, pg_synthesis_work_dependency(job, 4), (struct pg_synthesis_input){.pending = pg_synthesis_pending(argument)});
	pg_synthesis_forward(synthesis, job, local->rule);
}
