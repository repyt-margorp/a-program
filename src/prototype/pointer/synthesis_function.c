#include "synthesis_source.h"

struct pi_scope_work {
	const struct pg_object *binder;
	struct pg_synthesis_job *rule;
};

static void classifier_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void pi_scope_start(struct pg_synthesis *, struct pg_synthesis_job *);
static void pi_scope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection pi_scope_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class CLASSIFIER_FORMATION_JOB[1] = {{
	.advance = classifier_step}};
static const struct pg_synthesis_work_class PI_SCOPE_JOB[1] = {{
	.size = sizeof(struct pi_scope_work), .start = pi_scope_start,
	.advance = pi_scope_step, .project = pi_scope_project}};

struct pg_synthesis_job *pg_synthesis_classifier_formation(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *context, const struct pg_synthesis_job *term)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!term || term->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, term};
	return pg_synthesis_work_request(synthesis, CLASSIFIER_FORMATION_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_classifier_formation_input(const struct pg_synthesis_job *job)
{
	return job && job->role == CLASSIFIER_FORMATION_JOB ? (void *)job->inputs[1] : NULL;
}

/* Only the pending recipe projects the operand's classifier. After acceptance,
 * the ordinary type-structure query reads the retained formation itself. */
int pg_synthesis_classifier_formation_structure(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_synthesis_job **structure)
{
	struct pg_synthesis_job *term = pg_synthesis_classifier_formation_input(job);
	if (!term) return 0;
	*structure = pg_synthesis_classifier_structure(synthesis, term);
	return 1;
}

static void classifier_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	for (size_t i = 0; i < 2; ++i)
		if (pg_synthesis_await(synthesis, job, (void *)job->inputs[i])) return;
	const struct pg_synthesis_job *context = job->inputs[0], *body = job->inputs[1];
	struct pg_typed_query *classifier = pg_classifier_request(synthesis->typing, context->result, body->result);
	int status = pg_typed_query_advance(classifier, 1);
	if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
	job->result = pg_typed_query_result(classifier);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static struct pi_scope_work *pi_scope_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, PI_SCOPE_JOB);
}

const struct pg_object *pg_synthesis_pi_scope_binder(const struct pg_synthesis_job *job)
{
	const struct pi_scope_work *local = pi_scope_work(job);
	return local ? local->binder : NULL;
}

static void pi_scope_start(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	pi_scope_work(job)->binder = pg_binder(synthesis->typing->graph);
	if (!pi_scope_work(job)->binder) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	pg_synthesis_enqueue(synthesis, job);
}

static struct pg_synthesis_job *pi_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *type)
{
	const void *inputs[] = {context, type};
	return pg_synthesis_work_request(synthesis, PI_SCOPE_JOB, 2, inputs);
}

static void pi_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pi_scope_work *local = pi_scope_work(job);
	if (!local->rule) {
		struct pg_synthesis_job *type = (void *)job->inputs[1];
		struct pg_synthesis_job *premises[] = {(void *)job->inputs[0],
			pg_synthesis_plain_rule(synthesis, PG_PI_DOMAIN, NULL, 1, &type)};
		local->rule = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, local->binder, 2, premises);
	}
	pg_synthesis_forward(synthesis, job, local->rule);
}

static struct pg_synthesis_projection pi_scope_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = pi_scope_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = rule, .preparing = !rule, .value_kind = -1};
}

struct pg_synthesis_job *pg_synthesis_constant_result(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *callable, size_t parameters)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!callable || callable->owner != synthesis->owner_key) return NULL;
	struct pg_synthesis_job *result = pg_synthesis_classifier_formation(synthesis, context, callable);
	if (parameters > SIZE_MAX / sizeof(struct pg_synthesis_job *)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **scopes = pg_alloc(&temporary, parameters * sizeof(*scopes));
	if (parameters && !scopes) { pg_graph_destroy(&temporary); return NULL; }
	/* Open the whole dependent telescope before discharging it in reverse.
	 * A scope request owns its binder even while its domain is still pending. */
	for (size_t i = 0; result && i < parameters; ++i) {
		struct pg_synthesis_job *scope = pi_scope(synthesis, context, result);
		if (!scope) { result = NULL; break; }
		if (!pg_synthesis_pi_scope_binder(scope)) { result = NULL; break; }
		scopes[i] = scope;
		struct pg_synthesis_job *premises[] = {scope, result};
		premises[0] = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		premises[1] = pg_synthesis_plain_rule(synthesis, PG_VARIABLE, pg_synthesis_pi_scope_binder(scope), 1, &scope);
		result = pg_synthesis_plain_rule(synthesis, PG_PI_CODOMAIN, NULL, 2, premises);
		context = scope;
	}
	for (size_t i = parameters; result && i; --i) {
		struct pg_synthesis_job *premises[] = {scopes[i - 1], result};
		result = pg_synthesis_plain_rule(synthesis, PG_PI_FORM, NULL, 2, premises);
		result = pg_synthesis_plain_rule(synthesis, PG_PI_CONSTANT_CODOMAIN, NULL, 1, &result);
	}
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_synthesis_job *pg_synthesis_application(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	return pg_synthesis_application_jobs(synthesis, pg_synthesis_evidence(synthesis, context), function, argument);
}

struct pg_synthesis_job *pg_synthesis_application_domain(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *callee)
{
	struct pg_synthesis_job *formation = pg_synthesis_classifier_formation(synthesis, context, callee);
	if (!formation) return NULL;
	return pg_synthesis_plain_rule(synthesis, PG_PI_DOMAIN, NULL, 1, &formation);
}

struct pg_synthesis_job *pg_synthesis_application_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *function, struct pg_synthesis_job *argument)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!function || function->owner != synthesis->owner_key) return NULL;
	if (!argument || argument->owner != synthesis->owner_key) return NULL;
	struct pg_synthesis_job *callee = pg_synthesis_normalize_classifier_jobs(synthesis, context, function);
	if (!callee) return NULL;
	struct pg_synthesis_job *domain = pg_synthesis_application_domain(synthesis, context, callee);
	struct pg_synthesis_job *checked = pg_synthesis_expect(synthesis, argument, domain);
	if (!checked) return NULL;
	struct pg_synthesis_job *premises[] = {callee, checked};
	return pg_synthesis_plain_rule(synthesis, PG_APP_ELIM, NULL, 2, premises);
}

struct pg_synthesis_job *pg_synthesis_lambda_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context,
	struct pg_synthesis_job *body)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!body || body->owner != synthesis->owner_key) return NULL;
	body = pg_synthesis_body(synthesis, body, context);
	if (!body) return NULL;
	struct pg_synthesis_job *codomain = pg_synthesis_classifier_formation(synthesis, context, body);
	if (!codomain) return NULL;
	struct pg_synthesis_job *premises[] = {context, codomain};
	struct pg_synthesis_job *pi = pg_synthesis_plain_rule(synthesis, PG_PI_FORM, NULL, 2, premises);
	if (!pi) return NULL;
	struct pg_synthesis_job *lambda_premises[] = {pi, body};
	return pg_synthesis_plain_rule(synthesis, PG_LAMBDA_INTRO, NULL, 2, lambda_premises);
}
