#include "synthesis_source.h"
#include "computation.h"
#include "derivation.h"
#include "synthesis_effect.h"
#include "effect_inference.h"

/* Raw descriptions close pending effect equations; only ordinary rule checking
 * produces Evidence. Both sequencing and handling use the same Fold graph. */
struct fold_structure_state {
	size_t next;
	struct pg_operation_clause clauses[];
};
struct computation_structure_work {
	const struct pg_term *result;
	struct pg_synthesis_job *left, *right, *clause;
	struct fold_structure_state *fold;
	struct pg_comparison comparison;
};
static void computation_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void computation_structure_destroy(struct pg_synthesis_job *);
static void computation_structure_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static const struct pg_term *computation_structure_result(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class TERM_STRUCTURE_JOB[1] = {{
	.size = sizeof(struct computation_structure_work), .advance = computation_structure_step,
	.destroy = computation_structure_destroy, .completed = computation_structure_completed,
	.structure = computation_structure_result}};
static const struct pg_synthesis_work_class CLASSIFIER_STRUCTURE_JOB[1] = {{
	.size = sizeof(struct computation_structure_work), .advance = computation_structure_step,
	.destroy = computation_structure_destroy, .completed = computation_structure_completed,
	.structure = computation_structure_result}};

static struct computation_structure_work *computation_structure_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, job->role);
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

const struct pg_synthesis_work_class *pg_synthesis_cbpv_structure_class(enum pg_evidence_rule rule, int classifier)
{
	switch (rule) {
	case PG_RETURN_INTRO: case PG_THUNK_INTRO: case PG_FORCE_ELIM:
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM: case PG_EFFECT_SUBSUMPTION:
		return classifier ? CLASSIFIER_STRUCTURE_JOB : TERM_STRUCTURE_JOB;
	default:
		return !classifier && pg_synthesis_cbpv_type_rule(rule) ? TERM_STRUCTURE_JOB : NULL;
	}
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
	if (count && !local->fold) {
		if (count > (SIZE_MAX - sizeof(*local->fold)) / sizeof(struct pg_operation_clause)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		local->fold = pg_alloc(synthesis->typing->graph,
			sizeof(*local->fold) + count * sizeof(struct pg_operation_clause));
		if (!local->fold) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (!local->left) {
		size_t offset = input->rule == PG_REQUEST_INTRO ? 2 : 0;
		local->left = pg_synthesis_term_structure(synthesis, pg_synthesis_rule_premise(synthesis, producer, offset));
		local->right = pg_synthesis_term_structure(synthesis, pg_synthesis_rule_premise(synthesis, producer, offset + 1));
	}
	struct pg_synthesis_job *parts[] = {local->left, local->right};
	for (size_t i = 0; i < 2; ++i) {
		if (!parts[i]) { pg_synthesis_finish(synthesis, job, handling ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_UNSUPPORTED); return; }
		if (pg_synthesis_await(synthesis, job, parts[i])) return;
	}
	struct fold_structure_state *state = local->fold;
	if (count && state->next < count) {
		if (!local->clause) local->clause = pg_synthesis_term_structure(synthesis,
			pg_synthesis_rule_premise(synthesis, producer, 5 + 3 * state->next));
		if (pg_synthesis_await(synthesis, job, local->clause)) return;
		state->clauses[state->next] = (struct pg_operation_clause){
			pg_handler_signature_label(input->parameters.handler, state->next),
			pg_synthesis_type_structure_result(local->clause)};
		++state->next;
		local->clause = NULL;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	const struct pg_term *left = pg_synthesis_type_structure_result(local->left);
	const struct pg_term *right = pg_synthesis_type_structure_result(local->right);
	if (input->rule == PG_REQUEST_INTRO)
		local->result = pg_computation_request(synthesis->typing->graph, input->parameters.operation_label, left, right);
	else local->result = pg_computation_fold(synthesis->typing->graph, left, right, count, count ? state->clauses : NULL);
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
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	int classifier = job->role == CLASSIFIER_STRUCTURE_JOB;
	const struct pg_evidence *proof = pg_synthesis_result(producer);
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
	if (!local->left) {
		size_t ordinal = 0;
		int type = pg_synthesis_cbpv_type_rule(input->rule);
		if (classifier) switch (input->rule) {
		case PG_REQUEST_INTRO: ordinal = 3; break;
		case PG_HANDLER_ELIM: ordinal = 2; type = 1; break;
		case PG_EFFECT_SUBSUMPTION: ordinal = 1; type = 1; break;
		default: break;
		}
		struct pg_synthesis_job *premise = pg_synthesis_rule_premise(synthesis, producer, ordinal);
		local->left = type ? pg_synthesis_type_structure(synthesis, premise)
			: classifier ? pg_synthesis_classifier_structure(synthesis, premise)
			: pg_synthesis_term_structure(synthesis, premise);
	}
	if (!local->left) goto unsupported;
	if (pg_synthesis_await(synthesis, job, local->left)) return;
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
		if (!local->right) local->right = pg_synthesis_classifier_structure(synthesis,
			pg_synthesis_rule_premise(synthesis, producer, 1));
		if (pg_synthesis_await(synthesis, job, local->right)) return;
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
	case PG_HANDLER_ELIM: case PG_EFFECT_SUBSUMPTION: local->result = left; break;
	default: goto unsupported;
	} else switch (input->rule) {
	case PG_RETURN_TYPE_FORM:
		if (producer->inputs[1]) {
			const struct pg_synthesis_job *effects = producer->inputs[1];
			row = pg_reference(synthesis->typing->graph,
				pg_effect_equation_parameter(pg_synthesis_effect_worker(effects), producer->inputs[2]));
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
	case PG_EFFECT_SUBSUMPTION: local->result = left; break;
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
	struct pg_synthesis_job *context, struct pg_synthesis_job *computation,
	const struct pg_object *binder)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!computation || computation->owner != synthesis->owner_key || !binder) return NULL;
	struct pg_synthesis_job *formation = pg_synthesis_classifier_formation(synthesis, context, computation);
	if (!formation) return NULL;
	struct pg_synthesis_job *domain = pg_synthesis_plain_rule(synthesis, PG_RETURN_CONTENT, NULL, 1, &formation);
	if (!domain) return NULL;
	struct pg_synthesis_job *premises[] = {context, domain};
	return pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, binder, 2, premises);
}

struct pg_synthesis_job *pg_synthesis_result_context_input(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *context, const struct pg_object *binder)
{
	const struct pg_derivation_input *extension = pg_synthesis_plain_derivation(context);
	if (!extension || extension->rule != PG_CONTEXT_EXTEND || extension->parameters.binder != binder) return NULL;
	struct pg_synthesis_job *domain = pg_synthesis_rule_premise(synthesis, context, 1);
	const struct pg_derivation_input *content = pg_synthesis_plain_derivation(domain);
	if (!content || content->rule != PG_RETURN_CONTENT) return NULL;
	return pg_synthesis_classifier_formation_input(pg_synthesis_rule_premise(synthesis, domain, 0));
}

struct contents_work {
	struct pg_synthesis_job *dependency;
	unsigned stage;
};
struct body_work {
	struct pg_synthesis_job *rule;
};
struct sequence_work {
	struct pg_synthesis_job *normalized, *fold, *rule;
	struct pg_comparison comparison;
	unsigned stage;
};

static void contents_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void body_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void sequence_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void sequence_destroy(struct pg_synthesis_job *);
static void sequence_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static struct pg_synthesis_projection body_project(const struct pg_synthesis_job *);
static struct pg_synthesis_projection sequence_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class RETURN_JOB[1] = {{
	.size = sizeof(struct contents_work), .advance = contents_step}};
static const struct pg_synthesis_work_class THUNK_JOB[1] = {{
	.size = sizeof(struct contents_work), .advance = contents_step}};
static const struct pg_synthesis_work_class BODY_JOB[1] = {{
	.size = sizeof(struct body_work), .advance = body_step, .project = body_project}};
static const struct pg_synthesis_work_class SEQUENCE_JOB[1] = {{
	.size = sizeof(struct sequence_work), .advance = sequence_step, .project = sequence_project,
	.destroy = sequence_destroy, .completed = sequence_completed}};

static struct body_work *body_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, BODY_JOB);
}

static struct sequence_work *sequence_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, SEQUENCE_JOB);
}

static struct pg_synthesis_projection body_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = body_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = rule, .preparing = !rule, .value_kind = 0};
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

struct pg_synthesis_job *pg_synthesis_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, struct pg_synthesis_job *context)
{
	const void *inputs[] = {input, context};
	return pg_synthesis_work_request(synthesis, BODY_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_body_input(const struct pg_synthesis_job *job)
{
	return body_work(job) ? (void *)job->inputs[0] : NULL;
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
	return job->role == THUNK_JOB ? pg_prove_thunk_computation(synthesis->typing, proof)
		: pg_prove_return_value(synthesis->typing, proof);
}

static void contents_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct contents_work *local = pg_synthesis_work_state(job, job->role);
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
		local->dependency = pg_synthesis_normalize_classifier(synthesis, job->inputs[0], term);
		local->stage = 1;
		pg_synthesis_subscribe(synthesis, job, local->dependency, 0);
		return;
	}
	job->result = contents(synthesis, job, local->dependency->result);
	if (!job->result && job->role == RETURN_JOB) {
		job->result = pg_prove_total_pure_value(synthesis->typing, local->dependency->result);
	}
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void body_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct body_work *local = body_work(job);
	struct pg_synthesis_job *input = (void *)job->inputs[0];
	if (!local->rule) {
		if (pg_synthesis_await_preparation(synthesis, job, input, NULL)) return;
		int kind = pg_synthesis_source_value_kind(input);
		if (kind < 0) {
			if (input->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, input, 0); return; }
			pg_synthesis_finish(synthesis, job, input->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : input->status);
			return;
		}
		struct pg_synthesis_job *body = input;
		if (kind == 2) body = pg_synthesis_plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &body);
		if (kind > 0) {
			struct pg_derivation_input returned = {.rule = PG_RETURN_INTRO,
				.parameters.totality = PG_TOTALITY_TOTAL, .count = 1};
			body = pg_synthesis_rule(synthesis, &returned, &body, NULL, NULL);
		}
		if (!body) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		local->rule = body;
	}
	struct pg_synthesis_job *context = (void *)job->inputs[1];
	if (context) {
		struct pg_synthesis_job *premises[] = {input, context};
		for (size_t i = 0; i < 2; ++i) {
			if (pg_synthesis_await(synthesis, job, premises[i])) return;
		}
		if (!input->result || !context->result || pg_evidence_context(input->result) != pg_evidence_context(context->result)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
	}
	pg_synthesis_forward(synthesis, job, local->rule);
}

struct pg_synthesis_job *pg_synthesis_sequence(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *input,
	struct pg_synthesis_job *continuation)
{
	struct pg_synthesis_job *producers[] = {context, input, continuation};
	for (size_t i = 0; i < 3; ++i)
		if (!producers[i] || producers[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, input, continuation};
	return pg_synthesis_work_request(synthesis, SEQUENCE_JOB, 3, inputs);
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
	if (!local->rule && !local->fold) {
		struct pg_synthesis_job *argument = (void *)job->inputs[1];
		if (pg_synthesis_await_preparation(synthesis, job, argument, NULL)) return;
		int kind = pg_synthesis_source_value_kind(argument);
		if (kind < 0) {
			if (argument->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, argument, 0); return; }
			pg_synthesis_finish(synthesis, job, argument->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : argument->status);
			return;
		}
		if (kind > 0) {
			if (kind == 2) argument = pg_synthesis_plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &argument);
			local->rule = pg_synthesis_application_jobs(synthesis, (void *)job->inputs[0],
				(void *)job->inputs[2], argument);
			if (!local->rule) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		} else {
			local->normalized = pg_synthesis_normalize_classifier_jobs(synthesis, (void *)job->inputs[0], argument);
			if (!local->normalized) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_synthesis_job *premises[] = {local->normalized, (void *)job->inputs[2]};
			local->fold = pg_synthesis_plain_rule(synthesis, PG_FOLD_ELIM, NULL, 2, premises);
			if (!local->fold) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
	}
	if (local->rule && local->rule != local->fold) { pg_synthesis_forward(synthesis, job, local->rule); return; }
	/* A checked Fold supersedes provisional choice, not the Context checks below. */
	if (!local->rule && pg_synthesis_result(local->fold)) local->rule = local->fold;
	if (!local->rule) {
		struct pg_synthesis_job *shapes[] = {
			pg_synthesis_classifier_structure(synthesis, local->normalized),
			pg_synthesis_classifier_structure(synthesis, (void *)job->inputs[2])};
		for (size_t i = 0; i < 2; ++i) {
			if (!shapes[i]) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (shapes[i]->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, shapes[i], 0); return; }
			if (shapes[i]->status == PG_SYNTHESIS_ERROR) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		int choice = fixed_sequence_fold(synthesis, job,
			pg_synthesis_type_structure_result(shapes[0]), pg_synthesis_type_structure_result(shapes[1]));
		if (!choice) return;
		if (choice > 0) local->rule = local->fold;
	}
	for (size_t i = 0; i < 3; ++i) {
		struct pg_synthesis_job *input = (void *)job->inputs[i];
		if (pg_synthesis_await(synthesis, job, input)) return;
	}
	const struct pg_evidence *context = ((const struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_evidence *input = ((const struct pg_synthesis_job *)job->inputs[1])->result;
	const struct pg_evidence *continuation = ((const struct pg_synthesis_job *)job->inputs[2])->result;
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
	if (pg_synthesis_await(synthesis, job, local->normalized)) return;
	input = local->normalized->result;
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
		struct pg_synthesis_job *extended = pg_synthesis_result_context(synthesis,
			(void *)job->inputs[0], local->normalized, binder);
		struct pg_synthesis_job *variable = pg_synthesis_plain_rule(synthesis, PG_VARIABLE, binder, 1, &extended);
		struct pg_synthesis_job *premises[] = {extended, (void *)job->inputs[2]};
		struct pg_synthesis_job *projected = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		struct pg_synthesis_job *applied = pg_synthesis_application_jobs(synthesis, extended, projected, variable);
		struct pg_synthesis_job *quoted = pg_synthesis_plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &applied);
		premises[0] = local->normalized;
		premises[1] = pg_synthesis_lambda_body(synthesis, extended, quoted);
		local->rule = pg_synthesis_plain_rule(synthesis, PG_FOLD_ELIM, NULL, 2, premises);
		pg_synthesis_forward(synthesis, job, local->rule);
		return;
	}
	struct pg_synthesis_job *argument = pg_synthesis_return(synthesis, context, input);
	local->rule = pg_synthesis_application(synthesis, context,
		(void *)job->inputs[2], argument);
	pg_synthesis_forward(synthesis, job, local->rule);
}
