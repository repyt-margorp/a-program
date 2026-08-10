#ifndef __PROTOTYPE_CWF_CERTIFICATE_H__
#define __PROTOTYPE_CWF_CERTIFICATE_H__

#include <stddef.h>
#include <stdint.h>

struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_judgement_db;

enum prototype_cwf_certificate_kind {
	PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION = 1,
	PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION = 2
};

struct prototype_cwf_certificate {
	int kind;
	uint32_t structural_id;
	uint32_t claim_id;
};

struct prototype_cwf_certificate_db {
	struct prototype_cwf_certificate* certificates;
	size_t certificate_count;
	size_t certificate_capacity;
	uint64_t intern_requests;
	uint64_t intern_hits;
};

void prototype_cwf_certificate_db_init(
	struct prototype_cwf_certificate_db* db,
	struct prototype_cwf_certificate* certificates,
	size_t certificate_capacity
);
const struct prototype_cwf_certificate* prototype_cwf_certificate_db_get(
	const struct prototype_cwf_certificate_db* db,
	uint32_t certificate_id
);
const struct prototype_cwf_certificate* prototype_cwf_certificate_db_get_kind(
	const struct prototype_cwf_certificate_db* db,
	uint32_t certificate_id,
	int kind
);
int prototype_cwf_certificate_db_add_context(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t classifier_claim_id,
	uint32_t* p_certificate_id
);
int prototype_cwf_certificate_db_add_substitution(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t claim_id,
	uint32_t* p_certificate_id
);
int prototype_cwf_certificate_db_validate(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
);
int prototype_cwf_certificate_db_validate_contexts(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
);
int prototype_cwf_certificate_db_validate_substitutions(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
);
int prototype_cwf_certificate_db_has(
	const struct prototype_cwf_certificate_db* db,
	int kind,
	uint32_t structural_id
);

#endif
