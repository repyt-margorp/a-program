#include "cwf_certificate.h"

#include "context.h"
#include "judgement.h"
#include "term.h"
#include "type_declaration.h"

static int context_certificate_is_valid(
	const struct prototype_cwf_certificate* certificate,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	const struct prototype_context* context = certificate ?
		prototype_context_get(contexts, certificate->structural_id) : NULL;
	const struct prototype_judgement_claim* claim = certificate ?
		prototype_judgement_claim_get(judgement, certificate->claim_id) : NULL;
	uint32_t classifier = prototype_context_classifier_term(context);
	uint32_t classifier_classifier;
	return certificate && contexts && terms && type_declarations && judgement &&
		certificate->kind == PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION &&
		certificate->structural_id != prototype_context_empty(contexts) &&
		context && classifier != PROTOTYPE_INVALID_ID &&
		prototype_context_classifier_variable(context) == PROTOTYPE_INVALID_ID &&
		claim && claim->proposition->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		claim->proposition->context_id == context->parent && claim->proposition->subject == classifier &&
		claim->proposition->operation_id == PROTOTYPE_INVALID_ID &&
		claim->proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION &&
		claim->proposition->classifier < terms->term_count &&
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, claim->proposition->classifier, &classifier_classifier
		) == 0 && classifier_classifier < terms->term_count &&
		terms->terms[classifier_classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

static int substitution_certificate_is_valid(
	const struct prototype_cwf_certificate* certificate,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
) {
	const struct prototype_substitution* substitution = certificate ?
		prototype_substitution_get(substitutions, certificate->structural_id) : NULL;
	const struct prototype_judgement_claim* claim = certificate ?
		prototype_judgement_claim_get(judgement, certificate->claim_id) : NULL;
	return certificate && substitutions && judgement &&
		certificate->kind == PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION &&
		substitution && claim &&
		substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
		claim->proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		claim->proposition->context_id == substitution->source_context &&
		claim->proposition->subject == substitution->term &&
		claim->proposition->classifier == substitution->term_classifier;
}

void prototype_cwf_certificate_db_init(
	struct prototype_cwf_certificate_db* db,
	struct prototype_cwf_certificate* certificates,
	size_t certificate_capacity
) {
	if (!db) {
		return;
	}
	db->certificates = certificates;
	db->certificate_count = 0;
	db->certificate_capacity = certificate_capacity;
	db->intern_requests = 0;
	db->intern_hits = 0;
}

const struct prototype_cwf_certificate* prototype_cwf_certificate_db_get(
	const struct prototype_cwf_certificate_db* db,
	uint32_t certificate_id
) {
	return db && certificate_id < db->certificate_count ?
		&db->certificates[certificate_id] : NULL;
}

const struct prototype_cwf_certificate* prototype_cwf_certificate_db_get_kind(
	const struct prototype_cwf_certificate_db* db,
	uint32_t certificate_id,
	int kind
) {
	const struct prototype_cwf_certificate* certificate =
		prototype_cwf_certificate_db_get(db, certificate_id);
	return certificate && certificate->kind == kind ? certificate : NULL;
}

static int certificate_add(
	struct prototype_cwf_certificate_db* db,
	struct prototype_cwf_certificate certificate,
	uint32_t* p_certificate_id
) {
	if (!db || !p_certificate_id || !db->certificates) {
		return -1;
	}
	db->intern_requests++;
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].kind == certificate.kind &&
			db->certificates[i].structural_id == certificate.structural_id &&
			db->certificates[i].claim_id == certificate.claim_id) {
			*p_certificate_id = i;
			db->intern_hits++;
			return 0;
		}
	}
	if (db->certificate_count >= db->certificate_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->certificate_count++;
	db->certificates[id] = certificate;
	*p_certificate_id = id;
	return 0;
}

static int certificate_is_duplicate(
	const struct prototype_cwf_certificate_db* db,
	uint32_t certificate_id
) {
	const struct prototype_cwf_certificate* certificate =
		&db->certificates[certificate_id];
	for (uint32_t i = 0; i < certificate_id; ++i) {
		if (db->certificates[i].kind == certificate->kind &&
			db->certificates[i].structural_id == certificate->structural_id &&
			db->certificates[i].claim_id == certificate->claim_id) {
			return 1;
		}
	}
	return 0;
}

int prototype_cwf_certificate_db_add_context(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t classifier_claim_id,
	uint32_t* p_certificate_id
) {
	struct prototype_cwf_certificate certificate = {
		.kind = PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION,
		.structural_id = context_id,
		.claim_id = classifier_claim_id
	};
	return context_certificate_is_valid(
		&certificate, contexts, terms, type_declarations, judgement
	) ? certificate_add(db, certificate, p_certificate_id) : -1;
}

int prototype_cwf_certificate_db_add_substitution(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t claim_id,
	uint32_t* p_certificate_id
) {
	struct prototype_cwf_certificate certificate = {
		.kind = PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION,
		.structural_id = substitution_id,
		.claim_id = claim_id
	};
	return substitution_certificate_is_valid(
		&certificate, substitutions, judgement
	) ? certificate_add(db, certificate, p_certificate_id) : -1;
}

int prototype_cwf_certificate_db_validate(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->certificate_count > db->certificate_capacity ||
		(db->certificate_count != 0 && !db->certificates)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		const struct prototype_cwf_certificate* certificate = &db->certificates[i];
		int valid = 0;
		if (certificate->kind == PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION) {
			valid = context_certificate_is_valid(
				certificate, contexts, terms, type_declarations, judgement
			);
		} else if (certificate->kind ==
				PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION) {
			valid = substitution_certificate_is_valid(
				certificate, substitutions, judgement
			);
		}
		if (!valid || certificate_is_duplicate(db, i)) {
			return -1;
		}
	}
	return 0;
}

int prototype_cwf_certificate_db_validate_contexts(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->certificate_count > db->certificate_capacity ||
		(db->certificate_count != 0 && !db->certificates)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].kind ==
				PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION &&
			(!context_certificate_is_valid(
				&db->certificates[i], contexts, terms, type_declarations, judgement
			) || certificate_is_duplicate(db, i))) {
			return -1;
		}
	}
	return 0;
}

int prototype_cwf_certificate_db_validate_substitutions(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->certificate_count > db->certificate_capacity ||
		(db->certificate_count != 0 && !db->certificates)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].kind ==
				PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION &&
			(!substitution_certificate_is_valid(
				&db->certificates[i], substitutions, judgement
			) || certificate_is_duplicate(db, i))) {
			return -1;
		}
	}
	return 0;
}

int prototype_cwf_certificate_db_has(
	const struct prototype_cwf_certificate_db* db,
	int kind,
	uint32_t structural_id
) {
	if (!db) {
		return 0;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].kind == kind &&
			db->certificates[i].structural_id == structural_id) {
			return 1;
		}
	}
	return 0;
}
