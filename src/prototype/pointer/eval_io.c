#include "eval_io.h"
#include "dag.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGCFG\1";

static int environment_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_environment *environment = key;
	if (index > 1) return 0;
	*child = index ? environment->value.environment : environment->parent;
	return *child ? 1 : 2;
}

static int argument_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	if (index) return 0;
	*child = ((const struct pg_argument *)key)->next;
	return *child ? 1 : 2;
}

static int reference(FILE *file, const struct pg_dag *dag, const void *key)
{
	if (!key) return pg_wire_write_u64(file, 0);
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? pg_wire_write_u64(file, node->id) : -1;
}

static int read_pair(FILE *file, uint64_t *pair, uint64_t left_limit, uint64_t right_limit)
{
	if (pg_wire_read_u64(file, pair) || pg_wire_read_u64(file, pair + 1)) return -1;
	return pair[0] <= left_limit && pair[1] <= right_limit ? 0 : -1;
}

int pg_eval_configurations_write(FILE *file, size_t count,
	const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct pg_graph scratch = {0};
	struct pg_dag environments = {0}, arguments = {0};
	int status = -1;
	if (pg_graph_init(&scratch) || pg_dag_init(&environments, environment_child, NULL)
		|| pg_dag_init(&arguments, argument_child, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) {
		if (!roots[i].head.term) goto done;
		if (roots[i].head.environment && pg_dag_add(&environments, roots[i].head.environment)) goto done;
		if (roots[i].arguments && pg_dag_add(&arguments, roots[i].arguments)) goto done;
	}
	for (const struct pg_dag_node *node = arguments.first; node; node = node->next) {
		const struct pg_argument *argument = node->key;
		if (argument->value.environment && pg_dag_add(&environments, argument->value.environment)) goto done;
	}
	if (arguments.count > SIZE_MAX - count) goto done;
	size_t n = count + arguments.count;
	if (environments.count > (SIZE_MAX - n) / 2) goto done;
	n += 2 * environments.count;
	if (n > SIZE_MAX / sizeof(const struct pg_term *)) goto done;
	const struct pg_term **terms = pg_alloc(&scratch, n * sizeof(*terms));
	if (!terms) goto done;
	if (fwrite(magic, 1, sizeof(magic), file) != sizeof(magic)
		|| pg_wire_write_u64(file, environments.count) || pg_wire_write_u64(file, arguments.count)
		|| pg_wire_write_u64(file, count)) goto done;
	size_t position = 0;
	for (const struct pg_dag_node *node = environments.first; node; node = node->next) {
		const struct pg_environment *environment = node->key;
		if (!environment->binder || environment->binder->kind != PG_BINDER || !environment->value.term) goto done;
		terms[position++] = pg_reference(&scratch, environment->binder);
		terms[position++] = environment->value.term;
		if (reference(file, &environments, environment->parent)
			|| reference(file, &environments, environment->value.environment)) goto done;
	}
	for (const struct pg_dag_node *node = arguments.first; node; node = node->next) {
		const struct pg_argument *argument = node->key;
		terms[position++] = argument->value.term;
		if (reference(file, &arguments, argument->next)
			|| reference(file, &environments, argument->value.environment)) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		terms[position++] = roots[i].head.term;
		if (reference(file, &environments, roots[i].head.environment)
			|| reference(file, &arguments, roots[i].arguments)) goto done;
	}
	status = pg_graph_write_descriptors(file, n, terms, codec, owner);
done:
	pg_dag_destroy(&arguments);
	pg_dag_destroy(&environments);
	pg_graph_destroy(&scratch);
	return status;
}

int pg_eval_configurations_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_eval_configuration **roots)
{
	if (!count || !roots) return -1;
	*count = 0;
	*roots = NULL;
	if (!file || !graph) return -1;
	char header[8];
	uint64_t ne, na, nr;
	if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, magic, sizeof(header))) return -1;
	if (pg_wire_read_u64(file, &ne) || pg_wire_read_u64(file, &na) || pg_wire_read_u64(file, &nr)) return -1;
	if (nr > limit || na > limit - nr || ne > (limit - nr - na) / 2) return -1;
	size_t n = (size_t)(2 * ne + na + nr), records = (size_t)(ne + na + nr);
	if (records > SIZE_MAX / sizeof(uint64_t) / 2) return -1;
	if (ne > SIZE_MAX / sizeof(struct pg_environment) || na > SIZE_MAX / sizeof(struct pg_argument)
		|| nr > SIZE_MAX / sizeof(struct pg_eval_configuration)) return -1;
	struct pg_graph scratch = {0};
	if (pg_graph_init(&scratch)) return -1;
	int status = -1;
	uint64_t *links = pg_alloc(&scratch, 2 * records * sizeof(*links));
	if (!links) goto done;
	for (size_t i = 0; i < ne; ++i)
		if (read_pair(file, &links[2 * i], i, i)) goto done;
	for (size_t i = 0; i < na; ++i)
		if (read_pair(file, &links[2 * (ne + i)], i, ne)) goto done;
	for (size_t i = 0; i < nr; ++i)
		if (read_pair(file, &links[2 * (ne + na + i)], ne, na)) goto done;
	const struct pg_term *const *terms;
	size_t term_count;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, owner, &term_count, &terms)
		|| term_count != n) goto done;
	struct pg_environment *environments = pg_alloc(graph, (size_t)ne * sizeof(*environments));
	struct pg_argument *arguments = pg_alloc(graph, (size_t)na * sizeof(*arguments));
	struct pg_eval_configuration *result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!environments || !arguments || !result) goto done;
	size_t position = 0;
	for (size_t i = 0; i < ne; ++i) {
		const struct pg_term *binder = terms[position++];
		if (binder->kind != PG_REFERENCE || binder->as.reference->kind != PG_BINDER) goto done;
		uint64_t parent = links[2 * i], captured = links[2 * i + 1];
		environments[i] = (struct pg_environment){binder->as.reference,
			{terms[position++], captured ? &environments[captured - 1] : NULL},
			parent ? &environments[parent - 1] : NULL};
	}
	for (size_t i = 0; i < na; ++i) {
		uint64_t next = links[2 * (ne + i)], captured = links[2 * (ne + i) + 1];
		arguments[i] = (struct pg_argument){
			{terms[position++], captured ? &environments[captured - 1] : NULL},
			next ? &arguments[next - 1] : NULL};
	}
	for (size_t i = 0; i < nr; ++i) {
		uint64_t captured = links[2 * (ne + na + i)], args = links[2 * (ne + na + i) + 1];
		result[i] = (struct pg_eval_configuration){
			{terms[position++], captured ? &environments[captured - 1] : NULL},
			args ? &arguments[args - 1] : NULL};
	}
	*count = (size_t)nr;
	*roots = result;
	status = 0;
done:
	pg_graph_destroy(&scratch);
	return status;
}
