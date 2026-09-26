#ifndef A_PROGRAM_PROTOTYPE_CHECKER_SESSION_H
#define A_PROGRAM_PROTOTYPE_CHECKER_SESSION_H

#include <stdint.h>

#include "a_program/checker/module.h"
#include "a_program/kernel/resource_usage.h"
#include "a_program/producer/effort.h"

enum prototype_checker_status {
	PROTOTYPE_CHECKER_COMPLETE = 1,
	PROTOTYPE_CHECKER_PAUSED = 2,
	PROTOTYPE_CHECKER_REJECTED = 3
};

enum prototype_checker_stop_reason {
	PROTOTYPE_CHECKER_STOP_NONE = 0,
	PROTOTYPE_CHECKER_STOP_EFFORT = 1,
	PROTOTYPE_CHECKER_STOP_UNSUPPORTED = 2,
	PROTOTYPE_CHECKER_STOP_MALFORMED = 3,
	PROTOTYPE_CHECKER_STOP_CONTEXT = 4,
	PROTOTYPE_CHECKER_STOP_SUBSTITUTION = 5,
	PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA = 6,
	PROTOTYPE_CHECKER_STOP_OCCURRENCE = 7,
	PROTOTYPE_CHECKER_STOP_CLASSIFIER = 8,
	PROTOTYPE_CHECKER_STOP_INTERFACE = 9,
	PROTOTYPE_CHECKER_STOP_RESOURCE_USAGE = 10,
	PROTOTYPE_CHECKER_STOP_UNIVERSE = 11
};

struct prototype_checker_options {
	struct prototype_effort_account* effort;
	/* Checked bases are capabilities minted by earlier checker runs. They are
	 * consulted only to validate exact external interfaces; their opaque term
	 * bodies are never imported or unfolded. */
	const struct prototype_checked_module* const* imported_bases;
	size_t imported_base_count;
};

struct prototype_checker_report {
	int status;
	int stop_reason;
	uint64_t effort_used;
	uint32_t subject;
};

/* A checked module is a process-local capability. Its representation is
 * private and no numeric artifact value can manufacture one. */
struct prototype_checked_module;
struct prototype_checked_export_ref;

enum prototype_checked_export_kind {
	PROTOTYPE_CHECKED_EXPORT_TERM = 1,
	PROTOTYPE_CHECKED_EXPORT_TYPE = 2,
	PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR = 3
};

int prototype_checker_check_module(
	const struct prototype_elaborated_module_view* module,
	const struct prototype_checker_options* options,
	struct prototype_checked_module** p_checked,
	struct prototype_checker_report* p_report
);

void prototype_checked_module_destroy(
	struct prototype_checked_module* checked
);

const struct prototype_elaborated_module_view*
prototype_checked_module_elaborated_view(
	const struct prototype_checked_module* checked
);

int prototype_checked_module_occurrence_usage(
	const struct prototype_checked_module* checked,
	uint32_t occurrence_id,
	const struct prototype_usage_entry** p_entries,
	size_t* p_entry_count,
	int* p_binder_usage
);

size_t prototype_checked_module_export_count(
	const struct prototype_checked_module* checked,
	int kind
);

const struct prototype_checked_export_ref* prototype_checked_module_export_at(
	const struct prototype_checked_module* checked,
	int kind,
	size_t index
);

const struct prototype_semantic_term_export* prototype_checked_term_export(
	const struct prototype_checked_export_ref* export_ref
);

const struct prototype_semantic_type_export* prototype_checked_type_export(
	const struct prototype_checked_export_ref* export_ref
);

const struct prototype_semantic_constructor_export*
prototype_checked_constructor_export(
	const struct prototype_checked_export_ref* export_ref
);

#endif
