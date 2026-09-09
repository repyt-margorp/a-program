#include "computation_io.h"
#include "computation.h"
#include "computation_internal.h"
#include "identity_internal.h"
#include "symmetry_internal.h"
#include "eval_internal.h"
#include "wire.h"

struct machine_owner {
	const struct pg_graph_codec *codec;
	void *owner;
	struct pg_eval *machine;
	const struct pg_eval *input;
	const struct pg_eval_work_operation *operation;
	void *state;
	size_t limit, name_limit, count, scope_count;
	const struct pg_eval_configuration *roots;
	const struct action_scope *const *scope_inputs;
	struct action_scope *const *scopes;
	int framed;
};

static int closure_task(const struct pg_eval_work_operation *operation)
{
	return operation == &pg_symmetry_prefix_operation || operation == &pg_action_scope_operation;
}

static int scopes_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct machine_owner *o = opaque;
	return pg_action_scopes_write(file, o->scope_count, o->scope_inputs, count, roots, o->codec, o->owner);
}

static int scopes_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct machine_owner *o = opaque;
	return pg_action_scopes_read(file, &o->machine->temporary, graph, limit, name_limit,
		o->codec, o->owner, &o->scope_count, &o->scopes, count, roots);
}

static int task_write(FILE *file, size_t n, const struct action_scope *const *scopes,
	size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct machine_owner *o = opaque;
	const struct pg_eval_work_operation *op = o->operation;
	o->scope_count = n;
	o->scope_inputs = scopes;
	if (!op || closure_task(op)) return scopes_write(file, count, roots, o);
	if (op == &pg_fold_work_operation)
		return pg_fold_work_write_with(file, o->state, count, roots, scopes_write, o);
	if (op == &pg_symmetry_composition_operation)
		return pg_symmetry_work_write_with(file, o->state, count, roots, scopes_write, o);
	if (op == &pg_action_result_operation)
		return pg_action_ownership_write(file, n, scopes, o->state, count, roots, o->codec, o->owner);
#define WRITE_TASK(operation, function) \
	if (op == &(operation)) return function(file, o->state, n, scopes, count, roots, o->codec, o->owner)
	WRITE_TASK(pg_scope_operation, pg_scope_work_write);
	WRITE_TASK(pg_action_body_operation, pg_action_body_write);
	WRITE_TASK(pg_higher_scope_operation, pg_higher_scope_write);
	WRITE_TASK(pg_force_family_scope_operation, pg_family_scope_write);
	WRITE_TASK(pg_field_family_scope_operation, pg_family_scope_write);
	WRITE_TASK(pg_force_family_result_operation, pg_family_result_write);
	WRITE_TASK(pg_field_family_result_operation, pg_family_result_write);
#undef WRITE_TASK
	return -1;
}

static int task_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *n, struct action_scope *const **scopes, size_t *count,
	const struct pg_term *const **roots, void *opaque)
{
	struct machine_owner *o = opaque;
	struct pg_graph *arena = &o->machine->temporary;
	const struct pg_eval_work_operation *op = o->operation;
	int status = -1;
	if (!op || closure_task(op)) status = scopes_read(file, graph, limit, name_limit, count, roots, o);
	else if (op == &pg_fold_work_operation) {
		struct fold_work *work = NULL;
		status = pg_fold_work_read_with(file, arena, graph, limit, name_limit, scopes_read, o, &work, count, roots);
		o->state = work;
	} else if (op == &pg_symmetry_composition_operation) {
		struct composition_work *work = NULL;
		status = pg_symmetry_work_read_with(file, arena, graph, limit, name_limit, scopes_read, o, &work, count, roots);
		o->state = work;
	} else if (op == &pg_action_result_operation) {
		struct action_result_work *work = NULL;
		status = pg_action_ownership_read(file, arena, graph, limit, name_limit, o->codec, o->owner,
			&o->scope_count, &o->scopes, &work, count, roots);
		o->state = work;
		if (!work) status = -1;
	}
#define READ_TASK(operation, type, function) \
	else if (op == &(operation)) { \
		struct type *work = NULL; \
		status = function(file, arena, graph, limit, name_limit, o->codec, o->owner, \
			&work, &o->scope_count, &o->scopes, count, roots); \
		o->state = work; \
	}
	READ_TASK(pg_scope_operation, scope_work, pg_scope_work_read)
	READ_TASK(pg_action_body_operation, action_body_work, pg_action_body_read)
	READ_TASK(pg_higher_scope_operation, higher_scope_work, pg_higher_scope_read)
	READ_TASK(pg_force_family_scope_operation, family_scope_work, pg_family_scope_read)
	READ_TASK(pg_field_family_scope_operation, family_scope_work, pg_family_scope_read)
	READ_TASK(pg_force_family_result_operation, family_result_work, pg_family_result_read)
	READ_TASK(pg_field_family_result_operation, family_result_work, pg_family_result_read)
#undef READ_TASK
	*n = o->scope_count;
	*scopes = o->scopes;
	return status;
}

static int terms_write(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	return task_write(file, 0, NULL, count, roots, opaque);
}

static int terms_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	size_t n;
	struct action_scope *const *scopes;
	int status = task_read(file, graph, limit, name_limit, &n, &scopes, count, roots, opaque);
	return status || n ? -1 : 0;
}

static int configurations_write(FILE *file, size_t count, const struct pg_eval_configuration *roots, void *opaque)
{
	struct machine_owner *o = opaque;
	if (pg_wire_write_u64(file, count)) return -1;
	if (!o->framed) return pg_eval_configurations_write_with(file, count, roots, terms_write, o);
	struct pg_eval_configuration current = {o->input->current, o->input->arguments};
	return pg_computation_frames_write_with(file, o->input->frames, &current, count, roots, task_write, o);
}

static int configurations_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_eval_configuration **roots, void *opaque)
{
	struct machine_owner *o = opaque;
	uint64_t n;
	if (pg_wire_read_u64(file, &n) || n > limit || n > SIZE_MAX) return -1;
	if (!o->framed) {
		if (pg_eval_configurations_read_with(file, graph, limit, name_limit, count, roots, terms_read, o)) return -1;
		return *count == n ? 0 : -1;
	}
	struct pg_eval_configuration current;
	if (pg_computation_frames_read_with(file, &o->machine->temporary, graph, limit, name_limit,
		task_read, o, &o->machine->frames, &current, (size_t)n, roots)) return -1;
	o->machine->current = current.head;
	o->machine->arguments = current.arguments;
	*count = (size_t)n;
	return 0;
}

static int payload_write(FILE *file, const struct pg_eval *machine, void *opaque)
{
	struct machine_owner *o = opaque;
	struct pg_graph temporary = {0};
	if (o->count >= SIZE_MAX / sizeof(struct pg_eval_configuration)) return -1;
	struct pg_eval_configuration *all = pg_alloc(&temporary, (o->count + 1) * sizeof(*all));
	if (!all) {
		pg_graph_destroy(&temporary);
		return -1;
	}
	all[0] = (struct pg_eval_configuration){machine->current, machine->arguments};
	for (size_t i = 0; i < o->count; ++i) all[i + 1] = o->roots[i];
	o->input = machine;
	o->framed = machine->frames != NULL;
	o->operation = machine->task ? machine->task->operation : NULL;
	o->state = machine->task ? machine->task->state : NULL;
	int status;
	if (o->operation == &pg_symmetry_prefix_operation)
		status = pg_symmetry_prefix_write_with(file, o->state, o->count + 1, all, configurations_write, o);
	else if (o->operation == &pg_action_scope_operation)
		status = pg_action_scope_work_write_with(file, o->state, o->count + 1, all, configurations_write, o);
	else status = configurations_write(file, o->count + 1, all, o);
	pg_graph_destroy(&temporary);
	return status;
}

static int payload_read(FILE *file, struct pg_eval *machine,
	const struct pg_eval_work_operation *operation, int framed, void *opaque)
{
	struct machine_owner *o = opaque;
	o->machine = machine;
	o->operation = operation;
	o->framed = framed;
	size_t count;
	const struct pg_eval_configuration *roots;
	int status;
	if (operation == &pg_symmetry_prefix_operation) {
		struct prefix_work *work = NULL;
		status = pg_symmetry_prefix_read_with(file, &machine->temporary, machine->output,
			o->limit, o->name_limit, configurations_read, o, &work, &count, &roots);
		o->state = work;
	} else if (operation == &pg_action_scope_operation) {
		struct action_scope_work *work = NULL;
		status = pg_action_scope_work_read_with(file, &machine->temporary, machine->output,
			o->limit, o->name_limit, configurations_read, o, &work, &count, &roots);
		o->state = work;
	} else status = configurations_read(file, machine->output, o->limit, o->name_limit, &count, &roots, o);
	if (status || !count) goto failure;
	if (framed && (machine->current.term != roots[0].head.term
		|| machine->current.environment != roots[0].head.environment || machine->arguments != roots[0].arguments)) goto failure;
	machine->current = roots[0].head;
	machine->arguments = roots[0].arguments;
	if (operation && pg_eval_defer(machine, operation, o->state)) goto failure;
	o->state = NULL;
	o->count = count - 1;
	o->roots = roots + 1;
	return 0;
failure:
	if (o->state) operation->destroy(o->state);
	o->state = NULL;
	return -1;
}

int pg_computation_machine_write(FILE *file, const struct pg_eval *machine,
	const struct pg_eval_policy *policy, size_t count, const struct pg_eval_configuration *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	if (count && !roots) return -1;
	struct machine_owner o = {.codec = codec, .owner = owner, .count = count, .roots = roots};
	return pg_computation_machine_write_with(file, machine, policy, payload_write, &o);
}

int pg_computation_machine_read(FILE *file, struct pg_eval *machine, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	const struct pg_eval_policy **policy, size_t *count, const struct pg_eval_configuration **roots)
{
	if (!count || !roots) return -1;
	*count = 0;
	*roots = NULL;
	struct machine_owner o = {.codec = codec, .owner = owner, .limit = limit, .name_limit = name_limit};
	if (pg_computation_machine_read_with(file, machine, output, name_limit, payload_read, &o, policy)) return -1;
	*count = o.count;
	*roots = o.roots;
	return 0;
}

static const char forest_magic[8] = "APGMFS\1";

struct forest_write {
	size_t machine_count, root_count;
	const struct pg_eval *const *machines;
	const struct pg_eval_policy *const *policies;
	const struct pg_term *const *roots;
	void *owner;
};

static int forest_write(FILE *file, const struct pg_graph_codec *codec, void *state)
{
	const struct forest_write *forest = state;
	if (pg_wire_write_u64(file, forest->machine_count)
		|| pg_graph_write_descriptors(file, forest->root_count, forest->roots, codec, forest->owner)) return -1;
	for (size_t i = 0; i < forest->machine_count; ++i)
		if (pg_computation_machine_write(file, forest->machines[i], forest->policies[i],
			0, NULL, codec, forest->owner)) return -1;
	return 0;
}

int pg_computation_machines_write(FILE *file, size_t machine_count,
	const struct pg_eval *const *machines, const struct pg_eval_policy *const *policies,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if ((machine_count && (!machines || !policies)) || (count && !roots)) return -1;
	struct forest_write forest = {machine_count, count, machines, policies, roots, owner};
	return pg_graph_image_write(file, forest_magic, codec, owner, forest_write, &forest);
}

struct forest_read {
	size_t machine_count, root_count, initialized;
	struct pg_eval **machines;
	const struct pg_eval_policy **policies;
	const struct pg_term *const *roots;
	void *owner;
};

static int forest_read(FILE *file, struct pg_graph *output, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *state)
{
	struct forest_read *forest = state;
	uint64_t n;
	if (pg_wire_read_u64(file, &n) || n > limit || n > SIZE_MAX / sizeof(struct pg_eval *)) return -1;
	forest->machine_count = (size_t)n;
	forest->machines = pg_alloc(output, (size_t)n * sizeof(*forest->machines));
	forest->policies = pg_alloc(output, (size_t)n * sizeof(*forest->policies));
	if (!forest->machines || !forest->policies) return -1;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, forest->owner,
		&forest->root_count, &forest->roots)) return -1;
	for (size_t i = 0; i < n; ++i) {
		forest->machines[i] = pg_alloc(output, sizeof(*forest->machines[i]));
		if (!forest->machines[i]) return -1;
		++forest->initialized;
		size_t extra_count;
		const struct pg_eval_configuration *extra;
		if (pg_computation_machine_read(file, forest->machines[i], output, limit, name_limit,
			codec, forest->owner, &forest->policies[i], &extra_count, &extra) || extra_count) return -1;
	}
	return 0;
}

int pg_computation_machines_read(FILE *file, struct pg_graph *output, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner, size_t *machine_count,
	struct pg_eval *const **machines, const struct pg_eval_policy *const **policies,
	size_t *count, const struct pg_term *const **roots)
{
	if (!machine_count || !machines || !policies || !count || !roots) return -1;
	*machine_count = *count = 0;
	*machines = NULL;
	*policies = NULL;
	*roots = NULL;
	struct forest_read forest = {.owner = owner};
	if (pg_graph_image_read(file, forest_magic, output, limit, name_limit, codec, owner, forest_read, &forest)) {
		for (size_t i = 0; i < forest.initialized; ++i) pg_eval_destroy(forest.machines[i]);
		return -1;
	}
	*machine_count = forest.machine_count;
	*machines = forest.machines;
	*policies = forest.policies;
	*count = forest.root_count;
	*roots = forest.roots;
	return 0;
}
