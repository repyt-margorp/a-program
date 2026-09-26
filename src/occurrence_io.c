#include "occurrence_io.h"
#include "context_io.h"
#include "wire.h"
#include "dag.h"

#include <string.h>

static const char magic[8] = "APGOCC7";
static const char scoped_magic[8] = "APGSCP1";

static int scope_operand(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_scope *scope = key;
	if (index > 1) return 0;
	*child = index ? scope->indices : scope->parent;
	return *child ? 1 : 2;
}

static size_t structure_arity(const struct pg_occurrence *source)
{
	return source->map ? 1 + source->map->count : source->operand_count + (source->origin != NULL);
}

static int operand(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_occurrence *source = key;
	size_t arity = structure_arity(source);
	if (index == arity && source->type) {
		*child = source->type;
		return 1;
	}
	if (index >= arity) {
		index -= arity + (source->type != NULL);
		const struct pg_context_map *const *maps = pg_occurrence_maps(source);
		for (size_t i = 0; i < source->map_count; ++i) {
			if (index < maps[i]->count) { *child = maps[i]->images[index]; return 1; }
			index -= maps[i]->count;
		}
		return 0;
	}
	if (source->map) {
		*child = index ? source->map->images[index - 1] : source->origin;
		return 1;
	}
	if (source->origin) {
		*child = index ? source->operands[index - 1] : source->origin;
		return 1;
	}
	*child = source->operands[index];
	return 1;
}

static int write_dag(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	int scoped, const struct pg_scope *const *scopes,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct pg_graph arena = {0};
	struct pg_dag dag = {0}, scope_dag = {0};
	int status = -1;
	if (pg_graph_init(&arena) || pg_dag_init(&dag, operand, NULL)) goto done;
	if (scoped) {
		if (count && !scopes) goto done;
		if (pg_dag_init(&scope_dag, scope_operand, NULL)) goto done;
		for (size_t i = 0; i < count; ++i) {
			if (!roots[i] || roots[i]->context != (scopes[i] ? scopes[i]->context : NULL)) goto done;
			if (scopes[i] && pg_dag_add(&scope_dag, scopes[i])) goto done;
		}
		for (const struct pg_dag_node *r = scope_dag.first; r; r = r->next)
			if (pg_dag_add(&dag, ((const struct pg_scope *)r->key)->type)) goto done;
	}
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&dag, roots[i])) goto done;
	size_t size = dag.count, context_count = size;
	if (size > SIZE_MAX / sizeof(void *) / 3) goto done;
	if (scope_dag.count > SIZE_MAX / sizeof(void *) - context_count) goto done;
	context_count += scope_dag.count;
	size_t term_count = 3 * size;
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_occurrence *o = r->key;
		if (o->map_count > SIZE_MAX / sizeof(void *) - context_count) goto done;
		context_count += o->map_count;
		if (o->induction) {
			if (o->induction->count > SIZE_MAX / sizeof(void *) - context_count) goto done;
			if (term_count > SIZE_MAX / sizeof(void *) - 3) goto done;
			context_count += o->induction->count;
			term_count += 3;
		}
	}
	const struct pg_context **contexts = pg_alloc(&arena, context_count * sizeof(*contexts));
	const struct pg_term **terms = pg_alloc(&arena, term_count * sizeof(*terms));
	if (!contexts || !terms) goto done;
	size_t next_context = size + scope_dag.count, next_term = 3 * size;
	if (fwrite(scoped ? scoped_magic : magic, 1, 8, file) != 8 || pg_wire_write_u64(file, size) || pg_wire_write_u64(file, count)) goto done;
	if (scoped) {
		if (pg_wire_write_u64(file, scope_dag.count)) goto done;
		for (const struct pg_dag_node *r = scope_dag.first; r; r = r->next) {
			const struct pg_scope *scope = r->key;
			contexts[size + r->id - 1] = scope->context;
			const struct pg_dag_node *parent = pg_dag_find(&scope_dag, scope->parent);
			const struct pg_dag_node *indices = pg_dag_find(&scope_dag, scope->indices);
			const struct pg_dag_node *type = pg_dag_find(&dag, scope->type);
			if (!type || pg_wire_write_u64(file, parent ? parent->id : 0) ||
				pg_wire_write_u64(file, indices ? indices->id : 0) || pg_wire_write_u64(file, type->id)) goto done;
		}
		for (size_t i = 0; i < count; ++i) {
			const struct pg_dag_node *r = pg_dag_find(&scope_dag, scopes[i]);
			if (pg_wire_write_u64(file, r ? r->id : 0)) goto done;
		}
	}
	for (const struct pg_dag_node *r = dag.first; r; r = r->next) {
		const struct pg_occurrence *o = r->key;
		contexts[r->id - 1] = o->context;
		terms[3 * (r->id - 1)] = o->core;
		terms[3 * (r->id - 1) + 1] = o->annotation ? o->annotation : o->core;
		terms[3 * (r->id - 1) + 2] = o->classifier ? o->classifier : o->core;
		int flags = (o->annotation != NULL) | ((o->classifier != NULL) << 1) |
			((o->map != NULL) << 2) | ((o->origin && !o->map) << 3) | ((o->type != NULL) << 4) |
			((o->induction != NULL) << 5);
		size_t arity = structure_arity(o) + (o->type != NULL);
		const struct pg_context_map *const *maps = pg_occurrence_maps(o);
		for (size_t i = 0; i < o->map_count; ++i) {
			if (maps[i]->count > SIZE_MAX - arity) goto done;
			arity += maps[i]->count;
			contexts[next_context++] = maps[i]->source;
		}
		if (fputc(flags, file) == EOF || fputc(o->judgement, file) == EOF ||
			pg_wire_write_u64(file, arity) || pg_wire_write_u64(file, o->selection)) goto done;
		for (size_t i = 0; i < arity; ++i) {
			const void *key;
			if (!operand(NULL, o, i, &key)) goto done;
			const struct pg_dag_node *child = pg_dag_find(&dag, key);
			if (!child || pg_wire_write_u64(file, child->id)) goto done;
		}
		if (pg_wire_write_u64(file, o->map_count)) goto done;
		for (size_t i = 0; i < o->map_count; ++i)
			if (pg_wire_write_u64(file, maps[i]->count)) goto done;
		if (o->induction) {
			const struct pg_induction_allocation *a = o->induction;
			if (pg_wire_write_u64(file, a->count)) goto done;
			for (size_t i = 0; i < a->count; ++i) contexts[next_context++] = a->clauses[i];
			terms[next_term++] = pg_reference(&arena, a->recursion);
			terms[next_term++] = pg_reference(&arena, a->argument);
			terms[next_term++] = pg_reference(&arena, a->self);
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *r = pg_dag_find(&dag, roots[i]);
		if (!r || pg_wire_write_u64(file, r->id)) goto done;
	}
	status = pg_contexts_write(file, context_count, contexts, term_count, terms, name, owner);
done:
	pg_dag_destroy(&scope_dag);
	pg_dag_destroy(&dag);
	pg_graph_destroy(&arena);
	return status;
}

int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	return write_dag(file, count, roots, 0, NULL, name, owner);
}

int pg_scoped_occurrences_write(FILE *file, size_t count,
	const struct pg_scope *const *scopes, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	return write_dag(file, count, roots, 1, scopes, name, owner);
}

struct input {
	size_t count, structural, map_count, clause_count, selection;
	size_t *map_sizes;
	int flags;
	enum pg_evidence_judgement judgement;
	uint64_t *operands;
};

static int read_dag(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_scope *const **scopes, const struct pg_occurrence *const **roots, struct pg_graph *scratch)
{
	if (!file || !typing || !typing->occurrences.capacity || !count || !roots) return -1;
	char header[8];
	uint64_t n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, scopes ? scoped_magic : magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return -1;
	if (n > limit || nr > limit - n || limit > SIZE_MAX / sizeof(struct input)) return -1;
	if (n > SIZE_MAX / 3) return -1;
	struct pg_graph *graph = typing->graph;
	struct input *inputs = pg_alloc(scratch, (size_t)n * sizeof(*inputs));
	uint64_t *ids = pg_alloc(scratch, (size_t)nr * sizeof(*ids));
	const struct pg_occurrence **all = pg_alloc(scratch, (size_t)n * sizeof(*all));
	if (!inputs || !ids || !all) return -1;
	size_t available = limit - (size_t)n - (size_t)nr, max_arity = 0, max_maps = 0, context_count = (size_t)n;
	uint64_t ns = 0, *scope_ids = NULL;
	if (scopes) {
		if (pg_wire_read_u64(file, &ns) || ns > available / 3) return -1;
		available -= 3 * (size_t)ns;
		if (nr > available) return -1;
		available -= (size_t)nr;
		scope_ids = pg_alloc(scratch, (3 * (size_t)ns + (size_t)nr) * sizeof(*scope_ids));
		if (!scope_ids) return -1;
		for (size_t i = 0; i < ns; ++i) {
			if (pg_wire_read_u64(file, &scope_ids[3 * i]) || scope_ids[3 * i] > i) return -1;
			if (pg_wire_read_u64(file, &scope_ids[3 * i + 1]) || scope_ids[3 * i + 1] > i) return -1;
			if (pg_wire_read_u64(file, &scope_ids[3 * i + 2]) || !scope_ids[3 * i + 2] || scope_ids[3 * i + 2] > n) return -1;
		}
		for (size_t i = 0; i < nr; ++i)
			if (pg_wire_read_u64(file, &scope_ids[3 * ns + i]) || scope_ids[3 * ns + i] > ns) return -1;
		context_count += (size_t)ns;
	}
	size_t term_count = 3 * (size_t)n;
	for (size_t i = 0; i < n; ++i) {
		inputs[i].flags = fgetc(file);
		inputs[i].judgement = fgetc(file);
		uint64_t arity, selection;
		if (inputs[i].flags < 0 || inputs[i].flags > 63 || (inputs[i].flags & 12) == 12) return -1;
		if ((inputs[i].flags & 32) && (inputs[i].flags & 12)) return -1;
		if (pg_wire_read_u64(file, &arity) || arity > available) return -1;
		if (pg_wire_read_u64(file, &selection) || selection > SIZE_MAX) return -1;
		if (selection && !(inputs[i].flags & 8)) return -1;
		inputs[i].selection = (size_t)selection;
		available -= (size_t)arity;
		inputs[i].count = (size_t)arity;
		if (arity > max_arity) max_arity = (size_t)arity;
		inputs[i].operands = pg_alloc(scratch, (size_t)arity * sizeof(uint64_t));
		if (!inputs[i].operands) return -1;
		for (size_t j = 0; j < arity; ++j) {
			uint64_t *id = &inputs[i].operands[j];
			if (pg_wire_read_u64(file, id) || !*id || *id > i) return -1;
		}
		uint64_t maps;
		if (pg_wire_read_u64(file, &maps) || maps > available) return -1;
		available -= (size_t)maps;
		context_count += (size_t)maps;
		inputs[i].map_count = (size_t)maps;
		if (maps > max_maps) max_maps = (size_t)maps;
		inputs[i].map_sizes = pg_alloc(scratch, (size_t)maps * sizeof(size_t));
		if (!inputs[i].map_sizes) return -1;
		size_t structural = (size_t)arity;
		for (size_t j = 0; j < maps; ++j) {
			uint64_t images;
			if (pg_wire_read_u64(file, &images) || images > structural) return -1;
			inputs[i].map_sizes[j] = (size_t)images;
			structural -= (size_t)images;
		}
		if ((inputs[i].flags & 16) && (!structural || !(inputs[i].flags & 2))) return -1;
		structural -= !!(inputs[i].flags & 16);
		if ((inputs[i].flags & 4) && !structural) return -1;
		if (inputs[i].flags & 8) {
			if (!structural || structural > 1 + (selection != 0) || (inputs[i].flags & 1)) return -1;
		}
		inputs[i].structural = structural;
		inputs[i].clause_count = 0;
		if (inputs[i].flags & 32) {
			uint64_t clauses;
			if (available < 3) return -1;
			available -= 3;
			if (pg_wire_read_u64(file, &clauses) || clauses > available) return -1;
			available -= (size_t)clauses;
			inputs[i].clause_count = (size_t)clauses;
			context_count += (size_t)clauses;
			if (term_count > SIZE_MAX - 3) return -1;
			term_count += 3;
		}
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &ids[i]) || !ids[i] || ids[i] > n) return -1;
	size_t nc, nt;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	if (pg_contexts_read(file, typing, limit, name_limit, resolve, owner, &nc, &contexts, &nt, &terms)) return -1;
	if (nc != context_count || nt != term_count) return -1;
	const struct pg_occurrence **operands = pg_alloc(scratch, max_arity * sizeof(*operands));
	const struct pg_context_map **maps = pg_alloc(scratch, max_maps * sizeof(*maps));
	if (!operands || !maps) return -1;
	size_t next_context = (size_t)n + (size_t)ns, next_term = 3 * (size_t)n;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < inputs[i].count; ++j) operands[j] = all[inputs[i].operands[j] - 1];
		size_t arity = inputs[i].structural;
		struct pg_occurrence header = {.judgement = inputs[i].judgement, .context = contexts[i],
			.core = terms[3 * i], .classifier = inputs[i].flags & 2 ? terms[3 * i + 2] : NULL,
			.annotation = inputs[i].flags & 1 ? terms[3 * i + 1] : NULL,
			.operand_count = arity, .map_count = inputs[i].map_count};
		const struct pg_occurrence *const *children = operands;
		if (inputs[i].flags & 4) {
			header.origin = operands[0];
			header.map = pg_context_map(typing, operands[0]->context,
				contexts[i], arity - 1, operands + 1);
			if (!header.map) return -1;
			header.operand_count = 0;
		} else if (inputs[i].flags & 8) {
			if (contexts[i] != operands[0]->context) return -1;
			header.origin = operands[0];
			header.selection = inputs[i].selection;
			header.operand_count = arity - 1;
			children = operands + 1;
		}
		if (inputs[i].flags & 16) header.type = operands[arity];
		size_t first_image = arity + !!(inputs[i].flags & 16);
		for (size_t j = 0; j < inputs[i].map_count; ++j) {
			maps[j] = pg_context_map(typing, contexts[next_context++], contexts[i],
				inputs[i].map_sizes[j], operands + first_image);
			if (!maps[j]) return -1;
			first_image += inputs[i].map_sizes[j];
		}
		struct pg_induction_allocation allocation;
		if (inputs[i].flags & 32) {
			for (size_t j = 0; j < 3; ++j)
				if (terms[next_term + j]->kind != PG_REFERENCE) return -1;
			allocation = (struct pg_induction_allocation){
				.recursion = terms[next_term]->as.reference,
				.argument = terms[next_term + 1]->as.reference,
				.self = terms[next_term + 2]->as.reference,
				.count = inputs[i].clause_count, .clauses = contexts + next_context};
			header.induction = &allocation;
			next_context += allocation.count;
			next_term += 3;
		}
		all[i] = pg_occurrence_intern(typing, &header, children, maps);
		if (!all[i]) return -1;
	}
	const struct pg_occurrence **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!result) return -1;
	for (size_t i = 0; i < nr; ++i) result[i] = all[ids[i] - 1];
	if (scopes) {
		const struct pg_scope **all_scopes = pg_alloc(scratch, (size_t)ns * sizeof(*all_scopes));
		const struct pg_scope **selected = pg_alloc(graph, (size_t)nr * sizeof(*selected));
		if (!all_scopes || !selected) return -1;
		for (size_t i = 0; i < ns; ++i) {
			uint64_t parent = scope_ids[3 * i], indices = scope_ids[3 * i + 1];
			all_scopes[i] = pg_scope_intern(typing, contexts[n + i],
				parent ? all_scopes[parent - 1] : NULL, indices ? all_scopes[indices - 1] : NULL,
				all[scope_ids[3 * i + 2] - 1]);
			if (!all_scopes[i]) return -1;
		}
		for (size_t i = 0; i < nr; ++i) {
			uint64_t id = scope_ids[3 * ns + i];
			selected[i] = id ? all_scopes[id - 1] : NULL;
			if (result[i]->context != (selected[i] ? selected[i]->context : NULL)) return -1;
		}
		*scopes = selected;
	}
	*count = (size_t)nr;
	*roots = result;
	return 0;
}

int pg_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_occurrence *const **roots)
{
	/* Wire IDs and assembly arrays are not part of the retained typed graph. */
	struct pg_graph scratch = {0};
	int status = read_dag(file, typing, limit, name_limit, resolve, owner, count, NULL, roots, &scratch);
	pg_graph_destroy(&scratch);
	return status;
}

int pg_scoped_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_scope *const **scopes, const struct pg_occurrence *const **roots)
{
	if (!scopes) return -1;
	struct pg_graph scratch = {0};
	int status = read_dag(file, typing, limit, name_limit, resolve, owner, count, scopes, roots, &scratch);
	pg_graph_destroy(&scratch);
	return status;
}
