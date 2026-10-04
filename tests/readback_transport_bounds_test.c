#include "eval_io.h"
#include "eval_internal.h"
#include "wire.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static struct pg_eval_configuration fixture(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	struct pg_environment *environment = pg_alloc(graph, sizeof(*environment));
	struct pg_argument *arguments = pg_alloc(graph, 2 * sizeof(*arguments));
	assert(environment && arguments);
	*environment = (struct pg_environment){x, {vy, NULL}, NULL};
	struct pg_closure head = {pg_lambda(graph, y, vx), environment};
	arguments[0] = (struct pg_argument){head, &arguments[1]};
	arguments[1] = (struct pg_argument){{pg_application(graph, vx, vx), environment}, NULL};
	return (struct pg_eval_configuration){head, arguments};
}

static void rejected_write(const struct materialization *work, const struct pg_eval_configuration *input)
{
	FILE *file = tmpfile();
	assert(file && pg_materialization_write(file, work, input, NULL, NULL));
	assert(!fclose(file));
}

static void rejected_read(FILE *file, size_t limit)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct materialization rejected;
	struct pg_eval_configuration ignored;
	rewind(file);
	assert(pg_materialization_read(file, &graph, limit, 100, NULL, NULL, &rejected, &ignored));
	assert(!rejected.entry && !ignored.head.term && !ignored.arguments);
	pg_graph_destroy(&graph);
}

int main(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct pg_eval_configuration input = fixture(&graph);
	struct materialization work = {0};
	size_t steps = 0;
	while (!pg_materialize_step(&work, &graph, input.head, input.arguments))
		assert(++steps < 100);
	assert(work.done && work.partial && work.readback.results.count >= 2);
	const struct pg_term *shared = work.partial->as.application.function;
	assert(shared->kind == PG_APPLICATION && shared->as.application.function == shared->as.application.argument);
	struct readback_entry *entries[32];
	size_t count = 0;
	for (size_t bucket = 0; bucket < work.readback.results.capacity; ++bucket) {
		for (struct pg_index_entry *node = work.readback.results.buckets[bucket]; node; node = node->next) {
			assert(count < sizeof(entries) / sizeof(*entries));
			entries[count++] = (struct readback_entry *)node;
		}
	}
	assert(count == work.readback.results.count && count >= 2);
	size_t original_order = entries[0]->order;
	entries[0]->order = SIZE_MAX;
	rejected_write(&work, &input);
	entries[0]->order = entries[1]->order;
	rejected_write(&work, &input);
	entries[0]->order = original_order;
	work.readback.results.count = count + 1;
	rejected_write(&work, &input);
	work.readback.results.count = SIZE_MAX / sizeof(struct readback_entry *) + 1;
	rejected_write(&work, &input);
	work.readback.results.count = count;

	FILE *file = tmpfile();
	assert(file && !pg_materialization_write(file, &work, &input, NULL, NULL));
	rejected_read(file, 0);
	struct pg_graph restored;
	assert(!pg_graph_init(&restored));
	struct materialization loaded;
	struct pg_eval_configuration loaded_input;
	rewind(file);
	assert(!pg_materialization_read(file, &restored, 10000, 100, NULL, NULL, &loaded, &loaded_input));
	assert(loaded_input.head.term == loaded_input.arguments->value.term);
	assert(loaded_input.head.environment == loaded_input.arguments->value.environment);
	assert(loaded.readback.results.count == count && loaded.readback.steps == work.readback.steps);
	shared = loaded.partial->as.application.function;
	assert(shared->kind == PG_APPLICATION && shared->as.application.function == shared->as.application.argument);
	uint64_t before = loaded.readback.steps;
	const struct pg_term *answer = loaded.partial;
	assert(pg_materialize_step(&loaded, &restored, loaded_input.head, loaded_input.arguments) == 1);
	assert(loaded.partial == answer && loaded.readback.steps == before && loaded.readback.results.count == count);
	pg_materialize_destroy(&loaded);
	pg_graph_destroy(&restored);
	assert(!fseek(file, 8, SEEK_SET) && !pg_wire_write_u64(file, UINT64_MAX));
	rejected_read(file, SIZE_MAX);
	assert(!fclose(file));
	pg_materialize_destroy(&work);
	pg_graph_destroy(&graph);
	puts("readback transport: retained aliases/answers, no-step reuse, invalid order/count and wire overflow rejected");
	return 0;
}
