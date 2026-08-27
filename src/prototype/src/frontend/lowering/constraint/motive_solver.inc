/* Motive synthesis is typed compiler work above the context-free Core graph.
 * The helpers here derive rewrite candidates from one certified family index;
 * they never turn Core shape equality into judgemental equality. */

static int operation_motive_core_constructor_spine(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t* p_core,
	int* p_is_constructor
) {
	if (!ctx || !p_core || !p_is_constructor || term >= ctx->terms->term_count) {
		return -1;
	}
	*p_is_constructor = 0;
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	int spine_status = prototype_term_constructor_spine_info(
		ctx->terms,
		term,
		&head,
		&owner,
		&constructor_id,
		arguments,
		64,
		&argument_count
	);
	if (spine_status != 0) {
		*p_core = term;
		return spine_status < 0 ? -1 : 0;
	}
	uint32_t core_owner;
	uint32_t core;
	if (prototype_term_core_projection(
			ctx->terms, owner, &core_owner
		) != 0 || prototype_term_constructor(
			ctx->terms, core_owner, constructor_id, &core
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_term_app(ctx->terms, core, arguments[i], &core) != 0) {
			return -1;
		}
	}
	*p_core = core;
	*p_is_constructor = 1;
	(void)head;
	return 0;
}

struct operation_motive_occurrence_set {
	uint32_t* terms;
	uint8_t* visited;
	size_t count;
	size_t capacity;
	size_t visited_count;
};

static void operation_motive_occurrence_set_dispose(
	struct operation_motive_occurrence_set* set
) {
	if (!set) {
		return;
	}
	free(set->terms);
	free(set->visited);
	memset(set, 0, sizeof(*set));
}

static int operation_motive_occurrence_set_add(
	struct operation_motive_occurrence_set* set,
	uint32_t term
) {
	if (!set) {
		return -1;
	}
	if (set->count == set->capacity) {
		size_t capacity = set->capacity ? set->capacity * 2 : 8;
		if (capacity < set->capacity || capacity > SIZE_MAX / sizeof(*set->terms)) {
			return -1;
		}
		uint32_t* terms = realloc(set->terms, capacity * sizeof(*terms));
		if (!terms) {
			return -1;
		}
		set->terms = terms;
		set->capacity = capacity;
	}
	set->terms[set->count++] = term;
	return 0;
}

/* Locate structural occurrences without mutating Core substitution semantics.
 * The caller supplies a shape derived from one certified family index and must
 * validate the resulting typed motive against every branch equation. */
static int operation_motive_collect_shape_occurrences(
	const struct prototype_term_db* terms,
	uint32_t root,
	uint32_t shape,
	struct operation_motive_occurrence_set* set
) {
	if (!terms || !set || root >= terms->term_count ||
		shape >= terms->term_count || root >= set->visited_count) {
		return -1;
	}
	if (set->visited[root]) {
		return 0;
	}
	set->visited[root] = 1;
	int equal;
	if (prototype_term_view_shape_equal(terms, root, shape, &equal) != 0) {
		return -1;
	}
	if (equal) {
		return operation_motive_occurrence_set_add(set, root);
	}
	uint32_t child_count;
	if (prototype_term_child_count(terms, root, &child_count) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		struct prototype_term_child child;
		if (prototype_term_child_at(terms, root, i, &child) != 0 ||
			operation_motive_collect_shape_occurrences(
				terms, child.term, shape, set
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_motive_replace_certified_core_shape(
	struct compile_context* ctx,
	uint32_t body,
	uint32_t core_index,
	uint32_t replacement,
	uint32_t* p_result,
	int* p_dependency_found
) {
	if (!ctx || !p_result || !p_dependency_found ||
		body >= ctx->terms->term_count || core_index >= ctx->terms->term_count ||
		replacement >= ctx->terms->term_count) {
		return -1;
	}
	struct operation_motive_occurrence_set occurrences;
	memset(&occurrences, 0, sizeof(occurrences));
	occurrences.visited_count = ctx->terms->term_count;
	occurrences.visited = calloc(occurrences.visited_count, 1);
	if (!occurrences.visited) {
		return -1;
	}
	int status = operation_motive_collect_shape_occurrences(
		ctx->terms, body, core_index, &occurrences
	);
	uint32_t result = body;
	for (size_t i = 0; status == 0 && i < occurrences.count; ++i) {
		status = prototype_term_graph_replace_exact(
			ctx->terms,
			prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
			result,
			occurrences.terms[i],
			replacement,
			&result
		);
	}
	*p_result = result;
	*p_dependency_found = status == 0 && occurrences.count > 0;
	operation_motive_occurrence_set_dispose(&occurrences);
	return status;
}

static int operation_motive_abstract_index(
	struct compile_context* ctx,
	uint32_t body,
	uint32_t index_term,
	uint32_t replacement,
	uint32_t* p_result,
	int* p_dependency_found
) {
	if (!ctx || !p_result || !p_dependency_found ||
		body >= ctx->terms->term_count || index_term >= ctx->terms->term_count ||
		replacement >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t result;
	if (prototype_term_graph_replace_exact(
			ctx->terms,
			prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
			body,
			index_term,
			replacement,
			&result
		) != 0) {
		return -1;
	}
	if (result != body) {
		*p_result = result;
		*p_dependency_found = 1;
		return 0;
	}

	/* Generated constructor schemas can expose one typed constructor index while
	 * a result-family TYPE_VIEW stores the corresponding context-free constructor
	 * spine. The projection is derived from this exact index and its owner; it is
	 * not a search for any convertible subterm. */
	uint32_t core_index;
	int is_constructor;
	if (operation_motive_core_constructor_spine(
			ctx, index_term, &core_index, &is_constructor
		) != 0) {
		return -1;
	}
	if (!is_constructor || operation_motive_replace_certified_core_shape(
			ctx, body, core_index, replacement, &result, p_dependency_found
		) != 0) {
		*p_result = body;
		*p_dependency_found = 0;
		return !is_constructor ? 0 : -1;
	}
	*p_result = result;
	return 0;
}
