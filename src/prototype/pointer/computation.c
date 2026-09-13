#include "computation_internal.h"
#include "identity.h"
#include "iadt.h"
#include "symmetry.h"
#include <stdlib.h>
#include <string.h>

static const struct pg_object_class return_class = {"return"};
static const struct pg_object_class total_result_class = {"total-result"};
static const struct pg_object_class thunk_class = {"thunk"};
static const struct pg_object_class force_class = {"force"};
static const struct pg_object_class fold_class = {"computation-fold"};
static const struct pg_object_class request_class = {"operation-request"};
static const struct pg_object_class handler_class = {"computation-fold-clauses"};
const struct pg_object pg_return_operation = {PG_SEMANTIC_OBJECT, &return_class};
const struct pg_object pg_total_result_operation = {PG_SEMANTIC_OBJECT, &total_result_class};
const struct pg_object pg_thunk_operation = {PG_SEMANTIC_OBJECT, &thunk_class};
const struct pg_object pg_force_operation = {PG_SEMANTIC_OBJECT, &force_class};
const struct pg_object pg_fold_operation = {PG_SEMANTIC_OBJECT, &fold_class};
const struct pg_object pg_request_operation = {PG_SEMANTIC_OBJECT, &request_class};

struct handler_entry {
	struct pg_object_entry base;
	size_t count;
	struct pg_clause_position *positions;
};

static const struct handler_entry *handler_owner(const struct pg_object *object)
{
	if (!object || object->owner != &handler_class) return NULL;
	return (const struct handler_entry *)((const char *)object - offsetof(struct pg_object_entry, object));
}

static int compare_labels(const void *left, const void *right)
{
	uintptr_t a = (uintptr_t)((const struct pg_clause_position *)left)->label;
	uintptr_t b = (uintptr_t)((const struct pg_clause_position *)right)->label;
	return a < b ? -1 : a != b;
}

static const struct pg_object *handler(struct pg_graph *graph,
	size_t count, const struct pg_operation_clause *clauses)
{
	if (!count) return &pg_fold_operation;
	if (count > SIZE_MAX / sizeof(struct pg_clause_position)) return NULL;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = UINT64_C(1469598103934665603) ^ count;
	for (size_t i = 0; i < count; ++i) {
		if (!clauses[i].label || clauses[i].label->kind != PG_SEMANTIC_OBJECT) return NULL;
		hash = (hash ^ (uintptr_t)clauses[i].label) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		const struct pg_object_entry *base = (const struct pg_object_entry *)p;
		const struct handler_entry *entry = handler_owner(&base->object);
		if (!entry || entry->count != count) continue;
		size_t i = 0;
		while (i < count && entry->positions[i].label == clauses[entry->positions[i].position].label) ++i;
		if (i == count) return &base->object;
	}
	struct pg_graph temporary = {0};
	struct pg_clause_position *positions = pg_alloc(&temporary, count * sizeof(*positions));
	const struct pg_object *result = NULL;
	if (!positions) goto done;
	for (size_t i = 0; i < count; ++i) positions[i] = (struct pg_clause_position){clauses[i].label, i};
	qsort(positions, count, sizeof(*positions), compare_labels);
	for (size_t i = 1; i < count; ++i) if (positions[i - 1].label == positions[i].label) goto done;
	struct handler_entry *entry = pg_alloc(graph, sizeof(*entry));
	if (!entry) goto done;
	entry->positions = pg_alloc(graph, count * sizeof(*positions));
	if (!entry->positions) goto done;
	entry->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &handler_class};
	entry->count = count;
	memcpy(entry->positions, positions, count * sizeof(*positions));
	if (!pg_index_insert(&graph->objects, &entry->base.index, hash)) result = &entry->base.object;
done:
	pg_graph_destroy(&temporary);
	return result;
}

int pg_computation_handler_view(const struct pg_object *object,
	size_t *count, const struct pg_clause_position **positions)
{
	const struct handler_entry *entry = handler_owner(object);
	if (!entry) return 0;
	*count = entry->count;
	*positions = entry->positions;
	return 1;
}

const struct pg_object *pg_computation_handler_restore(struct pg_graph *graph,
	size_t count, const struct pg_clause_position *positions)
{
	if (!graph || !count || !positions || count > SIZE_MAX / sizeof(struct pg_operation_clause)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_operation_clause *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
	const struct pg_object *result = NULL;
	if (!clauses) goto done;
	for (size_t i = 0; i < count; ++i) {
		size_t position = positions[i].position;
		if (position >= count || !positions[i].label || clauses[position].label) goto done;
		clauses[position].label = positions[i].label;
	}
	result = handler(graph, count, clauses);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_term *pg_computation_fold(struct pg_graph *graph,
	const struct pg_term *source, const struct pg_term *returned,
	size_t count, const struct pg_operation_clause *clauses)
{
	if (!graph || !source || !returned || (count && !clauses)) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_clause_position) - 2) return NULL;
	for (size_t i = 0; i < count; ++i) if (!clauses[i].body) return NULL;
	const struct pg_object *object = handler(graph, count, clauses);
	if (!object) return NULL;
	const struct pg_term *term = pg_application(graph, pg_reference(graph, object), source);
	term = pg_application(graph, term, returned);
	for (size_t i = 0; i < count; ++i) term = pg_application(graph, term, clauses[i].body);
	return term;
}

static size_t clause_index(const struct handler_entry *handler, const struct pg_object *label)
{
	if (!handler) return 0;
	size_t low = 0, high = handler->count;
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		const struct pg_clause_position *entry = &handler->positions[middle];
		if (entry->label == label) return entry->position;
		if ((uintptr_t)entry->label < (uintptr_t)label) low = middle + 1;
		else high = middle;
	}
	return handler->count;
}

static const struct {
	const struct pg_object *object;
	const char *name;
} descriptors[] = {
	{&pg_return_operation, "kernel/return/v1"},
	{&pg_total_result_operation, "kernel/total-result/v1"},
	{&pg_thunk_operation, "kernel/thunk/v1"},
	{&pg_force_operation, "kernel/force/v1"},
	{&pg_fold_operation, "kernel/fold/v1"},
	{&pg_request_operation, "kernel/request/v1"}
};

const char *pg_computation_name(const struct pg_object *object)
{
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
		if (object == descriptors[i].object) return descriptors[i].name;
	return NULL;
}

const struct pg_object *pg_computation_resolve(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
		if (!strcmp(name, descriptors[i].name)) return descriptors[i].object;
	return NULL;
}

const struct pg_term *pg_computation_request(struct pg_graph *graph,
	const struct pg_object *label, const struct pg_term *payload,
	const struct pg_term *continuation)
{
	if (!graph || !label || label->kind != PG_SEMANTIC_OBJECT) return NULL;
	if (!payload || !continuation) return NULL;
	const struct pg_term *term = pg_reference(graph, &pg_request_operation);
	term = pg_application(graph, term, pg_reference(graph, label));
	term = pg_application(graph, term, payload);
	return pg_application(graph, term, continuation);
}

int pg_computation_request_view(const struct pg_term *term,
	const struct pg_object **label, const struct pg_term **payload,
	const struct pg_term **continuation)
{
	if (!term || !label || !payload || !continuation) return 0;
	const struct pg_term *arguments[3];
	for (size_t i = 3; i; --i) {
		if (term->kind != PG_APPLICATION) return 0;
		arguments[i - 1] = term->as.application.argument;
		term = term->as.application.function;
	}
	if (term->kind != PG_REFERENCE || term->as.reference != &pg_request_operation) return 0;
	if (arguments[0]->kind != PG_REFERENCE) return 0;
	if (arguments[0]->as.reference->kind != PG_SEMANTIC_OBJECT) return 0;
	*label = arguments[0]->as.reference;
	*payload = arguments[1];
	*continuation = arguments[2];
	return 1;
}

static const struct pg_term *unary_argument(const struct pg_term *term, const struct pg_object *operation)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = term->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != operation) return NULL;
	return term->as.application.argument;
}

static int return_continuation(const struct pg_term *term)
{
	if (term->kind != PG_LAMBDA) return 0;
	const struct pg_term *body = unary_argument(term->as.lambda.body, &pg_return_operation);
	return body && body->kind == PG_REFERENCE && body->as.reference == term->as.lambda.binder;
}

static const struct pg_term *fold_unit_source(const struct pg_term *term)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *body = unary_argument(term->as.application.function, &pg_fold_operation);
	return body && return_continuation(term->as.application.argument) ? body : NULL;
}

const struct pg_term *pg_computation_eta(struct pg_graph *graph, const struct pg_term *term)
{
	if (!term) return NULL;
	const struct pg_term *body = unary_argument(term, &pg_thunk_operation);
	if (!body) return fold_unit_source(term);
	const struct pg_term *value = unary_argument(body, &pg_force_operation);
	if (value) return value;
	const struct pg_term *source = fold_unit_source(body);
	return source ? pg_application(graph, term->as.application.function, source) : NULL;
}

static int force_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused);
static const struct pg_eval_continuation force_answer_continuation = {
	"computation/force_answer/v1", force_answer
};

static int force_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_term *source;
	if (pg_identity_action_view(answer, &source)) {
		/* Keep reflexivity outside a neutral observation, so pre-normalizing a
		 * callee cannot hide the diagonal application rule behind FORCE. */
		const struct pg_term *observed = pg_application(machine->output,
			pg_reference(machine->output, &pg_force_operation), source);
		return pg_eval_enter(machine, (struct pg_closure){pg_identity_action(machine->output, observed), NULL}, 1);
	}
	const struct pg_term *body = unary_argument(answer, &pg_thunk_operation);
	return body ? pg_eval_enter(machine, (struct pg_closure){body, NULL}, 1) : pg_identity_force(machine, answer);
}

static int fold_poll(void *state)
{
	struct fold_work *work = state;
	struct pg_graph *graph = work->graph;
	switch (work->phase) {
	case FOLD_BINDERS:
		if (work->position < work->count + 2) {
			const struct pg_object *binder = pg_binder(graph);
			if (!binder) return -1;
			work->binders[work->position++] = binder;
			return 0;
		}
		work->x = pg_binder(graph);
		work->next = pg_application(graph, work->resume, pg_reference(graph, work->x));
		work->next = pg_application(graph, work->head, work->next);
		work->position = 1;
		work->phase = FOLD_ARGUMENTS;
		break;
	case FOLD_ARGUMENTS:
		if (work->position < work->count + 2) {
			work->next = pg_application(graph, work->next, pg_reference(graph, work->binders[work->position++]));
			break;
		}
		work->phase = FOLD_CLAUSE;
		break;
	case FOLD_CLAUSE:
		work->next = pg_lambda(graph, work->x, work->next);
		if (work->index == work->count)
			work->next = pg_computation_request(graph, work->label, work->payload, work->next);
		else {
			work->next = pg_application(graph, pg_reference(graph, &pg_thunk_operation), work->next);
			const struct pg_term *clause = pg_application(graph,
				pg_reference(graph, work->binders[work->index + 2]), work->payload);
			work->next = pg_application(graph, clause, work->next);
		}
		work->position = work->count + 2;
		work->phase = FOLD_ABSTRACT;
		break;
	case FOLD_ABSTRACT:
		if (!work->position) return 1;
		work->next = pg_lambda(graph, work->binders[--work->position], work->next);
		break;
	}
	return work->next ? 0 : -1;
}

static int fold_resume(struct pg_eval *machine, void *state)
{
	struct fold_work *work = state;
	return pg_eval_enter(machine, (struct pg_closure){work->next, NULL}, 0);
}

static void fold_destroy(void *state)
{
	(void)state; /* The evaluator's temporary arena owns the work and binder array. */
}

const struct pg_eval_work_operation pg_fold_work_operation = {
	fold_poll, fold_resume, fold_destroy, "computation/fold_work/v1"
};

static int fold_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state);
static int total_result_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	const struct pg_term *value = unary_argument(answer, &pg_return_operation);
	if (value) return pg_eval_enter(machine, (struct pg_closure){value, NULL}, 1);
	if (answer->kind != PG_APPLICATION) return 1;
	const struct pg_term *source = unary_argument(answer->as.application.function, &pg_fold_operation);
	if (!source) return 1;
	/* q(Fold(M,K)) = q(K(q(M))) belongs to the TOTAL/empty-row result
	 * projection, not to ordinary Fold. Typing checks q's domain. */
	struct pg_graph *graph = machine->output;
	const struct pg_term *projection = pg_reference(graph, &pg_total_result_operation);
	const struct pg_term *argument = pg_application(graph, projection, source);
	const struct pg_term *call = pg_application(graph, answer->as.application.argument, argument);
	const struct pg_term *result = pg_application(graph, projection, call);
	return result ? pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1) : -1;
}

static const struct pg_eval_continuation total_result_continuation = {
	"computation/total_result/v1", total_result_answer
};

static const struct pg_eval_continuation fold_answer_continuation = {
	"computation/fold_answer/v2", fold_answer
};

static int associate_fold(struct pg_eval *machine, const struct pg_term *inner)
{
	if (inner->kind != PG_APPLICATION) return 1;
	const struct pg_term *source = unary_argument(inner->as.application.function, &pg_fold_operation);
	if (!source) return 1;
	struct pg_graph *graph = machine->output;
	const struct pg_object *ignored = pg_binder(graph), *outer = pg_binder(graph), *x = pg_binder(graph);
	if (!ignored || !outer || !x) return -1;
	const struct pg_term *next = pg_application(graph, inner->as.application.argument, pg_reference(graph, x));
	next = pg_computation_fold(graph, next, pg_reference(graph, outer), 0, NULL);
	next = pg_computation_fold(graph, source, pg_lambda(graph, x, next), 0, NULL);
	/* Reuse the caller's two argument closures. The already-demanded source
	 * is discarded, while the outer continuation keeps its original environment. */
	next = pg_lambda(graph, ignored, pg_lambda(graph, outer, next));
	return next ? pg_eval_enter(machine, (struct pg_closure){next, NULL}, 0) : -1;
}

static int fold_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	/* Demand restores this exact caller before delivering the answer. */
	const struct handler_entry *handler = handler_owner(machine->current.term->as.reference);
	size_t count = handler ? handler->count : 0;
	const struct pg_term *value = unary_argument(answer, &pg_return_operation);
	struct pg_closure continuation = *pg_eval_argument(machine, 1);
	if (value) return pg_eval_apply(machine, continuation, (struct pg_closure){value, NULL}, count + 2);
	const struct pg_object *label;
	const struct pg_term *payload, *resume;
	if (!pg_computation_request_view(answer, &label, &payload, &resume))
		return handler ? 1 : associate_fold(machine, answer);
	/* Bind the existing argument closures without demanding them. Only k is
	 * recursively handled; a selected clause runs outside this handler. */
	struct fold_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	*work = (struct fold_work){.graph = machine->output, .head = machine->current.term,
		.resume = resume, .payload = payload, .label = label,
		.count = count, .index = clause_index(handler, label), .phase = FOLD_BINDERS};
	work->binders = pg_alloc(&machine->temporary, (count + 2) * sizeof(*work->binders));
	if (!work->binders) return -1;
	return pg_eval_defer(machine, &pg_fold_work_operation, work);
}

const struct pg_eval_work_operation *pg_computation_work_resolve(const char *name)
{
	static const struct pg_eval_work_operation *const entries[] = {&pg_fold_work_operation};
	const struct pg_eval_work_operation *found = pg_eval_work_find(name, 1, entries);
	if (!found) found = pg_identity_work_resolve(name);
	return found ? found : pg_symmetry_work_resolve(name);
}

const struct pg_eval_continuation *pg_computation_continuation_resolve(const char *name)
{
	static const struct pg_eval_continuation *const entries[] = {
		&force_answer_continuation, &fold_answer_continuation, &total_result_continuation
	};
	const struct pg_eval_continuation *found = pg_eval_continuation_find(name, sizeof(entries) / sizeof(*entries), entries);
	if (!found) found = pg_data_continuation_resolve(name);
	if (!found) found = pg_identity_continuation_resolve(name);
	return found ? found : pg_symmetry_continuation_resolve(name);
}

static int dispatch(struct pg_eval *machine)
{
	const struct pg_object *operation = machine->current.term->as.reference;
	if (operation == &pg_total_result_operation) {
		if (!pg_eval_argument(machine, 0)) return 1;
		return pg_eval_demand(machine, 0, &total_result_continuation, NULL);
	}
	if (operation == &pg_thunk_operation) {
		const struct pg_closure *body = pg_eval_argument(machine, 0);
		if (!body) return 1;
		const struct pg_term *value = unary_argument(body->term, &pg_force_operation);
		if (value) return pg_eval_enter(machine, (struct pg_closure){value, body->environment}, 1);
		/* Strip only an administrative right unit under suspension, not an
		 * arbitrary computation, so THUNK(FOLD(FORCE(v), return)) contracts too. */
		value = pg_computation_eta(machine->output, body->term);
		if (!value) return 1;
		return pg_eval_apply(machine, machine->current, (struct pg_closure){value, body->environment}, 1);
	}
	if (operation == &pg_force_operation) {
		if (!pg_eval_argument(machine, 0)) return 1;
		return pg_eval_demand(machine, 0, &force_answer_continuation, NULL);
	}
	const struct handler_entry *handler = handler_owner(operation);
	if (operation == &pg_fold_operation || handler) {
		if (handler && !pg_eval_argument(machine, handler->count + 1)) return 1;
		const struct pg_closure *continuation = pg_eval_argument(machine, 1);
		if (!continuation) return 1;
		/* Recognize the right unit without evaluating a continuation that M
		 * might never invoke. The returned reference must be this lambda's binder. */
		if (!handler && return_continuation(continuation->term)) return pg_eval_enter(machine, *pg_eval_argument(machine, 0), 2);
		return pg_eval_demand(machine, 0, &fold_answer_continuation, NULL);
	}
	int data = pg_data_dispatch(machine);
	if (data != 1) return data;
	int identity = pg_identity_dispatch(machine);
	return identity == 1 ? pg_symmetry_dispatch(machine) : identity;
}

const struct pg_eval_policy pg_pure_policy = {dispatch};

static const struct {
	const struct pg_eval_policy *policy;
	const char *name;
} portable_policies[] = {
	{&pg_beta_policy, "evaluation/beta/v1"},
	{&pg_pure_policy, "evaluation/pure/v4"}
};

const char *pg_computation_policy_name(const struct pg_eval_policy *policy)
{
	for (size_t i = 0; i < sizeof(portable_policies) / sizeof(*portable_policies); ++i)
		if (portable_policies[i].policy == policy) return portable_policies[i].name;
	return NULL;
}

const struct pg_eval_policy *pg_computation_policy_resolve(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < sizeof(portable_policies) / sizeof(*portable_policies); ++i)
		if (!strcmp(portable_policies[i].name, name)) return portable_policies[i].policy;
	return NULL;
}

void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term)
{
	pg_eval_init(machine, term);
	machine->output = output;
	machine->dispatch = pg_pure_policy.dispatch;
}
