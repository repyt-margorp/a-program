#include "a_program/kernel/cwf_certificate.h"

#include "a_program/kernel/context.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

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
		claim && prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context->parent && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == classifier &&
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->occurrence_id == PROTOTYPE_INVALID_ID &&
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier < terms->term_count &&
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, &classifier_classifier
		) == 0 && classifier_classifier < terms->term_count &&
		terms->terms[classifier_classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

static int substitution_claim_is_exact(
	const struct prototype_substitution* substitution,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	const struct prototype_judgement_proposition* proposition = claim ?
		prototype_judgement_claim_proposition(judgement, claim_id) : NULL;
	return substitution && proposition &&
		proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proposition->context_id == substitution->source_context &&
		proposition->subject == substitution->term &&
		proposition->classifier == substitution->term_classifier;
}

int prototype_cwf_substitution_claim_certifies(
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t claim_id
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	return substitution && substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
		substitution_claim_is_exact(substitution, judgement, claim_id);
}

static int substitution_certificate_id_for_root(
	const struct prototype_cwf_certificate_db* db,
	uint32_t substitution_id,
	uint32_t* p_certificate_id
) {
	if (!db || !p_certificate_id) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].kind ==
				PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION &&
			db->certificates[i].structural_id == substitution_id) {
			*p_certificate_id = i;
			return 0;
		}
	}
	return -1;
}

static int substitution_certificate_is_valid_at_depth(
	const struct prototype_cwf_certificate* certificate,
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t depth
) {
	const struct prototype_substitution* substitution = certificate ?
		prototype_substitution_get(substitutions, certificate->structural_id) : NULL;
	if (!certificate || !db || !substitutions || !judgement || !substitution ||
		certificate->kind != PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION ||
		depth > substitutions->substitution_count) {
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		if (!prototype_cwf_substitution_claim_certifies(
				substitutions,
				judgement,
				certificate->structural_id,
				certificate->claim_id
			)) {
			return 0;
		}
		const struct prototype_substitution* prefix =
			prototype_substitution_get(substitutions, substitution->first);
		if (!prefix) {
			return 0;
		}
		if (prefix->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			prefix->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			uint32_t prefix_certificate_id;
			if (substitution_certificate_id_for_root(
					db, substitution->first, &prefix_certificate_id
				) != 0 || !substitution_certificate_is_valid_at_depth(
					&db->certificates[prefix_certificate_id],
					db,
					substitutions,
					judgement,
					depth + 1
				)) {
				return 0;
			}
		}
		return 1;
	}
	if (certificate->claim_id != PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (substitution->kind != PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY ||
			substitution->kind == PROTOTYPE_SUBSTITUTION_EMPTY ||
			substitution->kind == PROTOTYPE_SUBSTITUTION_PROJECTION;
	}
	const uint32_t children[2] = { substitution->first, substitution->second };
	for (uint32_t i = 0; i < 2; ++i) {
		const struct prototype_substitution* child =
			prototype_substitution_get(substitutions, children[i]);
		if (!child) {
			return 0;
		}
		if (child->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			child->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			uint32_t child_certificate_id;
			if (substitution_certificate_id_for_root(
					db, children[i], &child_certificate_id
				) != 0 || !substitution_certificate_is_valid_at_depth(
					&db->certificates[child_certificate_id],
					db,
					substitutions,
					judgement,
					depth + 1
				)) {
				return 0;
			}
		}
	}
	return 1;
}

static int substitution_certificate_is_valid(
	const struct prototype_cwf_certificate* certificate,
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
) {
	return substitution_certificate_is_valid_at_depth(
		certificate, db, substitutions, judgement, 0
	);
}

static int certify_substitution_root_at_depth(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t depth,
	uint32_t* p_certificate_id
);

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
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	struct prototype_cwf_certificate certificate = {
		.kind = PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION,
		.structural_id = substitution_id,
		.claim_id = claim_id
	};
	if (!substitution || !substitution_certificate_is_valid(
			&certificate, db, substitutions, judgement
		)) {
		/* A structurally composed prefix may not have been selected as a
		 * capability before this EXTEND. Close structural dependencies here,
		 * while refusing to invent evidence for any nested EXTEND. */
		if (!prototype_cwf_substitution_claim_certifies(
				substitutions, judgement, substitution_id, claim_id
		)) {
			return -1;
		}
		uint32_t prefix_certificate_id;
		if (certify_substitution_root_at_depth(
				db,
				substitutions,
				judgement,
				substitution->first,
				0,
				&prefix_certificate_id
			) != 0 || !substitution_certificate_is_valid(
				&certificate, db, substitutions, judgement
			)) {
			return -1;
		}
	}
	return substitution_certificate_is_valid(
		&certificate, db, substitutions, judgement
	) ? certificate_add(db, certificate, p_certificate_id) : -1;
}

static int certify_substitution_root_at_depth(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t depth,
	uint32_t* p_certificate_id
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!db || !substitution || !judgement || !p_certificate_id ||
		depth > substitutions->substitution_count) {
		return -1;
	}
	uint32_t existing;
	if (substitution_certificate_id_for_root(db, substitution_id, &existing) == 0) {
		if (!substitution_certificate_is_valid(
				&db->certificates[existing], db, substitutions, judgement
			)) {
			return -1;
		}
		*p_certificate_id = existing;
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		uint32_t child_certificate;
		if (certify_substitution_root_at_depth(
				db,
				substitutions,
				judgement,
				substitution->first,
				depth + 1,
				&child_certificate
			) != 0 || certify_substitution_root_at_depth(
				db,
				substitutions,
				judgement,
				substitution->second,
				depth + 1,
				&child_certificate
			) != 0) {
			return -1;
		}
	}
	return prototype_cwf_certificate_db_add_substitution(
		db,
		substitutions,
		judgement,
		substitution_id,
		PROTOTYPE_INVALID_ID,
		p_certificate_id
	);
}

int prototype_cwf_certificate_db_certify_substitution_root(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t* p_certificate_id
) {
	return certify_substitution_root_at_depth(
		db, substitutions, judgement, substitution_id, 0, p_certificate_id
	);
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
				certificate, db, substitutions, judgement
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
				&db->certificates[i], db, substitutions, judgement
			) || certificate_is_duplicate(db, i))) {
			return -1;
		}
	}
	return 0;
}

int prototype_cwf_certified_substitution_ref_get(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t certificate_id,
	struct prototype_certified_substitution_ref* p_ref
) {
	const struct prototype_cwf_certificate* certificate =
		prototype_cwf_certificate_db_get_kind(
			db,
			certificate_id,
			PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
		);
	if (!p_ref || !certificate || certificate->structural_id != substitution_id ||
		!substitution_certificate_is_valid(
			certificate, db, substitutions, judgement
		)) {
		return -1;
	}
	*p_ref = (struct prototype_certified_substitution_ref) {
		.substitution_id = substitution_id,
		.certificate_id = certificate_id
	};
	return 0;
}

int prototype_cwf_certificate_db_substitution_evidence(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t* p_claim_id
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	uint32_t certificate_id;
	if (!db || !substitution || !judgement || !p_claim_id ||
		substitution_certificate_id_for_root(
			db, substitution_id, &certificate_id
		) != 0 || !substitution_certificate_is_valid(
			&db->certificates[certificate_id], db, substitutions, judgement
		)) {
		return -1;
	}
	*p_claim_id = substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ?
		db->certificates[certificate_id].claim_id : PROTOTYPE_INVALID_ID;
	return 0;
}

int prototype_cwf_certificate_db_validate_substitution_roots(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	const struct prototype_certified_substitution_ref* roots,
	size_t root_count
) {
	if ((!roots && root_count != 0) || !db || !substitutions || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < root_count; ++i) {
		struct prototype_certified_substitution_ref checked;
		if (prototype_cwf_certified_substitution_ref_get(
				db,
				substitutions,
				judgement,
				roots[i].substitution_id,
				roots[i].certificate_id,
				&checked
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int accepted_substitution_root_is_covered_at_depth(
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t depth
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution || depth > substitutions->substitution_count) {
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		int exact_claim_found = 0;
		for (uint32_t i = 0; i < judgement->claim_count; ++i) {
			if (prototype_cwf_substitution_claim_certifies(
					substitutions, judgement, substitution_id, i
				)) {
				exact_claim_found = 1;
				break;
			}
		}
		return exact_claim_found && accepted_substitution_root_is_covered_at_depth(
			substitutions, judgement, substitution->first, depth + 1
		);
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return accepted_substitution_root_is_covered_at_depth(
				substitutions, judgement, substitution->first, depth + 1
			) && accepted_substitution_root_is_covered_at_depth(
				substitutions, judgement, substitution->second, depth + 1
			);
	}
	return substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY ||
		substitution->kind == PROTOTYPE_SUBSTITUTION_EMPTY ||
		substitution->kind == PROTOTYPE_SUBSTITUTION_PROJECTION;
}

int prototype_cwf_validate_accepted_semantic_action_coverage(
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
) {
	if (!substitutions || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, i);
		if (!derivation) {
			return -1;
		}
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			!accepted_substitution_root_is_covered_at_depth(
				substitutions, judgement, derivation->semantic_action_id, 0
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				!accepted_substitution_root_is_covered_at_depth(
					substitutions,
					judgement,
					derivation->premises[j].semantic_action_id,
					0
				)) {
				return -1;
			}
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
