#include "eval_io.h"
#include "eval_internal.h"
#include "dag.h"
#include "wire.h"

#include <string.h>
#include <stdlib.h>

static const char magic[8] = "APGCFG\1";
static const char substitution_magic[8] = "APGSUB\2";
static const char materialization_magic[8] = "APGMAT\1";

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

int pg_eval_configurations_write_with(FILE *file, size_t count,
	const struct pg_eval_configuration *roots,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *state)
{
	if (!file || !write_terms || (count && !roots)) return -1;
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
	status = write_terms(file, n, terms, state);
done:
	pg_dag_destroy(&arguments);
	pg_dag_destroy(&environments);
	pg_graph_destroy(&scratch);
	return status;
}

int pg_eval_configurations_read_with(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, size_t *count, const struct pg_eval_configuration **roots,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *, const struct pg_term *const **, void *), void *state)
{
	if (!count || !roots) return -1;
	*count = 0;
	*roots = NULL;
	if (!file || !graph || !read_terms) return -1;
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
	if (read_terms(file, graph, limit, name_limit, &term_count, &terms, state)
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

struct configuration_graph_codec {
	const struct pg_graph_codec *codec;
	void *owner;
};

static int write_configuration_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	const struct configuration_graph_codec *context = opaque;
	return pg_graph_write_descriptors(file, count, roots, context->codec, context->owner);
}

static int read_configuration_terms(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	const struct configuration_graph_codec *context = opaque;
	return pg_graph_read_descriptors(file, graph, limit, name_limit, context->codec, context->owner, count, roots);
}

int pg_eval_configurations_write(FILE *file, size_t count,
	const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner)
{
	struct configuration_graph_codec context = {codec, owner};
	return pg_eval_configurations_write_with(file, count, roots, write_configuration_terms, &context);
}

int pg_eval_configurations_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_eval_configuration **roots)
{
	struct configuration_graph_codec context = {codec, owner};
	return pg_eval_configurations_read_with(file, graph, limit, name_limit, count, roots, read_configuration_terms, &context);
}

static int readback_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct readback_entry *entry = key;
	if (index > 1) return 0;
	*child = index ? entry->right : entry->left;
	return *child ? 1 : 2;
}

static int readback_write(FILE *file, const char format[8], const struct readback_context *context,
	const struct readback_entry *root, uint64_t flags, size_t extra_count,
	const struct pg_eval_configuration *extra, const struct pg_graph_codec *codec, void *owner)
{
	if (!file) return -1;
	struct pg_dag entries = {0};
	int status = -1;
	if (pg_dag_init(&entries, readback_child, NULL) || pg_graph_init(&entries.storage)) goto done;
	if (root && pg_dag_add(&entries, root)) goto done;
	/* Earlier arguments may leave reusable results outside the current root. */
	for (size_t i = 0; i < context->results.capacity; ++i)
		for (const struct pg_index_entry *entry = context->results.buckets[i]; entry; entry = entry->next)
			if (pg_dag_add(&entries, entry)) goto done;
	if (extra_count > SIZE_MAX / sizeof(struct pg_eval_configuration)) goto done;
	if (entries.count > (SIZE_MAX / sizeof(struct pg_eval_configuration) - extra_count) / 4) goto done;
	size_t total = 4 * entries.count + extra_count;
	struct pg_eval_configuration *roots = pg_alloc(&entries.storage, total * sizeof(*roots));
	if (!roots) goto done;
	if (fwrite(format, 1, 8, file) != 8 || pg_wire_write_u64(file, entries.count)
		|| reference(file, &entries, root) || reference(file, &entries, context->pending)
		|| pg_wire_write_u64(file, context->steps) || pg_wire_write_u64(file, flags)) goto done;
	for (const struct pg_dag_node *node = entries.first; node; node = node->next) {
		const struct readback_entry *entry = node->key;
		struct pg_eval_configuration *r = &roots[4 * (node->id - 1)];
		r[0] = (struct pg_eval_configuration){entry->input, NULL};
		r[1] = (struct pg_eval_configuration){{entry->input.term, entry->cursor}, NULL};
		r[2] = (struct pg_eval_configuration){{entry->binder
			? pg_reference(&entries.storage, entry->binder) : entry->input.term, NULL}, NULL};
		r[3] = (struct pg_eval_configuration){{entry->result ? entry->result : entry->input.term, NULL}, NULL};
		if (pg_wire_write_u64(file, entry->stage) || pg_wire_write_u64(file, entry->binder != NULL)
			|| pg_wire_write_u64(file, entry->result != NULL) || reference(file, &entries, entry->left)
			|| reference(file, &entries, entry->right) || reference(file, &entries, entry->next)) goto done;
	}
	for (size_t i = 0; i < extra_count; ++i) roots[4 * entries.count + i] = extra[i];
	status = pg_eval_configurations_write(file, total, roots, codec, owner);
done:
	pg_dag_destroy(&entries);
	return status;
}

static int readback_read(FILE *file, const char format[8], struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner, size_t extra_count,
	struct readback_context *output, struct readback_entry **output_root,
	uint64_t *flags, const struct pg_eval_configuration **extra)
{
	if (!file || !graph) return -1;
	char header[8];
	uint64_t n, root, pending, steps;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, format, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &root)
		|| pg_wire_read_u64(file, &pending) || pg_wire_read_u64(file, &steps)
		|| pg_wire_read_u64(file, flags)) return -1;
	if (extra_count > limit || n > (limit - extra_count) / 4 || root > n || pending > n) return -1;
	if ((!root) != (!n)) return -1;
	if (n > SIZE_MAX / sizeof(uint64_t) / 6 || n > SIZE_MAX / sizeof(struct readback_entry)) return -1;
	struct pg_graph scratch = {0};
	if (pg_graph_init(&scratch)) return -1;
	struct readback_context restored = {.output = graph, .steps = steps};
	int status = -1;
	struct readback_context *context = &restored;
	if (pg_index_init(&context->results)) goto done;
	uint64_t *records = pg_alloc(&scratch, (size_t)n * 6 * sizeof(*records));
	if (!records) goto done;
	for (size_t i = 0; i < n; ++i) {
		uint64_t *r = &records[6 * i];
		for (size_t j = 0; j < 6; ++j) if (pg_wire_read_u64(file, &r[j])) goto done;
		if (r[0] > 2 || r[1] > 1 || r[2] > 1 || r[3] > i || r[4] > i || r[5] > n) goto done;
	}
	size_t count;
	const struct pg_eval_configuration *roots;
	if (pg_eval_configurations_read(file, graph, limit, name_limit, codec, owner, &count, &roots)
		|| count != 4 * n + extra_count) goto done;
	struct readback_entry *entries = pg_alloc(&context->temporary, (size_t)n * sizeof(*entries));
	if (!entries) goto done;
	for (size_t i = 0; i < n; ++i) {
		uint64_t *r = &records[6 * i];
		const struct pg_eval_configuration *c = &roots[4 * i];
		for (size_t j = 0; j < 4; ++j) if (c[j].arguments) goto done;
		if (c[0].head.term != c[1].head.term || c[2].head.environment || c[3].head.environment) goto done;
		struct readback_entry *entry = &entries[i];
		*entry = (struct readback_entry){.input = c[0].head, .cursor = c[1].head.environment,
			.stage = (unsigned)r[0], .result = r[2] ? c[3].head.term : NULL,
			.left = r[3] ? &entries[r[3] - 1] : NULL, .right = r[4] ? &entries[r[4] - 1] : NULL,
			.next = r[5] ? &entries[r[5] - 1] : NULL};
		if (r[1]) {
			const struct pg_term *binder = c[2].head.term;
			if (binder->kind != PG_REFERENCE || binder->as.reference->kind != PG_BINDER) goto done;
			entry->binder = binder->as.reference;
		}
		if (entry->stage == 2 && entry->input.term->kind != PG_APPLICATION) goto done;
		if (entry->stage && !entry->result) {
			if (!entry->left) goto done;
			if (entry->stage == 2 && !entry->right) goto done;
			if (entry->input.term->kind == PG_LAMBDA && !entry->binder) goto done;
		}
		if (pg_readback_index(context, entry)) goto done;
	}
	context->pending = pending ? &entries[pending - 1] : NULL;
	struct membership { unsigned char reachable, queued; };
	struct membership *members = pg_alloc(&scratch, (size_t)n * sizeof(*members));
	if (!members) goto done;
	memset(members, 0, (size_t)n * sizeof(*members));
	/* Child IDs precede parents, so one reverse pass establishes reachability. */
	if (root) members[root - 1].reachable = 1;
	for (size_t i = (size_t)n; i; --i) {
		if (!members[i - 1].reachable) continue;
		uint64_t *r = &records[6 * (i - 1)];
		if (r[3]) members[r[3] - 1].reachable = 1;
		if (r[4]) members[r[4] - 1].reachable = 1;
	}
	for (uint64_t id = pending; id;) {
		members[id - 1].queued = 1;
		uint64_t next = records[6 * (id - 1) + 5];
		if (next && next <= id) goto done;
		id = next;
	}
	for (size_t i = 0; i < n; ++i) {
		if (!members[i].reachable && (!extra_count || !entries[i].result)) goto done;
		if (members[i].queued != (entries[i].result == NULL)) goto done;
	}
	if (root && (context->pending == NULL) != (entries[root - 1].result != NULL)) goto done;
	*output = restored;
	*output_root = root ? &entries[root - 1] : NULL;
	*extra = &roots[4 * n];
	memset(&restored, 0, sizeof(restored));
	status = 0;
done:
	pg_readback_destroy(&restored);
	pg_graph_destroy(&scratch);
	return status;
}

int pg_substitution_write(FILE *file, const struct pg_substitution *work,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!work || pg_substitution_status(work) == PG_SUBSTITUTION_ERROR) return -1;
	return readback_write(file, substitution_magic, &work->state->context,
		work->state->root, 0, 0, NULL, codec, owner);
}

int pg_substitution_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_substitution *work)
{
	if (!work) return -1;
	work->state = NULL;
	struct pg_substitution candidate = {calloc(1, sizeof(*candidate.state))};
	if (!candidate.state) return -1;
	const struct pg_eval_configuration *extra;
	uint64_t flags;
	if (readback_read(file, substitution_magic, graph, limit, name_limit, codec, owner, 0,
		&candidate.state->context, &candidate.state->root, &flags, &extra)
		|| flags || !candidate.state->root) {
		pg_substitution_destroy(&candidate);
		return -1;
	}
	candidate.state->status = candidate.state->context.pending ? PG_SUBSTITUTION_PENDING : PG_SUBSTITUTION_DONE;
	*work = candidate;
	return 0;
}

int pg_materialization_write(FILE *file, const struct materialization *work,
	const struct pg_eval_configuration *input, const struct pg_graph_codec *codec, void *owner)
{
	if (!work || !input || !input->head.term) return -1;
	uint64_t flags = (work->readback.output != NULL) | (work->done ? 2 : 0) | (work->partial ? 4 : 0);
	struct pg_eval_configuration extra[] = {
		*input,
		{{input->head.term, NULL}, work->remaining},
		{{work->partial ? work->partial : input->head.term, NULL}, NULL}
	};
	return readback_write(file, materialization_magic, &work->readback, work->entry, flags, 3, extra, codec, owner);
}

int pg_materialization_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct materialization *work, struct pg_eval_configuration *input)
{
	if (!work || !input) return -1;
	memset(work, 0, sizeof(*work));
	memset(input, 0, sizeof(*input));
	struct materialization candidate = {0};
	const struct pg_eval_configuration *extra;
	uint64_t flags;
	if (readback_read(file, materialization_magic, graph, limit, name_limit, codec, owner, 3,
		&candidate.readback, &candidate.entry, &flags, &extra)) goto failure;
	if (flags > 7 || ((flags & 1) != (candidate.entry != NULL))) goto failure;
	if (extra[1].head.term != extra[0].head.term || extra[1].head.environment
		|| extra[2].head.environment || extra[2].arguments) goto failure;
	candidate.remaining = extra[1].arguments;
	candidate.partial = flags & 4 ? extra[2].head.term : NULL;
	candidate.done = (flags & 2) != 0;
	if (!candidate.entry) {
		if (flags || candidate.readback.steps || candidate.remaining) goto failure;
		pg_index_destroy(&candidate.readback.results);
		candidate.readback.output = NULL;
	} else if (candidate.done) {
		if (!candidate.partial || candidate.remaining || candidate.readback.pending || !candidate.entry->result) goto failure;
	}
	*work = candidate;
	*input = extra[0];
	return 0;
failure:
	pg_materialize_destroy(&candidate);
	return -1;
}
