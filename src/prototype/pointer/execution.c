#include "execution.h"
#include "computation.h"
#include "host.h"

#include <string.h>

static int returned(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	struct pg_execution *execution = (struct pg_execution *)machine;
	execution->result = answer;
	execution->status = PG_EXECUTION_DONE;
	return 1;
}

static int printed(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	struct pg_execution *execution = (struct pg_execution *)machine;
	const struct pg_object *type;
	const unsigned char *bytes;
	size_t count;
	if (answer->kind != PG_REFERENCE) goto stuck;
	if (!pg_host_literal_view(answer->as.reference, &type, &count, &bytes)) goto stuck;
	if (type != pg_host_type("Text")) goto stuck;
	if (fwrite(bytes, 1, count, execution->output) != count || fflush(execution->output)) {
		execution->status = PG_EXECUTION_IO_ERROR;
		return 1;
	}
	return pg_eval_apply(machine, *pg_eval_argument(machine, 2),
		(struct pg_closure){answer, NULL}, 3);
stuck:
	execution->status = PG_EXECUTION_STUCK;
	return 1;
}

static const struct pg_eval_continuation print_response = {NULL, printed};

static int requested(struct pg_eval *machine, const struct pg_term *label, const void *state)
{
	(void)state;
	struct pg_execution *execution = (struct pg_execution *)machine;
	if (label->kind == PG_REFERENCE && label->as.reference == pg_host_print(machine->output))
		return pg_eval_demand(machine, 1, &print_response, NULL);
	execution->status = PG_EXECUTION_UNHANDLED;
	return 1;
}

static const struct pg_eval_continuation return_response = {NULL, returned};
static const struct pg_eval_continuation request_label = {NULL, requested};

int pg_execution_init(struct pg_execution *execution, struct pg_typing *typing,
	const struct pg_evidence *computation, FILE *output)
{
	memset(execution, 0, sizeof(*execution));
	execution->status = PG_EXECUTION_ERROR;
	if (!typing || !output || !pg_evidence_owned_by(computation, typing)) return -1;
	if (pg_evidence_context(computation)) return -1;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return -1;
	enum pg_totality totality;
	const struct pg_effect_row *effects;
	const struct pg_term *content;
	if (!pg_computation_type_view(pg_evidence_classifier(computation), &totality, &effects, &content)) return -1;
	pg_computation_eval_init(&execution->machine, typing->graph, pg_evidence_subject(computation)->core);
	execution->output = output;
	execution->status = PG_EXECUTION_PENDING;
	return 0;
}

static int dispatch_root(struct pg_execution *execution)
{
	struct pg_eval *machine = &execution->machine;
	const struct pg_term *head = machine->current.term;
	if (head->kind != PG_REFERENCE) goto stuck;
	const struct pg_object *operation = head->as.reference;
	if (operation == &pg_return_operation && pg_eval_argument(machine, 0) && !pg_eval_argument(machine, 1))
		return pg_eval_demand(machine, 0, &return_response, NULL);
	if (operation == &pg_request_operation && pg_eval_argument(machine, 2) && !pg_eval_argument(machine, 3))
		return pg_eval_demand(machine, 0, &request_label, NULL);
stuck:
	execution->status = PG_EXECUTION_STUCK;
	return 1;
}

enum pg_execution_status pg_execution_advance(struct pg_execution *execution, uint64_t budget)
{
	while (execution->status == PG_EXECUTION_PENDING && budget--) {
		++execution->steps;
		struct pg_eval *machine = &execution->machine;
		if (machine->status == PG_EVAL_WHNF) {
			/* Only the unhandled root reaches here. Dispatching during a pure
			 * demand would steal requests from the enclosing user handler. */
			machine->status = PG_EVAL_PENDING;
			if (dispatch_root(execution) < 0) machine->status = PG_EVAL_ERROR;
		} else pg_eval_advance(machine, 1);
		if (machine->status == PG_EVAL_ERROR) execution->status = PG_EXECUTION_ERROR;
	}
	return execution->status;
}

void pg_execution_destroy(struct pg_execution *execution)
{
	pg_eval_destroy(&execution->machine);
}
