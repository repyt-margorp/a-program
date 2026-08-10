void prototype_hott_action_db_init(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_request* requests,
	size_t request_capacity,
	struct prototype_hott_action_certificate* certificates,
	size_t certificate_capacity,
	struct prototype_hott_action_result* results,
	size_t result_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->requests = requests;
	db->request_capacity = request_capacity;
	db->certificates = certificates;
	db->certificate_capacity = certificate_capacity;
	db->results = results;
	db->result_capacity = result_capacity;
	for (size_t i = 0; i < PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT; ++i) {
		db->request_index_heads[i] = PROTOTYPE_INVALID_ID;
	}
}

const struct prototype_hott_action_request* prototype_hott_action_request_get(
	const struct prototype_hott_action_db* db,
	uint32_t request_id
) {
	return db && request_id < db->request_count ?
		&db->requests[request_id] : NULL;
}

const struct prototype_hott_action_result* prototype_hott_action_result_get(
	const struct prototype_hott_action_db* db,
	uint32_t result_id
) {
	return db && result_id < db->result_count ? &db->results[result_id] : NULL;
}

static int hott_term_has_compiler_relation_head(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t head = term_id;
	for (uint32_t depth = 0; depth <= terms->term_count; ++depth) {
		const struct prototype_term* term = &terms->terms[head];
		if (term->tag != PROTOTYPE_TERM_APP) {
			return term->tag == PROTOTYPE_TERM_RELATION_TYPE_FORMER ||
				term->tag == PROTOTYPE_TERM_RELATION_WITNESS_FORMER;
		}
		if (term->as.app.function >= terms->term_count) {
			return -1;
		}
		head = term->as.app.function;
	}
	return -1;
}

