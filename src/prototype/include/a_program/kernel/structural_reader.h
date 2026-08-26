#ifndef A_PROGRAM_PROTOTYPE_KERNEL_STRUCTURAL_READER_H
#define A_PROGRAM_PROTOTYPE_KERNEL_STRUCTURAL_READER_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/term.h"

struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_term_db;

enum prototype_structural_context_extension_kind {
	PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_INVALID = 0,
	PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_VALUE = 1,
	PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_SEQUENCE_RESULT = 2
};

struct prototype_context_structural_record {
	uint32_t parent;
	uint32_t binding_id;
	uint32_t classifier;
	int extension_kind;
	uint32_t producer_computation;
};

struct prototype_context_structural_reader {
	const void* state;
	size_t count;
	int (*read)(
		const void* state,
		uint32_t context_id,
		struct prototype_context_structural_record* p_record
	);
};

enum prototype_structural_substitution_kind {
	PROTOTYPE_STRUCTURAL_SUBSTITUTION_IDENTITY = 1,
	PROTOTYPE_STRUCTURAL_SUBSTITUTION_EMPTY = 2,
	PROTOTYPE_STRUCTURAL_SUBSTITUTION_PROJECTION = 3,
	PROTOTYPE_STRUCTURAL_SUBSTITUTION_EXTEND = 4,
	PROTOTYPE_STRUCTURAL_SUBSTITUTION_COMPOSE = 5
};

struct prototype_substitution_structural_record {
	int kind;
	uint32_t source_context;
	uint32_t target_context;
	uint32_t first;
	uint32_t second;
	uint32_t term;
	uint32_t term_classifier;
};

struct prototype_substitution_structural_reader {
	const void* state;
	size_t count;
	int (*read)(
		const void* state,
		uint32_t substitution_id,
		struct prototype_substitution_structural_record* p_record
	);
};

struct prototype_term_structural_ih_scope {
	uint32_t match_term;
	uint32_t scrutinee_binding_id;
};

struct prototype_term_structural_reader {
	const void* state;
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t ih_scope_count;
	size_t fold_clause_count;
	int (*read_term)(
		const void* state,
		uint32_t term_id,
		const struct prototype_term** p_term
	);
	int (*read_case)(
		const void* state,
		uint32_t case_id,
		const struct prototype_match_case** p_case
	);
	int (*read_case_binder)(
		const void* state,
		uint32_t binder_id,
		const struct prototype_case_binder** p_binder
	);
	int (*read_ih_scope)(
		const void* state,
		uint32_t scope_id,
		struct prototype_term_structural_ih_scope* p_scope
	);
	int (*read_fold_clause)(
		const void* state,
		uint32_t clause_id,
		const struct prototype_computation_fold_clause** p_clause
	);
};

enum prototype_substitution_structural_image_kind {
	PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_VARIABLE = 1,
	PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_TERM = 2
};

struct prototype_substitution_structural_image {
	int kind;
	uint32_t binding_id;
	uint32_t term;
};

int prototype_term_structural_reader_from_db(
	const struct prototype_term_db* db,
	struct prototype_term_structural_reader* p_reader
);

int prototype_term_structural_read(
	const struct prototype_term_structural_reader* reader,
	uint32_t term_id,
	const struct prototype_term** p_term
);

int prototype_term_structural_pure_family_parts(
	const struct prototype_term_structural_reader* reader,
	uint32_t family_id,
	uint32_t* p_binding_id,
	uint32_t* p_body_id
);

int prototype_term_structural_pi_parts(
	const struct prototype_term_structural_reader* reader,
	uint32_t pi_id,
	uint32_t* p_domain_id,
	uint32_t* p_binding_id,
	uint32_t* p_body_id
);

int prototype_context_structural_reader_from_db(
	const struct prototype_context_db* db,
	struct prototype_context_structural_reader* p_reader
);

int prototype_substitution_structural_reader_from_db(
	const struct prototype_substitution_db* db,
	struct prototype_substitution_structural_reader* p_reader
);

int prototype_context_structural_read(
	const struct prototype_context_structural_reader* reader,
	uint32_t context_id,
	struct prototype_context_structural_record* p_record
);

int prototype_context_structural_validate(
	const struct prototype_context_structural_reader* reader,
	size_t term_count
);

int prototype_context_structural_find_binding(
	const struct prototype_context_structural_reader* reader,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_entry_context_id,
	uint32_t* p_classifier
);

int prototype_context_structural_is_ancestor(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant
);

int prototype_context_structural_path(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant,
	uint32_t* path,
	size_t path_capacity,
	size_t* p_count
);

int prototype_context_structural_path_length(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant,
	size_t* p_count
);

int prototype_substitution_structural_read(
	const struct prototype_substitution_structural_reader* reader,
	uint32_t substitution_id,
	struct prototype_substitution_structural_record* p_record
);

int prototype_substitution_structural_validate(
	const struct prototype_substitution_structural_reader* substitutions,
	const struct prototype_context_structural_reader* contexts,
	size_t term_count
);

/* Returns zero for a structural image, one when composition/empty requires
 * term interpretation, and minus one for malformed input. */
int prototype_substitution_structural_binding_image(
	const struct prototype_substitution_structural_reader* substitutions,
	const struct prototype_context_structural_reader* contexts,
	uint32_t substitution_id,
	uint32_t binding_id,
	struct prototype_substitution_structural_image* p_image
);

#endif
