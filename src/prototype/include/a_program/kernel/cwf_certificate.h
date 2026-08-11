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

/* A capability for using one candidate Substitution as a CwF morphism. The
 * certificate must name this exact root. Its recursive validation covers every
 * EXTEND in the root's prefix/composition closure. */
struct prototype_certified_substitution_ref {
	uint32_t substitution_id;
	uint32_t certificate_id;
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
/* A Claim certifies EXTEND only when it is the exact accepted source-context
 * HAS_TYPE proposition. Its authority may be a Derivation, Operation, or Name;
 * CwF formation must not collapse those distinct acceptance mechanisms. */
int prototype_cwf_substitution_claim_certifies(
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t claim_id
);
/* For EXTEND, claim_id must be its exact accepted HAS_TYPE evidence. The API
 * closes structural prefix certificates, but never synthesizes evidence for a
 * nested EXTEND. */
/* Creates the structural root certificate when needed. EXTEND evidence is
 * never invented: an exact accepted Claim certificate must already exist. */
int prototype_cwf_certificate_db_certify_substitution_root(
	struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
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
int prototype_cwf_certified_substitution_ref_get(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t certificate_id,
	struct prototype_certified_substitution_ref* p_ref
);
/* Returns the exact Claim selected by a valid EXTEND capability. Structural
 * substitutions have no Claim evidence and return PROTOTYPE_INVALID_ID. */
int prototype_cwf_certificate_db_substitution_evidence(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	uint32_t substitution_id,
	uint32_t* p_claim_id
);
int prototype_cwf_certificate_db_validate_substitution_roots(
	const struct prototype_cwf_certificate_db* db,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	const struct prototype_certified_substitution_ref* roots,
	size_t root_count
);
/* Artifact readback has accepted Claims but intentionally no compiler-local
 * certificate store. This reconstructs the same EXTEND coverage from every
 * accepted Derivation semantic-action root. */
int prototype_cwf_validate_accepted_semantic_action_coverage(
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
);
int prototype_cwf_certificate_db_has(
	const struct prototype_cwf_certificate_db* db,
	int kind,
	uint32_t structural_id
);

#endif
