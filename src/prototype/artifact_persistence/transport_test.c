#include "occurrence_io.h"
#include "declaration_io.h"
#include "classifier.h"
#include "iadt.h"
#include "dag.h"
#include "wire.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Transport must not evaluate, even if a saved classifier looks complete. */
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	(void)work; (void)budget;
	abort();
}

static void equal_files(FILE *left, FILE *right)
{
	rewind(left); rewind(right);
	for (;;) {
		int a = fgetc(left), b = fgetc(right);
		assert(a == b);
		if (a == EOF) break;
	}
	assert(!ferror(left) && !ferror(right));
}

static void reference_aliases(void)
{
	struct pg_graph graph, temporary, restored;
	assert(!pg_graph_init(&graph) && !pg_graph_init(&temporary) && !pg_graph_init(&restored));
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_term *ref = pg_reference(&graph, binder);
	const struct pg_term *alias = pg_reference(&temporary, binder);
	const struct pg_term *lambda = pg_lambda(&graph, binder, ref);
	const struct pg_term *roots[] = {lambda, alias, ref};
	assert(ref != alias);
	struct pg_dag terms, objects;
	assert(!pg_dag_init(&objects, NULL, NULL));
	assert(!pg_graph_dependencies_init(&terms, &objects, NULL, NULL));
	for (size_t i = 0; i < 3; ++i) assert(!pg_dag_add(&terms, roots[i]));
	assert(terms.count == 2 && objects.count == 1);
	assert(pg_dag_find(&terms, ref) == pg_dag_find(&terms, alias));
	pg_dag_destroy(&terms); pg_dag_destroy(&objects);
	FILE *file = tmpfile(), *again = tmpfile();
	size_t before = graph.terms.count;
	assert(file && again && !pg_graph_write(file, 3, roots, NULL, NULL));
	assert(graph.terms.count == before);
	rewind(file);
	char magic[8]; uint64_t no, nt, nr;
	assert(fread(magic, 1, 8, file) == 8);
	assert(!pg_wire_read_u64(file, &no) && !pg_wire_read_u64(file, &nt) && !pg_wire_read_u64(file, &nr));
	assert(no == 1 && nt == 2 && nr == 3);
	rewind(file);
	size_t count; const struct pg_term *const *loaded;
	assert(!pg_graph_read(file, &restored, 100, 0, NULL, NULL, &count, &loaded));
	assert(count == 3 && loaded[1] == loaded[2] && loaded[0]->as.lambda.body == loaded[1]);
	assert(!pg_graph_write(again, count, loaded, NULL, NULL));
	equal_files(file, again);
	assert(!fclose(file) && !fclose(again));
	pg_graph_destroy(&restored); pg_graph_destroy(&temporary); pg_graph_destroy(&graph);
}

struct store {
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_declaration_io codec;
};

static void initialize(struct store *store)
{
	assert(!pg_graph_init(&store->graph));
	assert(!pg_typing_init(&store->typing, &store->graph));
	assert(!pg_declaration_io_init(&store->codec, &store->typing));
}

static void destroy(struct store *store)
{
	pg_declaration_io_destroy(&store->codec);
	pg_typing_destroy(&store->typing);
	pg_graph_destroy(&store->graph);
}

struct image {
	struct store *store;
	int scoped;
	size_t count, term_count;
	const struct pg_scope *const *scopes;
	const struct pg_occurrence *const *roots;
	const struct pg_term *const *terms;
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *owner)
{
	const struct image *image = owner;
	int status = image->scoped
		? pg_scoped_occurrences_write_descriptors(file, image->count, image->scopes, image->roots, codec, &image->store->codec)
		: pg_occurrences_write_descriptors(file, image->count, image->roots, codec, &image->store->codec);
	if (status) return status;
	return pg_graph_write_descriptors(file, image->term_count, image->terms, codec, &image->store->codec);
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t names,
	const struct pg_graph_codec *codec, void *owner)
{
	struct image *image = owner;
	int status = image->scoped
		? pg_scoped_occurrences_read_descriptors(file, &image->store->typing, limit, names, codec,
			&image->store->codec, &image->count, &image->scopes, &image->roots)
		: pg_occurrences_read_descriptors(file, &image->store->typing, limit, names, codec,
			&image->store->codec, &image->count, &image->roots);
	if (status) return status;
	return pg_graph_read_descriptors(file, graph, limit, names, codec, &image->store->codec,
		&image->term_count, &image->terms);
}

static void write_image(FILE *file, struct image *image)
{
	size_t terms = image->store->graph.terms.count, occurrences = image->store->typing.occurrences.count;
	assert(!pg_graph_image_write(file, "APGTEST1", &pg_declaration_graph_codec,
		&image->store->codec, write_payload, image));
	assert(image->store->graph.terms.count == terms && image->store->typing.occurrences.count == occurrences);
	assert(!image->store->typing.proofs.count);
}

static void check_image(const struct image *image)
{
	assert(image->count == 6 && image->term_count == 2);
	const struct pg_occurrence *const *roots = image->roots;
	assert(roots[0] == roots[2] && roots[0] != roots[1]);
	assert(roots[0]->core == roots[1]->core && roots[0]->classifier != roots[1]->classifier);
	assert(roots[0]->context->binder == roots[1]->context->binder);
	assert(roots[0]->type->origin == roots[3] && roots[1]->type->origin == roots[4]);
	assert(roots[0]->type->context == roots[0]->context && roots[1]->type->context == roots[1]->context);
	assert(roots[3]->core != roots[4]->core);
	assert(roots[3]->core == image->terms[0] && roots[0]->core == image->terms[1]);
	const struct pg_data_declaration *a = pg_data_declaration_view(roots[3]->core->as.reference);
	const struct pg_data_declaration *b = pg_data_declaration_view(roots[4]->core->as.reference);
	assert(a && b && a != b && pg_data_declaration_layout(a) != pg_data_declaration_layout(b));
	assert(roots[5]->core->as.reference == pg_data_constructor(pg_data_declaration_layout(a), 0));
	if (image->scoped) {
		assert(image->scopes[0] == image->scopes[2]);
		assert(image->scopes[0]->context == roots[0]->context && image->scopes[0]->type == roots[3]);
		assert(image->scopes[1]->context == roots[1]->context && image->scopes[1]->type == roots[4]);
		assert(!image->scopes[3] && !image->scopes[4] && !image->scopes[5]);
	}
	assert(!image->store->typing.proofs.count && !image->store->typing.typed_queries.count);
	assert(!image->store->typing.occurrence_actions.count && !image->store->typing.occurrence_inputs.count);
}

static void rejected_reads(const struct image *image)
{
	FILE *file = tmpfile(); assert(file);
	int result = image->scoped
		? pg_scoped_occurrences_write_descriptors(file, image->count, image->scopes, image->roots,
			&pg_declaration_graph_codec, &image->store->codec)
		: pg_occurrences_write_descriptors(file, image->count, image->roots,
			&pg_declaration_graph_codec, &image->store->codec);
	assert(!result);
	for (int missing_codec = 0; missing_codec < 2; ++missing_codec) {
		struct store target;
		initialize(&target);
		size_t count = 37;
		const struct pg_occurrence *const *roots = image->roots;
		const struct pg_scope *const *scopes = image->scopes;
		const struct pg_graph_codec *codec = missing_codec ? NULL : &pg_declaration_graph_codec;
		size_t limit = missing_codec ? 10000 : 0;
		rewind(file);
		result = image->scoped
			? pg_scoped_occurrences_read_descriptors(file, &target.typing, limit, 1000, codec,
				&target.codec, &count, &scopes, &roots)
			: pg_occurrences_read_descriptors(file, &target.typing, limit, 1000, codec,
				&target.codec, &count, &roots);
		assert(result == -1 && count == 37 && roots == image->roots && scopes == image->scopes);
		assert(!target.typing.proofs.count);
		destroy(&target);
	}
	assert(!fclose(file));
}

static void nominal_occurrences(int scoped)
{
	struct store source;
	initialize(&source);
	struct pg_graph *g = &source.graph;
	struct pg_typing *t = &source.typing;
	const struct pg_term *universe = pg_universe(g, 0);
	const struct pg_object *self = pg_binder(g), *binder = pg_binder(g);
	const struct pg_context *parameters = pg_context_bind(t, NULL, self, universe, PG_JUDGEMENT_VALUE);
	const struct pg_term *result = pg_reference(g, self);
	const struct pg_data_constructor_input constructor = {parameters, &result};
	const struct pg_occurrence *families[2], *variables[2];
	const struct pg_scope *scopes[6] = {0};
	const struct pg_data_declaration *declarations[2];
	for (size_t i = 0; i < 2; ++i) {
		declarations[i] = pg_data_declaration(g, parameters, parameters, 1, &constructor);
		const struct pg_term *family = pg_reference(g, pg_data_declaration_family(declarations[i]));
		families[i] = pg_occurrence(t, PG_JUDGEMENT_VALUE_TYPE, NULL, family, universe, NULL, 0, NULL);
		const struct pg_context *context = pg_context_bind(t, NULL, binder, family, PG_JUDGEMENT_VALUE);
		struct pg_occurrence header = {.judgement = PG_JUDGEMENT_VALUE, .context = context,
			.core = pg_reference(g, binder), .classifier = family,
			.type = pg_occurrence_weaken(t, context, families[i])};
		variables[i] = pg_occurrence_intern(t, &header, NULL, NULL);
		scopes[i] = pg_scope_intern(t, context, NULL, NULL, families[i]);
		assert(families[i] && variables[i] && scopes[i]);
	}
	scopes[2] = scopes[0];
	const struct pg_occurrence *zero = pg_occurrence_typed(t, PG_JUDGEMENT_VALUE,
		pg_reference(g, pg_data_constructor(pg_data_declaration_layout(declarations[0]), 0)), families[0], NULL, 0, NULL);
	const struct pg_occurrence *roots[] = {variables[0], variables[1], variables[0], families[0], families[1], zero};
	const struct pg_term *terms[] = {families[0]->core, variables[0]->core};
	struct image image = {&source, scoped, 6, 2, scopes, roots, terms};
	rejected_reads(&image);
	FILE *file = tmpfile();
	assert(file);
	write_image(file, &image);
	destroy(&source);
	for (size_t generation = 0; generation < 3; ++generation) {
		struct store target;
		initialize(&target);
		struct image loaded = {.store = &target, .scoped = scoped};
		rewind(file);
		assert(!pg_graph_image_read(file, "APGTEST1", &target.graph, 10000, 1000,
			&pg_declaration_graph_codec, &target.codec, read_payload, &loaded));
		check_image(&loaded);
		FILE *next = tmpfile(); assert(next);
		write_image(next, &loaded);
		equal_files(file, next);
		assert(!fclose(file)); file = next;
		destroy(&target);
	}
	assert(!fclose(file));
}

int main(void)
{
	reference_aliases();
	nominal_occurrences(0);
	nominal_occurrences(1);
	puts("artifact transport: descriptor sharing, nominal separation and inert byte-stable cycles passed");
	return 0;
}
