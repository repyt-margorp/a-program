#include "a_program/artifact/wire_v85.h"

#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/kernel/cwf_certificate.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artifact_graph_internal.h"
#define A_PROGRAM_ARTIFACT_DEFINE_RUNTIME_CAPABILITY_HELPER
#include "artifact_internal.h"
#undef A_PROGRAM_ARTIFACT_DEFINE_RUNTIME_CAPABILITY_HELPER

static int read_artifact_term_key(
	FILE* stream,
	struct prototype_term_canonical_key* key
) {
	unsigned long long hash;
	if (fscanf(
			stream,
			"%llu %u %u %u %d %d %d %d",
			&hash,
			&key->node_count,
			&key->bound_binder_count,
			&key->free_binder_count,
			&key->has_frame_local_reference,
			&key->has_type_local_reference,
			&key->has_type_name_reference,
			&key->has_type_universe_reference
		) != 8) {
		return -1;
	}
	key->hash = (uint64_t)hash;
	return 0;
}

static int read_artifact_type_representation_fingerprint(
	FILE* stream,
	struct prototype_type_representation_fingerprint* key
) {
	unsigned long long hash;
	if (fscanf(
			stream,
			"%llu %u %u %u %u %u %d %d",
			&hash,
			&key->node_count,
			&key->parameter_count,
			&key->constructor_count,
			&key->bound_binder_count,
			&key->free_binder_count,
			&key->has_local_universe_reference,
			&key->has_name_reference
		) != 8) {
		return -1;
	}
	key->hash = (uint64_t)hash;
	return 0;
}

static int read_artifact_type_expr(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_id,
	uint32_t* p_next_level_var
);

static int function_graph_export_name_matches(
	const struct symbol_table* symbols,
	int symbol_id,
	const char* prefix,
	const char* owner_name
) {
	char expected[256];
	const char* actual = symbol_to_string(symbols, symbol_id);
	int length;
	if (!symbols || !prefix || !owner_name || !actual) {
		return 0;
	}
	length = snprintf(expected, sizeof(expected), "$%s.%s", prefix, owner_name);
	return length >= 0 && (size_t)length < sizeof(expected) &&
		strcmp(actual, expected) == 0;
}

static int validate_function_graph_association_names(
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_artifact_function_graph_association* association
) {
	const struct prototype_artifact_term_export* owner;
	const struct prototype_artifact_type_export* graph;
	const struct prototype_artifact_type_export* result;
	const struct prototype_artifact_term_export* graph_interface;
	const struct prototype_artifact_term_export* runner;
	const char* owner_name;
	if (!symbols || !interface || !association ||
		association->owner_term_export_index >= interface->term_export_count ||
		association->graph_type_export_index >= interface->type_export_count ||
		association->result_type_export_index >= interface->type_export_count ||
		association->graph_interface_term_export_index >=
			interface->term_export_count ||
		association->certified_runner_term_export_index >=
			interface->term_export_count) {
		return -1;
	}
	owner = &interface->term_exports[association->owner_term_export_index];
	graph = &interface->type_exports[association->graph_type_export_index];
	result = &interface->type_exports[association->result_type_export_index];
	graph_interface = &interface->term_exports[
		association->graph_interface_term_export_index
	];
	runner = &interface->term_exports[
		association->certified_runner_term_export_index
	];
	owner_name = symbol_to_string(symbols, owner->name_symbol_id);
	if (!owner_name ||
		graph->namespace_symbol_id != owner->namespace_symbol_id ||
		result->namespace_symbol_id != owner->namespace_symbol_id ||
		graph_interface->namespace_symbol_id != owner->namespace_symbol_id ||
		runner->namespace_symbol_id != owner->namespace_symbol_id ||
		!function_graph_export_name_matches(
			symbols, graph->name_symbol_id, "graph", owner_name
		) ||
		!function_graph_export_name_matches(
			symbols, result->name_symbol_id, "result", owner_name
		) ||
		!function_graph_export_name_matches(
			symbols, graph_interface->name_symbol_id, "interface", owner_name
		) ||
		!function_graph_export_name_matches(
			symbols, runner->name_symbol_id, "certified", owner_name
		)) {
		return -1;
	}
	if (association->certified_adapter_term_export_index != PROTOTYPE_INVALID_ID) {
		if (association->certified_adapter_term_export_index >=
				interface->term_export_count) {
			return -1;
		}
		const struct prototype_artifact_term_export* adapter =
			&interface->term_exports[
				association->certified_adapter_term_export_index
			];
		if (adapter->namespace_symbol_id != owner->namespace_symbol_id ||
			!function_graph_export_name_matches(
				symbols, adapter->name_symbol_id, "adapter", owner_name
			)) {
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_read_text_interface(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_artifact_interface* interface
) {
	if (!stream || !symbols || !intrinsic_environment || !interface) {
		return -1;
	}

	char word[256];
	char fingerprint[65];
	int version;
	if (fscanf(stream, "%255s %d %64s", word, &version, fingerprint) != 3 ||
		strcmp(word, "A_PROGRAM_ARTIFACT") != 0 ||
		version != PROTOTYPE_ARTIFACT_FORMAT_VERSION ||
		strcmp(fingerprint, PROTOTYPE_ARTIFACT_CALCULUS_FINGERPRINT) != 0) {
		return -1;
	}
	unsigned long long intrinsic_fingerprint;
	int default_integer_host_type;
	if (fscanf(
			stream,
			"%255s %llu %d",
			word,
			&intrinsic_fingerprint,
			&default_integer_host_type
		) != 3 || strcmp(word, "intrinsic_environment") != 0 ||
		(uint64_t)intrinsic_fingerprint !=
			prototype_intrinsic_environment_fingerprint(intrinsic_environment) ||
		default_integer_host_type !=
			intrinsic_environment->default_integer_host_type) {
		return -1;
	}
	interface->intrinsic_environment_fingerprint =
		(uint64_t)intrinsic_fingerprint;
	interface->default_integer_host_type = default_integer_host_type;
	if (fscanf(stream, "%255s", word) != 1 || strcmp(word, "SECTION") != 0 ||
		fscanf(stream, "%255s", word) != 1 || strcmp(word, "interface") != 0) {
		return -1;
	}

	interface->term_export_count = 0;
	interface->type_export_count = 0;
	interface->type_parameter_count = 0;
	interface->constructor_export_count = 0;
	interface->constructor_field_type_expr_count = 0;
	interface->type_expr_count = 0;
	interface->identity_root_count = 0;
	interface->dependency_count = 0;

	size_t count;
	size_t slot_count;
	size_t present_count;
	char label_slots[32];
	char label_present[32];
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_type_exprs") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "type_exprs") != 0 ||
		slot_count > interface->type_expr_capacity) {
		return -1;
	}
	memset(interface->type_exprs, 0, sizeof(*interface->type_exprs) * slot_count);
	struct prototype_type_declaration_db interface_type_expr_db;
	memset(&interface_type_expr_db, 0, sizeof(interface_type_expr_db));
	interface_type_expr_db.readback.exprs = interface->type_exprs;
	interface_type_expr_db.readback.expr_capacity = interface->type_expr_capacity;
	interface_type_expr_db.readback.expr_count = slot_count;
	uint32_t next_level_var = 0;
	for (size_t i = 0; i < present_count; ++i) {
		if (read_artifact_type_expr(
				stream,
				symbols,
				&interface_type_expr_db,
				PROTOTYPE_INVALID_ID,
				&next_level_var
			) != 0) {
			return -1;
		}
	}
	interface->type_expr_count = slot_count;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_type_parameters") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "parameters") != 0 ||
		slot_count > interface->type_parameter_capacity) {
		return -1;
	}
	for (size_t i = 0; i < slot_count; ++i) {
		interface->type_parameters[i].binding_id = PROTOTYPE_INVALID_ID;
		interface->type_parameters[i].name_symbol_id = -1;
		interface->type_parameters[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < present_count; ++i) {
		char name[256];
		size_t id;
		uint32_t binding_id;
		uint32_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %u",
				word,
				&id,
				&binding_id,
				name,
				&type_expr
			) != 5 ||
			strcmp(word, "interface_type_parameter") != 0 ||
			id >= slot_count ||
			type_expr >= interface->type_expr_count ||
			!artifact_type_expr_present(&interface->type_exprs[type_expr])) {
			return -1;
		}
		struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[id];
		parameter->binding_id = binding_id;
		parameter->type_expr = type_expr;
		parameter->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (parameter->name_symbol_id < 0) {
			return -1;
		}
	}
	interface->type_parameter_count = slot_count;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu",
			word,
			label_slots,
			&slot_count,
			label_present,
			&present_count
		) != 5 ||
		strcmp(word, "interface_constructor_field_refs") != 0 ||
		strcmp(label_slots, "slots") != 0 ||
		strcmp(label_present, "field_refs") != 0 ||
		slot_count > interface->constructor_field_type_expr_capacity) {
		return -1;
	}
	for (size_t i = 0; i < slot_count; ++i) {
		interface->constructor_field_type_exprs[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < present_count; ++i) {
		size_t id;
		uint32_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %u",
				word,
				&id,
				&type_expr
			) != 3 ||
			strcmp(word, "interface_constructor_field_ref") != 0 ||
			id >= slot_count ||
			type_expr >= interface->type_expr_count ||
			!artifact_type_expr_present(&interface->type_exprs[type_expr])) {
			return -1;
		}
		interface->constructor_field_type_exprs[id] = type_expr;
	}
	interface->constructor_field_type_expr_count = slot_count;

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "term_exports") != 0 ||
		count > interface->term_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		char occurrence_word[256];
		char evidence_word[256];
		struct prototype_artifact_term_export* export =
			&interface->term_exports[interface->term_export_count++];
		if (fscanf(
				stream,
				"%255s %255s %u %u %d",
				word,
				name,
				&export->local_term,
				&export->classifier,
				&export->transparency
			) != 5 ||
			strcmp(word, "term") != 0) {
			return -1;
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0 ||
			read_artifact_term_key(stream, &export->canonical_key) != 0 ||
			read_artifact_term_key(stream, &export->classifier_key) != 0 ||
			fscanf(
				stream,
				"%255s %255s %255s %u %255s %d %u %u",
				namespace_word,
				namespace_name,
				occurrence_word,
				&export->occurrence,
				evidence_word,
				&export->source_evidence.kind,
				&export->source_evidence.id,
				&export->source_condition_count
			) != 8 ||
			strcmp(namespace_word, "namespace") != 0 ||
			strcmp(occurrence_word, "occurrence") != 0 ||
			strcmp(evidence_word, "evidence") != 0) {
			return -1;
		}
		export->namespace_symbol_id = symbol_intern(
			symbols,
			namespace_name,
			strlen(namespace_name)
		);
		if (export->namespace_symbol_id < 0) {
			return -1;
		}
		if (export->source_evidence.kind ==
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM) {
			if (export->source_evidence.id == PROTOTYPE_INVALID_ID ||
				export->source_condition_count != 0) {
				return -1;
			}
			export->source_condition_first = 0;
		} else if (export->source_evidence.kind ==
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CONDITIONAL) {
			if (export->source_condition_count == 0 ||
				export->source_condition_count >
					PROTOTYPE_ARTIFACT_EXPORT_CONDITION_CAPACITY -
						interface->export_condition_obligation_count) {
				return -1;
			}
			export->source_condition_first =
				(uint32_t)interface->export_condition_obligation_count;
			for (uint32_t j = 0; j < export->source_condition_count; ++j) {
				uint32_t obligation_id;
				if (fscanf(stream, "%u", &obligation_id) != 1 ||
					(j != 0 && obligation_id <=
					 interface->export_condition_obligation_ids[
						export->source_condition_first + j - 1
					 ])) {
					return -1;
				}
				interface->export_condition_obligation_ids[
					interface->export_condition_obligation_count++
				] = obligation_id;
			}
		} else {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "type_exports") != 0 ||
		count > interface->type_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		struct prototype_artifact_type_export* export =
			&interface->type_exports[interface->type_export_count++];
		if (fscanf(
				stream,
				"%255s %255s %u %u %u %u %u %u %u",
				word,
				name,
				&export->local_type_id,
				&export->core_representation_anchor_type_id,
				&export->formation_classifier,
				&export->first_parameter,
				&export->parameter_count,
				&export->first_constructor_export,
				&export->constructor_count
			) != 9 ||
			strcmp(word, "type") != 0 ||
			export->first_parameter + export->parameter_count >
				interface->type_parameter_count) {
			return -1;
		}
		for (uint32_t j = 0; j < export->parameter_count; ++j) {
			if (!artifact_interface_parameter_present(
					&interface->type_parameters[export->first_parameter + j]
				)) {
				return -1;
			}
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0 ||
			read_artifact_type_representation_fingerprint(stream, &export->representation_fingerprint) != 0 ||
			fscanf(stream, "%255s %255s", namespace_word, namespace_name) != 2 ||
			strcmp(namespace_word, "namespace") != 0) {
			return -1;
		}
		export->namespace_symbol_id = symbol_intern(
			symbols,
			namespace_name,
			strlen(namespace_name)
		);
		if (export->namespace_symbol_id < 0) {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "constructor_exports") != 0 ||
		count > interface->constructor_export_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[interface->constructor_export_count++];
		if (fscanf(
				stream,
				"%255s %u %255s %u %u %u %u",
				word,
				&export->type_export_index,
				name,
				&export->ordinal,
				&export->readback_first_field_type,
				&export->readback_field_count,
				&export->constructor_classifier
			) != 7 ||
			strcmp(word, "constructor") != 0 ||
			(export->readback_field_count > 0 &&
				(export->readback_first_field_type == PROTOTYPE_INVALID_ID ||
					export->readback_first_field_type +
						export->readback_field_count >
						interface->constructor_field_type_expr_count))) {
			return -1;
		}
		export->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (export->name_symbol_id < 0) {
			return -1;
		}
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "function_graph_associations") != 0 ||
		count > PROTOTYPE_ARTIFACT_FUNCTION_GRAPH_ASSOCIATION_CAPACITY) {
		return -1;
	}
	interface->function_graph_association_count = 0;
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_artifact_function_graph_association* association =
			&interface->function_graph_associations[
				interface->function_graph_association_count
			];
		if (fscanf(
				stream,
					"%255s %zu %u %u %u %u %u %u %u",
				word,
				&id,
				&association->owner_term_export_index,
				&association->graph_type_export_index,
				&association->result_type_export_index,
				&association->graph_interface_term_export_index,
				&association->certified_adapter_term_export_index,
					&association->certified_runner_term_export_index,
					&association->certified_argument_index
				) != 9 || strcmp(word, "function_graph_association") != 0 ||
			id != interface->function_graph_association_count ||
			association->owner_term_export_index >= interface->term_export_count ||
			association->graph_type_export_index >= interface->type_export_count ||
			association->result_type_export_index >= interface->type_export_count ||
			association->graph_interface_term_export_index >=
				interface->term_export_count ||
			(association->certified_adapter_term_export_index !=
				PROTOTYPE_INVALID_ID &&
			 association->certified_adapter_term_export_index >=
				interface->term_export_count) ||
			association->certified_runner_term_export_index >=
				interface->term_export_count ||
			validate_function_graph_association_names(
				symbols, interface, association
			) != 0) {
			return -1;
		}
		for (size_t j = 0; j < interface->function_graph_association_count; ++j) {
			if (interface->function_graph_associations[j].owner_term_export_index ==
					association->owner_term_export_index) {
				return -1;
			}
		}
		interface->function_graph_association_count++;
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "identity_roots") != 0 ||
		count > interface->identity_root_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_artifact_identity_root* root =
			&interface->identity_roots[interface->identity_root_count];
		if (fscanf(
				stream,
				"%255s %zu %u %u %u %d",
				word,
				&id,
				&root->source_type_claim_id,
				&root->identity_family_has_type_claim_id,
				&root->witness_has_type_claim_id,
				&root->computation_rule
			) != 6 || strcmp(word, "identity_root") != 0 ||
			id != interface->identity_root_count) {
			return -1;
		}
		for (size_t j = 0; j < interface->identity_root_count; ++j) {
			if (interface->identity_roots[j].source_type_claim_id ==
					root->source_type_claim_id &&
				interface->identity_roots[j].identity_family_has_type_claim_id ==
					root->identity_family_has_type_claim_id &&
				interface->identity_roots[j].witness_has_type_claim_id ==
					root->witness_has_type_claim_id &&
				interface->identity_roots[j].computation_rule ==
					root->computation_rule) {
				return -1;
			}
		}
		interface->identity_root_count++;
	}

	if (fscanf(stream, "%255s %zu", word, &count) != 2 ||
		strcmp(word, "dependencies") != 0 ||
		count > interface->dependency_capacity) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		char namespace_word[256];
		char namespace_name[256];
		struct prototype_artifact_dependency* dependency =
			&interface->dependencies[interface->dependency_count++];
		if (fscanf(
				stream,
				"%255s %255s %255s %255s",
				word,
				name,
				namespace_word,
				namespace_name
			) != 4 ||
			strcmp(word, "dependency") != 0 ||
			strcmp(namespace_word, "namespace") != 0) {
			return -1;
		}
		dependency->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (strcmp(namespace_name, "-") == 0) {
			dependency->namespace_symbol_id = -1;
		} else {
			dependency->namespace_symbol_id = symbol_intern(
				symbols,
				namespace_name,
				strlen(namespace_name)
			);
			if (dependency->namespace_symbol_id < 0) {
				return -1;
			}
		}
		if (dependency->name_symbol_id < 0) {
			return -1;
		}
	}
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "interface") != 0) {
		return -1;
	}
	return 0;
}

static int expect_artifact_count(FILE* stream, const char* expected, size_t* p_count) {
	char word[256];
	if (!stream || !expected || !p_count ||
		fscanf(stream, "%255s %zu", word, p_count) != 2 ||
		strcmp(word, expected) != 0) {
		return -1;
	}
	return 0;
}

static int read_artifact_symbol(FILE* stream, struct symbol_table* symbols, int* p_symbol_id) {
	char name[256];
	if (!stream || !symbols || !p_symbol_id ||
		fscanf(stream, "%255s", name) != 1) {
		return -1;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	*p_symbol_id = symbol_id;
	return 0;
}

static int read_artifact_optional_symbol(FILE* stream, struct symbol_table* symbols, int* p_symbol_id) {
	char name[256];
	if (!stream || !symbols || !p_symbol_id ||
		fscanf(stream, "%255s", name) != 1) {
		return -1;
	}
	if (strcmp(name, "-") == 0) {
		*p_symbol_id = -1;
		return 0;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	*p_symbol_id = symbol_id;
	return 0;
}

static int read_artifact_type_expr(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_id,
	uint32_t* p_next_level_var
) {
	char word[256];
	uint32_t expr_id;
	int tag;
	if (!stream || !symbols || !type_declarations || !p_next_level_var ||
		fscanf(stream, "%255s %u %d", word, &expr_id, &tag) != 3 ||
		strcmp(word, "type_expr") != 0 ||
		(expected_id != PROTOTYPE_INVALID_ID && expr_id != expected_id) ||
		expr_id >= type_declarations->readback.expr_capacity) {
		return -1;
	}

	struct prototype_type_expr* expr = &type_declarations->readback.exprs[expr_id];
	if (artifact_type_expr_present(expr)) {
		return -1;
	}
	memset(expr, 0, sizeof(*expr));
	expr->tag = tag;
	switch (tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			return fscanf(stream, "%u", &expr->as.universe.level) == 1 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			if (fscanf(stream, "%u", &expr->as.universe_var.level_var) != 1) {
				return -1;
			}
			if (*p_next_level_var <= expr->as.universe_var.level_var) {
				*p_next_level_var = expr->as.universe_var.level_var + 1;
			}
			return 0;
		case PROTOTYPE_TYPE_EXPR_SELF:
			return 0;
		case PROTOTYPE_TYPE_EXPR_VAR:
			return fscanf(stream, "%u", &expr->as.var.binding_id) == 1 &&
				read_artifact_symbol(stream, symbols, &expr->as.var.symbol_id) == 0 ? 0 : -1;
			case PROTOTYPE_TYPE_EXPR_NAME:
				return read_artifact_symbol(stream, symbols, &expr->as.name.symbol_id);
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
				if (read_artifact_optional_symbol(
						stream,
						symbols,
						&expr->as.imported_type.name.namespace_symbol_id
					) != 0 ||
					read_artifact_symbol(stream, symbols, &expr->as.imported_type.name.name_symbol_id) != 0) {
				return -1;
			}
				return read_artifact_type_representation_fingerprint(stream, &expr->as.imported_type.representation_fingerprint);
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				return read_artifact_optional_symbol(
					stream,
					symbols,
					&expr->as.external_term.name.namespace_symbol_id
				) == 0 &&
					read_artifact_symbol(stream, symbols, &expr->as.external_term.name.name_symbol_id) == 0 ? 0 : -1;
			case PROTOTYPE_TYPE_EXPR_LOCAL_TYPE_MEMBER:
				return read_artifact_symbol(
						stream, symbols, &expr->as.local_type_member.owner_symbol_id
					) == 0 && read_artifact_symbol(
						stream, symbols, &expr->as.local_type_member.member_symbol_id
					) == 0 ? 0 : -1;
			case PROTOTYPE_TYPE_EXPR_COMPUTATION_REFERENCE:
				return fscanf(
					stream, "%u", &expr->as.computation_reference.result
				) == 1 ? 0 : -1;
			case PROTOTYPE_TYPE_EXPR_SEMANTIC_RELATION:
				return 0;
		case PROTOTYPE_TYPE_EXPR_APP:
			return fscanf(stream, "%u %u", &expr->as.app.function, &expr->as.app.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return fscanf(stream, "%u %u", &expr->as.arrow.domain, &expr->as.arrow.codomain) == 2 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_PI:
			return fscanf(stream, "%u", &expr->as.pi.binding_id) == 1 &&
				read_artifact_symbol(stream, symbols, &expr->as.pi.symbol_id) == 0 &&
				fscanf(stream, "%u %u", &expr->as.pi.domain, &expr->as.pi.codomain) == 2 ? 0 : -1;
		default:
			return -1;
	}
}

static int read_artifact_term(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_dimension_operator_db* dimension_operators,
	struct prototype_term_db* terms,
	uint32_t expected_id,
	uint32_t* p_next_binder_id
) {
	char word[256];
	uint32_t term_id;
	int tag;
	if (!stream || !symbols || !intrinsic_environment || !dimension_operators ||
		!terms ||
		!p_next_binder_id ||
		fscanf(stream, "%255s %u %d", word, &term_id, &tag) != 3 ||
		strcmp(word, "term_node") != 0 ||
		(expected_id != PROTOTYPE_INVALID_ID && term_id != expected_id) ||
		term_id >= terms->term_capacity) {
		return -1;
	}

	struct prototype_term* term = &terms->terms[term_id];
	if (artifact_term_present(term)) {
		return -1;
	}
	memset(term, 0, sizeof(*term));
	term->tag = tag;
	switch (tag) {
		case PROTOTYPE_TERM_VAR:
			if (fscanf(stream, "%u", &term->as.var.binding_id) != 1) {
				return -1;
			}
			if (term->as.var.binding_id != PROTOTYPE_INVALID_ID &&
				*p_next_binder_id <= term->as.var.binding_id) {
				*p_next_binder_id = term->as.var.binding_id + 1;
			}
			return 0;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return fscanf(stream, "%u %u", &term->as.constructor.owner, &term->as.constructor.constructor_id) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_APP:
			return fscanf(stream, "%u %u", &term->as.app.function, &term->as.app.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_LAMBDA:
			if (fscanf(stream, "%u %u", &term->as.lambda.binding_id, &term->as.lambda.body) != 2) {
				return -1;
			}
			if (term->as.lambda.binding_id != PROTOTYPE_INVALID_ID &&
				*p_next_binder_id <= term->as.lambda.binding_id) {
				*p_next_binder_id = term->as.lambda.binding_id + 1;
			}
			return 0;
		case PROTOTYPE_TERM_PI:
			return fscanf(stream, "%u %u", &term->as.pi.domain, &term->as.pi.codomain_family) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			return fscanf(
				stream,
				"%u %u %u %u",
				&term->as.match.scrutinee,
				&term->as.match.first_case,
				&term->as.match.case_count,
				&term->as.match.ih_scope_id
			) == 4 ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return fscanf(stream, "%u", &term->as.type_former.representation_id) == 1 ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			{
				char namespace_name[256];
				char name[256];
				if (fscanf(
						stream,
						"%u %255s %255s",
						&term->as.type_declaration.type_id,
						namespace_name,
						name
					) != 3) {
					return -1;
				}
				term->as.type_declaration.identity.namespace_symbol_id =
					strcmp(namespace_name, "-") == 0 ? -1 :
					symbol_intern(symbols, namespace_name, strlen(namespace_name));
				term->as.type_declaration.identity.name_symbol_id =
					strcmp(name, "-") == 0 ? -1 :
					symbol_intern(symbols, name, strlen(name));
				return term->as.type_declaration.identity.namespace_symbol_id >= -1 &&
					term->as.type_declaration.identity.name_symbol_id >= -1 ? 0 : -1;
			}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			char namespace_name[256];
			char name[256];
			if (fscanf(
					stream,
					"%u %255s %255s %u %u",
					&term->as.type_view.view_type_id,
					namespace_name,
					name,
					&term->as.type_view.core,
					&term->as.type_view.source
				) != 5) {
				return -1;
			}
			term->as.type_view.identity.namespace_symbol_id =
				strcmp(namespace_name, "-") == 0 ? -1 :
				symbol_intern(symbols, namespace_name, strlen(namespace_name));
			term->as.type_view.identity.name_symbol_id =
				strcmp(name, "-") == 0 ? -1 :
				symbol_intern(symbols, name, strlen(name));
			return term->as.type_view.identity.namespace_symbol_id >= -1 &&
				term->as.type_view.identity.name_symbol_id >= -1 ? 0 : -1;
		}
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return fscanf(stream, "%u %u", &term->as.induction_hypothesis.ih_scope_id, &term->as.induction_hypothesis.argument) == 2 ? 0 : -1;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return fscanf(stream, "%u", &term->as.universe_var.level_var) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return read_artifact_symbol(stream, symbols, &term->as.text_literal.text_symbol_id);
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				return fscanf(stream, "%" SCNd64, &term->as.int_literal.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return read_artifact_optional_symbol(
					stream,
					symbols,
					&term->as.external_ref.name.namespace_symbol_id
				) == 0 &&
					read_artifact_symbol(stream, symbols, &term->as.external_ref.name.name_symbol_id) == 0 ? 0 : -1;
			case PROTOTYPE_TERM_PURE_PRIMITIVE:
				{
					int symbol_id;
					if (read_artifact_symbol(stream, symbols, &symbol_id) != 0 ||
					read_artifact_optional_symbol(stream, symbols, &term->as.pure_primitive.type_symbol_id) != 0) {
						return -1;
					}
					struct prototype_intrinsic_namespace_binding binding;
					if (prototype_intrinsic_namespace_lookup(
							intrinsic_environment,
							symbol_to_string(symbols, symbol_id),
							&binding
						) != 0 ||
						binding.kind != PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE) {
						return -1;
					}
					term->as.pure_primitive.primitive_id = binding.target_id;
					return 0;
				}
			case PROTOTYPE_TERM_EFFECT_OPERATION:
				{
					int symbol_id;
					if (read_artifact_symbol(stream, symbols, &symbol_id) != 0 ||
						fscanf(stream, "%u", &term->as.effect_operation.classifier) != 1) {
						return -1;
					}
					struct prototype_intrinsic_namespace_binding binding;
					if (prototype_intrinsic_namespace_lookup(
							intrinsic_environment,
							symbol_to_string(symbols, symbol_id),
							&binding
						) != 0 ||
						binding.kind != PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION) {
						return -1;
					}
					term->as.effect_operation.operation_id = binding.target_id;
					return 0;
				}
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				return 0;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				if (fscanf(stream, "%u", &term->as.effect_row_var.binding_id) != 1) {
					return -1;
				}
				if (term->as.effect_row_var.binding_id != PROTOTYPE_INVALID_ID &&
					*p_next_binder_id <= term->as.effect_row_var.binding_id) {
					*p_next_binder_id = term->as.effect_row_var.binding_id + 1;
				}
				return 0;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return fscanf(stream, "%u %u", &term->as.effect_row_union.left,
					&term->as.effect_row_union.right) == 2 ? 0 : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				if (fscanf(stream, "%u %u", &term->as.effect_row_forall.binding_id,
						&term->as.effect_row_forall.body) != 2) {
					return -1;
				}
				if (term->as.effect_row_forall.binding_id != PROTOTYPE_INVALID_ID &&
					*p_next_binder_id <= term->as.effect_row_forall.binding_id) {
					*p_next_binder_id = term->as.effect_row_forall.binding_id + 1;
				}
				return 0;
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION: {
				int symbol_id;
				struct prototype_intrinsic_namespace_binding binding;
				if (read_artifact_symbol(stream, symbols, &symbol_id) != 0 ||
					fscanf(stream, "%u", &term->as.effect_row_operation.latent_row) != 1 ||
					prototype_intrinsic_namespace_lookup(
						intrinsic_environment,
						symbol_to_string(symbols, symbol_id), &binding
					) != 0 || binding.kind !=
						PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION) {
					return -1;
				}
				term->as.effect_row_operation.operation_id = binding.target_id;
				return 0;
			}
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				return fscanf(
					stream,
					"%u %u %d",
					&term->as.computation_type.label,
					&term->as.computation_type.result,
					&term->as.computation_type.totality
				) == 3 ? 0 : -1;
			case PROTOTYPE_TERM_THUNK_TYPE:
				return fscanf(stream, "%u", &term->as.thunk_type.computation) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_RETURN:
				return fscanf(stream, "%u", &term->as.return_term.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_THUNK:
				return fscanf(stream, "%u", &term->as.thunk.computation) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_FORCE:
				return fscanf(stream, "%u", &term->as.force.value) == 1 ? 0 : -1;
			case PROTOTYPE_TERM_COMPUTATION_FOLD: {
				uint32_t clause_count;
				if (fscanf(stream, "%u %u %u", &term->as.computation_fold.computation,
						&term->as.computation_fold.return_clause, &clause_count
					) != 3 || terms->computation_fold_clause_count + clause_count >
						PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY) {
					return -1;
				}
				term->as.computation_fold.first_clause = (uint32_t)terms->computation_fold_clause_count;
				term->as.computation_fold.clause_count = clause_count;
				for (uint32_t i = 0; i < clause_count; ++i) {
					struct prototype_computation_fold_clause* clause =
						&terms->computation_fold_clauses[terms->computation_fold_clause_count++];
					if (fscanf(stream, "%u %u", &clause->operation, &clause->body) != 2) {
						return -1;
					}
				}
				return 0;
			}
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				return fscanf(stream, "%u %u %u", &term->as.operation_request.operation,
					&term->as.operation_request.argument,
					&term->as.operation_request.continuation) == 3 ? 0 : -1;
			case PROTOTYPE_TERM_DIMENSION_ACTION: {
				uint32_t source_dimension;
				uint32_t target_dimension;
				size_t image_count;
				if (fscanf(
						stream,
						"%u %u %u %zu",
						&term->as.dimension_action.source,
						&source_dimension,
						&target_dimension,
						&image_count
					) != 4 || image_count != source_dimension || image_count >
						dimension_operators->image_capacity) {
					return -1;
				}
				struct prototype_dimension_axis_image* images = image_count == 0 ?
					NULL : calloc(image_count, sizeof(*images));
				if (image_count != 0 && !images) {
					return -1;
				}
				int status = 0;
				for (size_t i = 0; i < image_count; ++i) {
					if (fscanf(
							stream,
							"%d %u",
							&images[i].kind,
							&images[i].target_axis
						) != 2) {
						status = -1;
						break;
					}
				}
				if (status == 0 && prototype_dimension_operator_intern(
						dimension_operators,
						source_dimension,
						target_dimension,
						images,
						image_count,
						&term->as.dimension_action.operator_id
					) != 0) {
					status = -1;
				}
				free(images);
				return status;
			}
			case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
			case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
				return 0;
			default:
				return -1;
		}
	}

static int artifact_range_within(uint32_t first, uint32_t count, size_t slot_count) {
	if (count == 0) {
		return first <= slot_count;
	}
	if (first > UINT32_MAX - count) {
		return 0;
	}
	return (size_t)first + count <= slot_count;
}

static int artifact_read_type_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id
) {
	return type_declarations &&
		type_id < type_declarations->semantic_schema.type_count &&
		artifact_type_present(&type_declarations->semantic_schema.type_declarations[type_id]);
}

static int artifact_read_parameter_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_id
) {
	return type_declarations &&
		parameter_id < type_declarations->readback.parameter_count &&
		artifact_parameter_present(&type_declarations->readback.parameter_declarations[parameter_id]);
}

static int artifact_read_constructor_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t constructor_id
) {
	return type_declarations &&
		constructor_id < type_declarations->semantic_schema.constructor_count &&
		artifact_constructor_present(&type_declarations->semantic_schema.constructor_declarations[constructor_id]);
}

static int artifact_read_type_expr_present(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t expr_id
) {
	return type_declarations &&
		expr_id < type_declarations->readback.expr_count &&
		artifact_type_expr_present(&type_declarations->readback.exprs[expr_id]);
}

static int artifact_read_term_present(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	return terms &&
		term_id < terms->term_count &&
		artifact_term_present(&terms->terms[term_id]);
}

static int artifact_read_case_present(
	const struct prototype_term_db* terms,
	uint32_t case_id
) {
	return terms &&
		case_id < terms->case_count &&
		artifact_case_present(&terms->cases[case_id]);
}

static int artifact_read_case_binder_present(
	const struct prototype_term_db* terms,
	uint32_t binding_id
) {
	return terms &&
		binding_id < terms->case_binder_count &&
		artifact_case_binder_present(&terms->case_binders[binding_id]);
}

static int artifact_read_frame_present(
	const struct prototype_term_db* terms,
	uint32_t ih_scope_id
) {
	return terms &&
		ih_scope_id < terms->ih_scope_count &&
		artifact_frame_present(&terms->ih_scopes[ih_scope_id]);
}

static int artifact_validate_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t expr_id
) {
	if (!artifact_read_type_expr_present(type_declarations, expr_id)) {
		return -1;
	}
	const struct prototype_type_expr* expr = &type_declarations->readback.exprs[expr_id];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
		case PROTOTYPE_TYPE_EXPR_SELF:
			case PROTOTYPE_TYPE_EXPR_VAR:
			case PROTOTYPE_TYPE_EXPR_NAME:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
			case PROTOTYPE_TYPE_EXPR_LOCAL_TYPE_MEMBER:
			case PROTOTYPE_TYPE_EXPR_SEMANTIC_RELATION:
				return 0;
		case PROTOTYPE_TYPE_EXPR_COMPUTATION_REFERENCE:
			return artifact_read_type_expr_present(
				type_declarations, expr->as.computation_reference.result
			) ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_APP:
			return artifact_read_type_expr_present(type_declarations, expr->as.app.function) &&
				artifact_read_type_expr_present(type_declarations, expr->as.app.argument) ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return artifact_read_type_expr_present(type_declarations, expr->as.arrow.domain) &&
				artifact_read_type_expr_present(type_declarations, expr->as.arrow.codomain) ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_PI:
			return artifact_read_type_expr_present(type_declarations, expr->as.pi.domain) &&
				artifact_read_type_expr_present(type_declarations, expr->as.pi.codomain) ? 0 : -1;
		default:
			return -1;
	}
}

static int artifact_validate_type_graph_refs(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms
) {
	if (!type_declarations || !terms) {
		return -1;
	}
	for (size_t i = 0; i < type_declarations->semantic_schema.type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[i];
		const struct prototype_type_readback_entry* readback_entry =
			&type_declarations->readback.type_entries[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		if (type->type_index != i ||
			type->formation_classifier == PROTOTYPE_INVALID_ID ||
			!artifact_read_term_present(terms, type->formation_classifier) ||
			!artifact_range_within(
				readback_entry->first_parameter,
				type->parameter_count,
				type_declarations->readback.parameter_count
			) ||
			!artifact_range_within(
				type->first_constructor,
				type->constructor_count,
				type_declarations->semantic_schema.constructor_count
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < type->parameter_count; ++j) {
			if (!artifact_read_parameter_present(
					type_declarations,
					readback_entry->first_parameter + j
				)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			if (!artifact_read_constructor_present(
					type_declarations,
					type->first_constructor + j
				)) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < type_declarations->readback.parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->readback.parameter_declarations[i];
		if (!artifact_parameter_present(parameter)) {
			continue;
		}
		if (!artifact_read_type_expr_present(type_declarations, parameter->type_expr)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->semantic_schema.constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[i];
		const struct prototype_constructor_classifier_cache_entry* cache =
			prototype_type_constructor_classifier_cache_get(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache, (uint32_t)i
			);
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (!cache || !artifact_read_type_present(type_declarations, constructor->owner_type) ||
			constructor->result_classifier == PROTOTYPE_INVALID_ID ||
			!artifact_read_term_present(terms, constructor->result_classifier) ||
			cache->classifier == PROTOTYPE_INVALID_ID ||
			!artifact_read_term_present(terms, cache->classifier)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.field_type_count; ++i) {
		if (!artifact_field_type_present(&type_declarations->readback.field_types[i])) {
			continue;
		}
		if (!artifact_read_type_expr_present(
				type_declarations,
				type_declarations->readback.field_types[i]
			)) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		if (!artifact_type_expr_present(&type_declarations->readback.exprs[i])) {
			continue;
		}
		if (artifact_validate_type_expr_refs(type_declarations, (uint32_t)i) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_validate_term_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	int representation_handles_resolved
) {
	if (!artifact_read_term_present(terms, term_id) || !type_declarations) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
			case PROTOTYPE_TERM_VAR:
				case PROTOTYPE_TERM_UNIVERSE_VAR:
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
			case PROTOTYPE_TERM_INT_LITERAL:
				case PROTOTYPE_TERM_EXTERNAL_REF:
				case PROTOTYPE_TERM_PURE_PRIMITIVE:
				case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				case PROTOTYPE_TERM_EFFECT_ROW_VAR:
					return 0;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return artifact_read_term_present(
				terms, term->as.effect_operation.classifier
			) ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return artifact_read_term_present(terms, term->as.effect_row_union.left) &&
				artifact_read_term_present(terms, term->as.effect_row_union.right) ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_read_term_present(terms, term->as.effect_row_forall.body) ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return prototype_term_effect_operation_declaration(
					term->as.effect_row_operation.operation_id
				) && artifact_read_term_present(
					terms, term->as.effect_row_operation.latent_row
				) ? 0 : -1;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_read_term_present(terms, term->as.constructor.owner) ? 0 : -1;
		case PROTOTYPE_TERM_APP:
			return artifact_read_term_present(terms, term->as.app.function) &&
				artifact_read_term_present(terms, term->as.app.argument) ? 0 : -1;
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_read_term_present(terms, term->as.lambda.body) ? 0 : -1;
		case PROTOTYPE_TERM_PI:
			return artifact_read_term_present(terms, term->as.pi.domain) &&
				artifact_read_term_present(terms, term->as.pi.codomain_family) ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			if (!artifact_read_term_present(terms, term->as.match.scrutinee) ||
				!artifact_range_within(
					term->as.match.first_case,
					term->as.match.case_count,
					terms->case_count
				) ||
				(term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID &&
					!artifact_read_frame_present(terms, term->as.match.ih_scope_id))) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				if (!artifact_read_case_present(terms, term->as.match.first_case + i)) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			if (representation_handles_resolved) {
				if (term->as.type_former.representation_id >=
					type_declarations->representation_db.representation_count) {
					return -1;
				}
				return term->as.type_former.constructor_count ==
					type_declarations->representation_db.representations[
						term->as.type_former.representation_id
					].fingerprint.constructor_count ? 0 : -1;
			}
			return artifact_read_type_present(
				type_declarations,
				term->as.type_former.representation_id
			) ? 0 : -1;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (!artifact_read_type_present(
					type_declarations, term->as.type_declaration.type_id
				)) {
				return -1;
			}
			{
				const struct prototype_type_declaration* type =
					&type_declarations->semantic_schema.type_declarations[
						term->as.type_declaration.type_id
					];
				return type->namespace_symbol_id ==
						term->as.type_declaration.identity.namespace_symbol_id &&
					type->name_symbol_id ==
						term->as.type_declaration.identity.name_symbol_id ? 0 : -1;
			}
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (!artifact_read_type_present(
					type_declarations, term->as.type_view.view_type_id
				) || !artifact_read_term_present(terms, term->as.type_view.core) ||
				!artifact_read_term_present(terms, term->as.type_view.source)) {
				return -1;
			}
			{
				const struct prototype_type_declaration* type =
					&type_declarations->semantic_schema.type_declarations[term->as.type_view.view_type_id];
				return type->namespace_symbol_id ==
						term->as.type_view.identity.namespace_symbol_id &&
					type->name_symbol_id == term->as.type_view.identity.name_symbol_id ?
					0 : -1;
			}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_read_frame_present(terms, term->as.induction_hypothesis.ih_scope_id) &&
					artifact_read_term_present(terms, term->as.induction_hypothesis.argument) ? 0 : -1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return artifact_read_term_present(terms, term->as.computation_type.label) &&
				artifact_read_term_present(terms, term->as.computation_type.result) &&
				(term->as.computation_type.totality ==
					PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ||
				 term->as.computation_type.totality ==
					PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE) ? 0 : -1;
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_read_term_present(terms, term->as.thunk_type.computation) ? 0 : -1;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (!artifact_read_term_present(terms, term->as.computation_fold.computation) ||
				!artifact_read_term_present(terms, term->as.computation_fold.return_clause) ||
				(size_t)term->as.computation_fold.first_clause + term->as.computation_fold.clause_count >
					terms->computation_fold_clause_count) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (!artifact_read_term_present(terms, clause->operation) ||
					!artifact_read_term_present(terms, clause->body)) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return artifact_read_term_present(terms, term->as.operation_request.operation) &&
				artifact_read_term_present(terms, term->as.operation_request.argument) &&
				artifact_read_term_present(terms, term->as.operation_request.continuation) ? 0 : -1;
		case PROTOTYPE_TERM_RETURN:
			return artifact_read_term_present(terms, term->as.return_term.value) ? 0 : -1;
		case PROTOTYPE_TERM_THUNK:
			return artifact_read_term_present(terms, term->as.thunk.computation) ? 0 : -1;
		case PROTOTYPE_TERM_FORCE:
			return artifact_read_term_present(terms, term->as.force.value) ? 0 : -1;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return artifact_read_term_present(
				terms, term->as.dimension_action.source
			) && prototype_dimension_operator_get(
				dimension_operators, term->as.dimension_action.operator_id
			) ? 0 : -1;
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 0;
			default:
				return -1;
	}
}

static int artifact_validate_term_graph_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	int representation_handles_resolved
) {
	if (!terms || !type_declarations || !dimension_operators) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (artifact_validate_term_refs(
				terms,
				type_declarations,
				dimension_operators,
				(uint32_t)i,
				representation_handles_resolved
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		if (!artifact_case_present(match_case)) {
			continue;
		}
		if (!artifact_read_term_present(terms, match_case->body) ||
			(match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
				!artifact_read_term_present(terms, match_case->constructor_owner)) ||
			!artifact_range_within(
				match_case->first_binder,
				match_case->binder_count,
				terms->case_binder_count
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
			if (!artifact_read_case_binder_present(terms, match_case->first_binder + j)) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		const struct prototype_ih_scope* frame = &terms->ih_scopes[i];
		if (!artifact_frame_present(frame)) {
			continue;
		}
		if (!artifact_read_term_present(terms, frame->match_term)) {
			return -1;
		}
		const struct prototype_term* match = &terms->terms[frame->match_term];
		if (match->tag != PROTOTYPE_TERM_MATCH ||
			!artifact_read_term_present(terms, match->as.match.scrutinee)) {
			return -1;
		}
		const struct prototype_term* scrutinee =
			&terms->terms[match->as.match.scrutinee];
		if (scrutinee->tag == PROTOTYPE_TERM_VAR &&
			(frame->scrutinee_binding_id == PROTOTYPE_INVALID_ID ||
			 scrutinee->as.var.binding_id != frame->scrutinee_binding_id)) {
			return -1;
		}
	}
	return 0;
}

/* TYPE_FORMER ids are serialized as declaration anchors and rebound locally. */
static int artifact_resolve_representation_handles(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		struct prototype_term* term = &terms->terms[i];
		if (!artifact_term_present(term) || term->tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t representative_type_id = term->as.type_former.representation_id;
		if (representative_type_id >= type_declarations->semantic_schema.type_count ||
			!artifact_type_present(&type_declarations->semantic_schema.type_declarations[representative_type_id])) {
			return -1;
		}
		uint32_t representation_id =
			type_declarations->semantic_schema.type_declarations[representative_type_id].representation_id;
		if (representation_id == PROTOTYPE_INVALID_ID ||
			representation_id >= type_declarations->representation_db.representation_count) {
			return -1;
		}
		term->as.type_former.declaration_type_id = representative_type_id;
		term->as.type_former.representation_id = representation_id;
		term->as.type_former.constructor_count =
			type_declarations->semantic_schema.type_declarations[
				representative_type_id
			].constructor_count;
	}
	return 0;
}

static int artifact_validate_judgement_graph_refs(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return -1;
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (!artifact_candidate_claim_present(relation)) {
			continue;
		}
		if (!artifact_read_term_present(terms, relation->subject) ||
			!artifact_read_term_present(terms, relation->classifier)) {
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->derivation_candidate_count; ++i) {
		const struct prototype_judgement_derivation_candidate* proof = &judgement->derivation_candidates[i];
		if (!artifact_candidate_derivation_present(proof)) {
			continue;
		}
		if (proof->conclusion_proposition_id >=
				judgement->proposition_count ||
			!artifact_candidate_claim_present(
				&judgement->propositions[proof->conclusion_proposition_id]
			) ||
			!artifact_read_term_present(terms, proof->conclusion->subject) ||
			!artifact_read_term_present(terms, proof->conclusion->classifier) ||
			proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		if (proof->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			if (!artifact_read_term_present(terms, proof->rule_data.induction.match) ||
				!artifact_read_term_present(terms, proof->rule_data.induction.motive)) {
				return -1;
			}
		} else if ((proof->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
			proof->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) &&
			!artifact_read_term_present(
				terms, proof->rule_data.constructor.owner_view
			)) {
			return -1;
		}
		const struct prototype_judgement_proposition* conclusion =
			&judgement->propositions[proof->conclusion_proposition_id];
		if (conclusion->kind != proof->conclusion->kind ||
			conclusion->context_id != proof->conclusion->context_id ||
			conclusion->occurrence_id != proof->conclusion->occurrence_id ||
			conclusion->subject != proof->conclusion->subject ||
			conclusion->classifier != proof->conclusion->classifier) {
			return -1;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (!artifact_read_term_present(terms, proof->premises[j].proposition->subject) ||
				!artifact_read_term_present(terms, proof->premises[j].proposition->classifier)) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_validate_read_graph_refs(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement,
	int representation_handles_resolved
) {
	return artifact_validate_type_graph_refs(type_declarations, terms) == 0 &&
		artifact_validate_term_graph_refs(
			terms,
			type_declarations,
			dimension_operators,
			representation_handles_resolved
		) == 0 &&
		artifact_validate_judgement_graph_refs(judgement, terms) == 0 ? 0 : -1;
}

static int artifact_read_universe_node_present(
	const struct prototype_universe_db* universe,
	uint32_t node_id
) {
	return universe &&
		node_id < universe->node_count &&
		artifact_universe_node_present(&universe->nodes[node_id]);
}

static int artifact_validate_read_universe_refs(
	const struct prototype_universe_db* universe
) {
	if (!universe) {
		return -1;
	}
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (!artifact_universe_node_present(node)) {
			continue;
		}
		if (node->tag != PROTOTYPE_UNIVERSE_NODE_TYPE &&
			node->tag != PROTOTYPE_UNIVERSE_NODE_PARAMETER) {
			return -1;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (!artifact_universe_edge_present(edge)) {
			continue;
		}
		if (edge->tag != PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE ||
			!artifact_read_universe_node_present(universe, edge->from_node) ||
			!artifact_read_universe_node_present(universe, edge->to_node)) {
			return -1;
		}
	}
	return 0;
}

void prototype_internal_sync_artifact_universe_level_counters(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return;
	}
	uint32_t next_level_var = type_declarations->readback.next_level_var;
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		if (term->tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			term->as.universe_var.level_var >= next_level_var) {
			next_level_var = term->as.universe_var.level_var + 1;
		}
	}
	if (type_declarations->readback.next_level_var < next_level_var) {
		type_declarations->readback.next_level_var = next_level_var;
	}
	if (judgement->next_universe_var < next_level_var) {
		judgement->next_universe_var = next_level_var;
	}
}

int prototype_artifact_read_text_graph(
	FILE* stream,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_dimension_operator_db* dimension_operators,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !intrinsic_environment || !dimension_operators ||
		!terms ||
		!type_declarations || !judgement) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "graph") != 0) {
		return -1;
	}

	char label_terms[32];
	char label_cases[32];
	char label_case_binders[32];
	char label_frames[32];
	char label_types[32];
	char label_parameters[32];
	char label_constructors[32];
	char label_field_types[32];
	char label_type_exprs[32];
	char label_propositions[32];
	char label_claims[32];
	char label_derivations[32];
	size_t term_slot_count;
	size_t case_slot_count;
	size_t case_binder_slot_count;
	size_t frame_slot_count;
	size_t type_slot_count;
	size_t parameter_slot_count;
	size_t constructor_slot_count;
	size_t field_type_slot_count;
	size_t expr_slot_count;
	size_t proposition_slot_count;
	size_t claim_slot_count;
	size_t derivation_slot_count;
	if (fscanf(
			stream,
			"%255s"
			" %31s %zu %31s %zu %31s %zu %31s %zu"
			" %31s %zu %31s %zu %31s %zu %31s %zu"
			" %31s %zu %31s %zu %31s %zu %31s %zu",
			word,
			label_terms,
			&term_slot_count,
			label_cases,
			&case_slot_count,
			label_case_binders,
			&case_binder_slot_count,
			label_frames,
			&frame_slot_count,
			label_types,
			&type_slot_count,
			label_parameters,
			&parameter_slot_count,
			label_constructors,
			&constructor_slot_count,
			label_field_types,
			&field_type_slot_count,
			label_type_exprs,
			&expr_slot_count,
			label_propositions,
			&proposition_slot_count,
			label_claims,
			&claim_slot_count,
			label_derivations,
			&derivation_slot_count
		) != 25 ||
		strcmp(word, "counts") != 0 ||
		strcmp(label_terms, "terms") != 0 ||
		strcmp(label_cases, "cases") != 0 ||
		strcmp(label_case_binders, "case_binders") != 0 ||
		strcmp(label_frames, "frames") != 0 ||
		strcmp(label_types, "types") != 0 ||
		strcmp(label_parameters, "parameters") != 0 ||
		strcmp(label_constructors, "constructors") != 0 ||
		strcmp(label_field_types, "field_types") != 0 ||
		strcmp(label_type_exprs, "type_exprs") != 0 ||
		strcmp(label_propositions, "propositions") != 0 ||
		strcmp(label_claims, "claims") != 0 ||
		strcmp(label_derivations, "derivations") != 0) {
		return -1;
	}
	if (term_slot_count > terms->term_capacity ||
		case_slot_count > terms->case_capacity ||
		case_binder_slot_count > terms->case_binder_capacity ||
		frame_slot_count > terms->ih_scope_capacity ||
		type_slot_count > type_declarations->semantic_schema.type_capacity ||
		parameter_slot_count > type_declarations->readback.parameter_capacity ||
		constructor_slot_count > type_declarations->semantic_schema.constructor_capacity ||
		field_type_slot_count > type_declarations->readback.field_type_capacity ||
		expr_slot_count > type_declarations->readback.expr_capacity ||
		proposition_slot_count > judgement->proposition_capacity ||
		claim_slot_count > judgement->claim_capacity ||
		derivation_slot_count > judgement->derivation_candidate_capacity ||
		derivation_slot_count > judgement->derivation_capacity) {
		return -1;
	}

	size_t count;
	for (size_t i = 0; i < type_slot_count; ++i) {
		type_declarations->semantic_schema.type_declarations[i].name_symbol_id = -1;
		type_declarations->semantic_schema.type_declarations[i].namespace_symbol_id = -1;
		type_declarations->semantic_schema.type_declarations[i].type_index = PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.type_declarations[i].formation_classifier =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.type_declarations[i].parameter_context =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.type_declarations[i].index_context =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.type_declarations[i].index_count = 0;
		type_declarations->readback.type_entries[i].first_parameter =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.type_declarations[i].first_constructor = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < parameter_slot_count; ++i) {
		type_declarations->readback.parameter_declarations[i].binding_id = PROTOTYPE_INVALID_ID;
		type_declarations->readback.parameter_declarations[i].name_symbol_id = -1;
		type_declarations->readback.parameter_declarations[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < constructor_slot_count; ++i) {
		type_declarations->semantic_schema.constructor_declarations[i].name_symbol_id = -1;
		type_declarations->semantic_schema.constructor_declarations[i].owner_type = PROTOTYPE_INVALID_ID;
		type_declarations->readback.constructor_readbacks[i].first_field_type =
			PROTOTYPE_INVALID_ID;
		type_declarations->readback.constructor_readbacks[i].result_type =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.constructor_declarations[i].parameter_context =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.constructor_declarations[i].field_context =
			PROTOTYPE_INVALID_ID;
		type_declarations->semantic_schema.constructor_declarations[i].result_classifier =
			PROTOTYPE_INVALID_ID;
		type_declarations->constructor_classifier_cache.entries[i].classifier =
			PROTOTYPE_INVALID_ID;
		type_declarations->constructor_classifier_cache.entries[i].schema_revision = 0;
	}
	for (size_t i = 0; i < field_type_slot_count; ++i) {
		type_declarations->readback.field_types[i] = PROTOTYPE_INVALID_ID;
	}
	memset(type_declarations->readback.exprs, 0, sizeof(*type_declarations->readback.exprs) * expr_slot_count);

	if (expect_artifact_count(stream, "type_declarations", &count) != 0 ||
		count != type_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		char namespace_name[256];
		uint32_t type_index;
		uint32_t first_parameter;
		uint32_t parameter_count;
		uint32_t first_constructor;
		uint32_t constructor_count;
		uint32_t formation_classifier;
		uint32_t parameter_context;
		uint32_t index_context;
		uint32_t index_count;
		if (fscanf(
				stream,
				"%255s %zu %255s %255s %u %u %u %u %u %u %u %u %u",
				word,
				&id,
				name,
				namespace_name,
				&type_index,
				&first_parameter,
				&parameter_count,
				&first_constructor,
				&constructor_count,
				&formation_classifier,
				&parameter_context,
				&index_context,
				&index_count
			) != 13 ||
				strcmp(word, "type_decl") != 0 ||
				id >= type_slot_count ||
				type_index != id ||
				formation_classifier >= term_slot_count ||
				parameter_context == PROTOTYPE_INVALID_ID ||
				index_context == PROTOTYPE_INVALID_ID ||
				!artifact_range_within(first_parameter, parameter_count, parameter_slot_count) ||
				!artifact_range_within(first_constructor, constructor_count, constructor_slot_count)) {
				return -1;
			}
		struct prototype_type_declaration* type = &type_declarations->semantic_schema.type_declarations[id];
		if (artifact_type_present(type)) {
			return -1;
		}
		type->name_symbol_id = strcmp(name, "-") == 0 ? -1 :
			symbol_intern(symbols, name, strlen(name));
		type->namespace_symbol_id = strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (type->namespace_symbol_id < -1 || type->name_symbol_id < 0) {
			return -1;
		}
		type->type_index = type_index;
		type->formation_classifier = formation_classifier;
		type->parameter_context = parameter_context;
		type->index_context = index_context;
		type->index_count = index_count;
		type_declarations->readback.type_entries[id].first_parameter =
			first_parameter;
		type->parameter_count = parameter_count;
		type->first_constructor = first_constructor;
		type->constructor_count = constructor_count;
	}
	type_declarations->semantic_schema.type_count = type_slot_count;

	if (expect_artifact_count(stream, "type_parameters", &count) != 0 ||
		count != parameter_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		uint32_t binding_id;
		uint32_t type_expr;
		if (fscanf(stream, "%255s %zu %u %255s %u", word, &id, &binding_id, name, &type_expr) != 5 ||
			strcmp(word, "type_param") != 0 ||
			id >= parameter_slot_count ||
			type_expr >= expr_slot_count) {
			return -1;
		}
		struct prototype_type_parameter_declaration* parameter =
			&type_declarations->readback.parameter_declarations[id];
		if (artifact_parameter_present(parameter)) {
			return -1;
		}
		parameter->binding_id = binding_id;
		parameter->type_expr = type_expr;
		parameter->name_symbol_id = symbol_intern(symbols, name, strlen(name));
		if (parameter->name_symbol_id < 0) {
			return -1;
		}
	}
	type_declarations->readback.parameter_count = parameter_slot_count;

	if (expect_artifact_count(stream, "type_constructors", &count) != 0 ||
		count != constructor_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		uint32_t owner_type;
		uint32_t constructor_index;
		uint32_t first_field_type;
		uint32_t field_count;
		uint32_t result_type;
		uint32_t parameter_context;
		uint32_t field_context;
		uint32_t result_classifier;
		uint32_t constructor_classifier;
		if (fscanf(
				stream,
				"%255s %zu %255s %u %u %u %u %u %u %u %u %u",
				word,
				&id,
				name,
				&owner_type,
				&constructor_index,
				&first_field_type,
				&field_count,
				&result_type,
				&parameter_context,
				&field_context,
				&result_classifier,
				&constructor_classifier
			) != 12 ||
			strcmp(word, "type_constructor") != 0 ||
				id >= constructor_slot_count ||
				owner_type >= type_slot_count ||
				(field_count > 0 &&
					(first_field_type == PROTOTYPE_INVALID_ID ||
					!artifact_range_within(
						first_field_type, field_count, field_type_slot_count
					))) ||
				(result_type != PROTOTYPE_INVALID_ID && result_type >= expr_slot_count) ||
				result_classifier >= term_slot_count ||
				(constructor_classifier != PROTOTYPE_INVALID_ID &&
					constructor_classifier >= term_slot_count)) {
			return -1;
		}
		struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[id];
		if (artifact_constructor_present(constructor)) {
			return -1;
		}
		constructor->name_symbol_id = strcmp(name, "-") == 0 ? -1 :
			symbol_intern(symbols, name, strlen(name));
		if (constructor->name_symbol_id < -1) {
			return -1;
		}
		constructor->owner_type = owner_type;
		constructor->constructor_index = constructor_index;
		constructor->schema_revision = 1;
		type_declarations->readback.constructor_readbacks[id] =
			(struct prototype_type_constructor_readback){
				.first_field_type = first_field_type,
				.field_count = field_count,
				.result_type = result_type
			};
		constructor->parameter_context = parameter_context;
		constructor->field_context = field_context;
		constructor->result_classifier = result_classifier;
		type_declarations->constructor_classifier_cache.entries[id] =
			(struct prototype_constructor_classifier_cache_entry){
				.classifier = constructor_classifier,
				.schema_revision = constructor->schema_revision
			};
	}
	type_declarations->semantic_schema.constructor_count = constructor_slot_count;

	if (expect_artifact_count(stream, "type_field_refs", &count) != 0 ||
		count != field_type_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		uint32_t type_expr;
		if (fscanf(stream, "%255s %zu %u", word, &id, &type_expr) != 3 ||
			strcmp(word, "type_field_ref") != 0 ||
			id >= field_type_slot_count ||
			type_expr >= expr_slot_count) {
			return -1;
		}
		if (artifact_field_type_present(&type_declarations->readback.field_types[id])) {
			return -1;
		}
		type_declarations->readback.field_types[id] = type_expr;
	}
	type_declarations->readback.field_type_count = field_type_slot_count;

	if (expect_artifact_count(stream, "type_exprs", &count) != 0 ||
		count != expr_slot_count) {
		return -1;
	}
	uint32_t next_level_var = 0;
	for (size_t i = 0; i < count; ++i) {
		if (read_artifact_type_expr(
				stream,
				symbols,
				type_declarations,
				PROTOTYPE_INVALID_ID,
				&next_level_var
			) != 0) {
			return -1;
		}
	}
	type_declarations->readback.expr_count = expr_slot_count;
	type_declarations->readback.next_level_var = next_level_var;

	memset(terms->terms, 0, sizeof(*terms->terms) * term_slot_count);
	memset(terms->cases, 0, sizeof(*terms->cases) * case_slot_count);
	memset(terms->case_binders, 0, sizeof(*terms->case_binders) * case_binder_slot_count);
	memset(terms->ih_scopes, 0, sizeof(*terms->ih_scopes) * frame_slot_count);
	for (size_t i = 0; i < case_slot_count; ++i) {
		terms->cases[i].constructor_owner = PROTOTYPE_INVALID_ID;
		terms->cases[i].first_binder = PROTOTYPE_INVALID_ID;
		terms->cases[i].body = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < case_binder_slot_count; ++i) {
		terms->case_binders[i].binding_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < frame_slot_count; ++i) {
		terms->ih_scopes[i].match_term = PROTOTYPE_INVALID_ID;
		terms->ih_scopes[i].scrutinee_binding_id = PROTOTYPE_INVALID_ID;
	}

	if (expect_artifact_count(stream, "terms", &count) != 0 || count != term_slot_count) {
		return -1;
	}
	uint32_t next_binding_id = 0;
	for (size_t i = 0; i < count; ++i) {
		if (read_artifact_term(
					stream,
					symbols,
					intrinsic_environment,
					dimension_operators,
					terms,
					PROTOTYPE_INVALID_ID,
					&next_binding_id
				) != 0) {
			return -1;
		}
	}
	terms->term_count = term_slot_count;

	if (expect_artifact_count(stream, "match_cases", &count) != 0 || count != case_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char name[256];
		struct prototype_match_case read_case;
		if (fscanf(
				stream,
				"%255s %zu %255s %u %u %u %u %u",
				word,
				&id,
				name,
				&read_case.constructor_owner,
				&read_case.constructor_id,
				&read_case.first_binder,
				&read_case.binder_count,
				&read_case.body
			) != 8 ||
				strcmp(word, "match_case") != 0 ||
				id >= case_slot_count) {
				return -1;
			}
			if (artifact_case_present(&terms->cases[id])) {
				return -1;
			}
			terms->cases[id] = read_case;
			terms->case_label_symbols[id] = symbol_intern(symbols, name, strlen(name));
			if (terms->case_label_symbols[id] < 0) {
			return -1;
		}
	}
	terms->case_count = case_slot_count;

		if (expect_artifact_count(stream, "case_binders", &count) != 0 || count != case_binder_slot_count) {
			return -1;
		}
		for (size_t i = 0; i < count; ++i) {
			size_t id;
			uint32_t binding_id;
			int is_recursive;
			if (fscanf(stream, "%255s %zu", word, &id) != 2 ||
				strcmp(word, "case_binder") != 0 ||
				id >= case_binder_slot_count ||
				fscanf(stream, "%u %d", &binding_id, &is_recursive) != 2) {
				return -1;
			}
			if (artifact_case_binder_present(&terms->case_binders[id]) ||
				binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			terms->case_binders[id].binding_id = binding_id;
			terms->case_binders[id].is_recursive = is_recursive;
			if (terms->case_binders[id].binding_id != PROTOTYPE_INVALID_ID &&
				next_binding_id <= terms->case_binders[id].binding_id) {
				next_binding_id = terms->case_binders[id].binding_id + 1;
			}
	}
	terms->case_binder_count = case_binder_slot_count;
	terms->next_binding_id = next_binding_id;

	if (expect_artifact_count(stream, "match_frames", &count) != 0 || count != frame_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_ih_scope* frame;
		if (fscanf(stream, "%255s %zu", word, &id) != 2 ||
			strcmp(word, "match_frame") != 0 ||
				id >= frame_slot_count) {
				return -1;
			}
			frame = &terms->ih_scopes[id];
			if (artifact_frame_present(frame)) {
				return -1;
			}
			if (fscanf(
					stream,
					"%u %u %u %d",
					&frame->match_term,
					&frame->scrutinee_binding_id,
					&frame->key.case_count,
					&frame->key.is_linkable
				) != 4 ||
				read_artifact_term_key(stream, &frame->key.match_key) != 0) {
				return -1;
		}
		if (frame->scrutinee_binding_id != PROTOTYPE_INVALID_ID &&
			frame->scrutinee_binding_id >= terms->next_binding_id) {
			return -1;
		}
	}
	terms->ih_scope_count = frame_slot_count;

	memset(judgement->claims, 0, sizeof(*judgement->claims) * claim_slot_count);
	for (size_t i = 0; i < claim_slot_count; ++i) {
		judgement->claims[i].proposition_id = PROTOTYPE_INVALID_ID;
	}
	memset(judgement->propositions, 0,
		sizeof(*judgement->propositions) * judgement->proposition_capacity);
	memset(
		judgement->proposition_index_heads,
		0xff,
		sizeof(judgement->proposition_index_heads)
	);
	judgement->proposition_count = 0;
	judgement->proposition_resource_usage_count = 0;
	memset(judgement->derivations, 0, sizeof(*judgement->derivations) * derivation_slot_count);
	judgement->accepted_premise_count = 0;
	judgement->candidate_premise_count = 0;
	if (expect_artifact_count(stream, "propositions", &count) != 0 ||
		count != proposition_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_judgement_proposition read_proposition;
		memset(&read_proposition, 0, sizeof(read_proposition));
		unsigned int usage_count;
		if (fscanf(
				stream,
				"%255s %zu %d %d %u %u %u %u %u %u",
				word,
				&id,
				&read_proposition.kind,
				&read_proposition.authority_kind,
				&read_proposition.authority_id,
				&read_proposition.context_id,
				&read_proposition.occurrence_id,
				&read_proposition.subject,
				&read_proposition.classifier,
				&usage_count
			) != 10 || strcmp(word, "proposition") != 0 || id != i ||
			read_proposition.kind < PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			read_proposition.kind > PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
			read_proposition.authority_kind <
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			read_proposition.authority_kind >
				PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT ||
			usage_count > judgement->proposition_resource_usage_capacity) {
			return -1;
		}
		struct prototype_usage_entry usage[usage_count == 0 ? 1 : usage_count];
		for (unsigned int usage_index = 0;
			usage_index < usage_count;
			++usage_index) {
			if (fscanf(
					stream,
					"%u %d",
					&usage[usage_index].binding_id,
					&usage[usage_index].grade
				) != 2) {
				return -1;
			}
		}
		read_proposition.resource_usage_count = usage_count;
		read_proposition.resource_usage = usage_count == 0 ? NULL : usage;
		uint32_t proposition_id;
		if (prototype_judgement_proposition_intern(
				judgement, &read_proposition, &proposition_id
			) != 0 || proposition_id != id) {
			return -1;
		}
	}
	if (expect_artifact_count(stream, "claims", &count) != 0 ||
		count != claim_slot_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		char proposition_label[32];
		struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (fscanf(
				stream,
				"%255s %zu %31s %u",
				word,
				&id,
				proposition_label,
				&claim->proposition_id
			) != 4 || strcmp(word, "claim") != 0 || id != i ||
			strcmp(proposition_label, "proposition") != 0 ||
			claim->proposition_id >= proposition_slot_count) {
			return -1;
		}
		claim->closure_rank = PROTOTYPE_INVALID_ID;
	}
	judgement->claim_count = claim_slot_count;
	if (expect_artifact_count(stream, "derivations", &count) != 0 ||
		count != derivation_slot_count) {
		return -1;
	}
	unsigned char derivation_ids_seen[
		derivation_slot_count == 0 ? 1 : derivation_slot_count
	];
	memset(derivation_ids_seen, 0, sizeof(derivation_ids_seen));
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_judgement_derivation* derivation;
		char claim_label[32];
		char premise_count_label[32];
		int proof_kind;
		uint32_t conclusion_claim_id;
		uint32_t premise_count;
		if (fscanf(
				stream,
				"%255s %zu %d %31s %u %31s %u",
				word,
				&id,
				&proof_kind,
				claim_label,
				&conclusion_claim_id,
				premise_count_label,
				&premise_count
			) != 7 || strcmp(word, "derivation") != 0 ||
			id >= derivation_slot_count || derivation_ids_seen[id] ||
			strcmp(claim_label, "claim") != 0 ||
			strcmp(premise_count_label, "premises") != 0 ||
			proof_kind < PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO ||
			proof_kind > PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM ||
			((proof_kind >=
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION &&
			  proof_kind <=
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS) &&
			 proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX) ||
			conclusion_claim_id >= claim_slot_count ||
			premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		derivation_ids_seen[id] = 1;
		derivation = &judgement->derivations[id];
		derivation->proof_kind = proof_kind;
		derivation->conclusion_claim_id = conclusion_claim_id;
		derivation->premise_count = premise_count;
		derivation->closure_rank = PROTOTYPE_INVALID_ID;
		memset(&derivation->rule_data, 0xff, sizeof(derivation->rule_data));
		char payload_kind[32];
		if (fscanf(stream, "%255s %31s", word, payload_kind) != 2 ||
			strcmp(word, "payload") != 0) {
			return -1;
		}
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
			if (strcmp(payload_kind, "constructor") != 0 || fscanf(
					stream,
					"%u %u %u",
					&derivation->rule_data.constructor.owner_view,
					&derivation->rule_data.constructor.constructor_index,
					&derivation->rule_data.constructor.field_index
				) != 3) {
				return -1;
			}
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
			if (strcmp(payload_kind, "constructor_spine") != 0 || fscanf(
					stream,
					"%u",
					&derivation->rule_data.constructor.owner_view
				) != 1) {
				return -1;
			}
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			if (strcmp(payload_kind, "induction") != 0 || fscanf(
					stream,
					"%u %u %u %u",
					&derivation->rule_data.induction.match,
					&derivation->rule_data.induction.motive,
					&derivation->rule_data.induction.case_index,
					&derivation->rule_data.induction.field_index
				) != 4) {
				return -1;
			}
		} else if (strcmp(payload_kind, "none") != 0) {
			return -1;
		}
		if (fscanf(
				stream,
				"%255s %d %u",
				word,
				&derivation->semantic_action_kind,
				&derivation->semantic_action_id
			) != 3 || strcmp(word, "action") != 0) {
			return -1;
		}
		if (derivation->premise_count > judgement->accepted_premise_capacity -
			judgement->accepted_premise_count) {
			return -1;
		}
		derivation->premises = derivation->premise_count == 0 ? NULL :
			&judgement->accepted_premises[judgement->accepted_premise_count];
		judgement->accepted_premise_count += derivation->premise_count;
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			char premise_kind[32];
			char action_label[32];
			uint32_t reference_id;
			struct prototype_judgement_premise_edge* premise =
				&derivation->premises[j];
			if (fscanf(
					stream,
					"%255s %31s %u %31s %d %u",
					word,
					premise_kind,
					&reference_id,
					action_label,
					&premise->semantic_action_kind,
					&premise->semantic_action_id
				) != 6 || strcmp(word, "premise") != 0 ||
				strcmp(action_label, "action") != 0) {
				return -1;
			}
			premise->claim_id = PROTOTYPE_INVALID_ID;
			premise->scoped_proposition_id = PROTOTYPE_INVALID_ID;
			if (strcmp(premise_kind, "claim") == 0) {
				if (reference_id >= claim_slot_count) {
					return -1;
				}
				premise->claim_id = reference_id;
			} else if (strcmp(premise_kind, "scoped") == 0) {
				if (reference_id >= proposition_slot_count) {
					return -1;
				}
				premise->scoped_proposition_id = reference_id;
			} else {
				return -1;
			}
		}
	}
	judgement->derivation_count = derivation_slot_count;
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		return -1;
	}
		if (artifact_validate_read_graph_refs(
				terms,
				type_declarations,
				dimension_operators,
				judgement,
				0
			) != 0) {
			return -1;
		}
		judgement->next_universe_var = type_declarations->readback.next_level_var;
		prototype_internal_sync_artifact_universe_level_counters(terms, type_declarations, judgement);

	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "graph") != 0) {
		return -1;
	}
	prototype_type_declaration_db_mark_semantic_change(
		&type_declarations->semantic_schema
	);
	return 0;
}

int prototype_artifact_read_text_typed_occurrences(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !terms || !type_declarations || !judgement) {
		return -1;
	}
	char word[256];
	char section_name[256];
	int compile_policy;
	int definition_thunk_policy;
	char selected_entry_name[256];
	uint32_t selected_entry_term;
	uint32_t selected_entry_classifier;
	uint32_t selected_entry_occurrence;
	uint64_t required_runtime_capabilities;
	size_t occurrence_count;
	size_t occurrence_edge_count;
	size_t context_count;
	size_t substitution_count;
	size_t case_count;
	size_t fold_clause_count;
	size_t obligation_count;
	struct prototype_typed_occurrence_graph empty_graph;
	memset(&empty_graph, 0, sizeof(empty_graph));
	struct prototype_typed_occurrence_graph* graph = metadata ?
		prototype_compile_metadata_typed_occurrences(metadata) : &empty_graph;
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 || strcmp(section_name, "typed_occurrences") != 0 ||
		fscanf(
			stream,
			" %255s %d %d %255s %u %u %u %" SCNu64,
			word,
			&compile_policy,
			&definition_thunk_policy,
			selected_entry_name,
			&selected_entry_term,
			&selected_entry_classifier,
			&selected_entry_occurrence,
			&required_runtime_capabilities
		) != 8 || strcmp(word, "compile_policy") != 0 ||
		(compile_policy < PROTOTYPE_COMPILE_POLICY_STRICT ||
		 compile_policy > PROTOTYPE_COMPILE_POLICY_EXPLORATORY) ||
		(definition_thunk_policy < PROTOTYPE_DEFINITION_THUNK_IMPLICIT ||
		 definition_thunk_policy > PROTOTYPE_DEFINITION_THUNK_EXPLICIT) ||
		((strcmp(selected_entry_name, "-") == 0) !=
			(selected_entry_term == PROTOTYPE_INVALID_ID &&
			 selected_entry_classifier == PROTOTYPE_INVALID_ID &&
			 selected_entry_occurrence == PROTOTYPE_INVALID_ID)) ||
		(selected_entry_term != PROTOTYPE_INVALID_ID &&
			(!artifact_read_term_present(terms, selected_entry_term) ||
			 !artifact_read_term_present(terms, selected_entry_classifier))) ||
		expect_artifact_count(stream, "contexts", &context_count) != 0 ||
		(metadata && context_count > metadata->contexts.context_capacity)) {
		return -1;
	}
	if (metadata) {
		metadata->contexts.context_count = 0;
	}
	for (size_t i = 0; i < context_count; ++i) {
		size_t id;
		struct prototype_context context;
		uint32_t classifier;
		if (fscanf(
				stream,
				"%255s %zu %u %u %u %d %u %u",
				word,
				&id,
				&context.parent,
				&context.binding_id,
				&classifier,
				&context.extension_kind,
				&context.producer_computation,
				&context.depth
			) != 8 ||
			strcmp(word, "context") != 0 ||
			id != i ||
			(metadata && (
				(i != 0 && classifier == PROTOTYPE_INVALID_ID) ||
				(classifier != PROTOTYPE_INVALID_ID &&
				 !artifact_read_term_present(terms, classifier)) ||
				(context.producer_computation != PROTOTYPE_INVALID_ID &&
				 !artifact_read_term_present(
					terms, context.producer_computation
				 ))))) {
			return -1;
		}
		context.classifier_ref.kind = i == 0 ?
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_INVALID :
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM;
		context.classifier_ref.term_id = classifier;
		context.classifier_ref.variable_id = PROTOTYPE_INVALID_ID;
		if ((i == 0 && (context.extension_kind !=
				PROTOTYPE_CONTEXT_EXTENSION_INVALID ||
			context.producer_computation != PROTOTYPE_INVALID_ID)) ||
			(i != 0 && context.extension_kind !=
				PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
			 context.extension_kind !=
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) ||
			(i != 0 &&
			 (context.extension_kind == PROTOTYPE_CONTEXT_EXTENSION_VALUE) !=
				(context.producer_computation == PROTOTYPE_INVALID_ID))) {
			return -1;
		}
		if (metadata) {
			if ((i == 0 && context.binding_id != PROTOTYPE_INVALID_ID) ||
				(i != 0 && context.binding_id == PROTOTYPE_INVALID_ID)) {
				return -1;
			}
			if (i != 0 && context.binding_id >= terms->next_binding_id) {
				if (context.binding_id == PROTOTYPE_INVALID_ID - 1) {
					return -1;
				}
				terms->next_binding_id = context.binding_id + 1;
			}
			metadata->contexts.contexts[i] = context;
			metadata->contexts.context_count++;
		}
	}
	if (metadata && prototype_context_db_rebuild_runtime_index_after_bulk_load(
			&metadata->contexts
		) != 0) {
		return -1;
	}
	if (expect_artifact_count(stream, "substitutions", &substitution_count) != 0 ||
		(metadata &&
			substitution_count > metadata->substitutions.substitution_capacity)) {
		return -1;
	}
	if (metadata) {
		metadata->substitutions.substitution_count = 0;
	}
	for (size_t i = 0; i < substitution_count; ++i) {
		size_t id;
		struct prototype_substitution substitution;
		uint32_t evidence_claim_id;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u %u %u %u %u %u",
				word,
				&id,
				&substitution.kind,
				&substitution.source_context,
				&substitution.target_context,
				&substitution.first,
				&substitution.second,
				&substitution.term,
				&substitution.term_classifier,
				&evidence_claim_id
			) != 10 ||
			strcmp(word, "substitution") != 0 ||
			id != i ||
			substitution.kind < PROTOTYPE_SUBSTITUTION_IDENTITY ||
			substitution.kind > PROTOTYPE_SUBSTITUTION_COMPOSE ||
			substitution.source_context >= context_count ||
			substitution.target_context >= context_count ||
			(substitution.term != PROTOTYPE_INVALID_ID &&
				substitution.term >= terms->term_count) ||
			(substitution.term_classifier != PROTOTYPE_INVALID_ID &&
				substitution.term_classifier >= terms->term_count) ||
			(substitution.first != PROTOTYPE_INVALID_ID &&
				substitution.first >= i) ||
			(substitution.second != PROTOTYPE_INVALID_ID &&
				substitution.second >= i) ||
			(substitution.kind == PROTOTYPE_SUBSTITUTION_EXTEND) !=
				(evidence_claim_id != PROTOTYPE_INVALID_ID) ||
			(evidence_claim_id != PROTOTYPE_INVALID_ID &&
				evidence_claim_id >= judgement->claim_count)) {
			return -1;
		}
		if (metadata) {
			metadata->substitutions.substitutions[i] = substitution;
			metadata->substitutions.substitution_count++;
			if (substitution.kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
				int has_derivation = evidence_claim_id < judgement->claim_count &&
					judgement->claims[evidence_claim_id].first_derivation !=
						PROTOTYPE_INVALID_ID;
				int certifies = prototype_cwf_substitution_claim_certifies(
						&metadata->substitutions,
						judgement,
						(uint32_t)i,
						evidence_claim_id
					);
				if (!has_derivation || !certifies ||
					prototype_compile_metadata_record_accepted_substitution_claim(
						metadata, (uint32_t)i, evidence_claim_id
					) != 0) {
					return -1;
				}
			}
		}
	}
	if (metadata && prototype_substitution_db_rebuild_runtime_index_after_bulk_load(
			&metadata->substitutions
		) != 0) {
		return -1;
	}
	if ((metadata && prototype_substitution_db_validate(
			&metadata->substitutions, &metadata->contexts, terms
		) != 0) ||
		expect_artifact_count(stream, "typed_occurrences", &occurrence_count) != 0 ||
		(metadata && occurrence_count > graph->occurrence_capacity)) {
		return -1;
	}
	if (metadata) {
		metadata->compile_policy = compile_policy;
		metadata->definition_thunk_policy = definition_thunk_policy;
		metadata->selected_entry_symbol_id = strcmp(selected_entry_name, "-") == 0 ?
			-1 : symbol_intern(symbols, selected_entry_name, strlen(selected_entry_name));
		if (strcmp(selected_entry_name, "-") != 0 &&
			metadata->selected_entry_symbol_id < 0) {
			return -1;
		}
		metadata->selected_entry_term = selected_entry_term;
		metadata->selected_entry_classifier = selected_entry_classifier;
		metadata->selected_entry_occurrence = selected_entry_occurrence;
		metadata->required_runtime_capabilities = required_runtime_capabilities;
		graph->occurrence_count = 0;
		graph->edge_count = 0;
		graph->case_count = 0;
		graph->fold_clause_count = 0;
		memset(
			&metadata->effect_constraint_summary,
			0,
			sizeof(metadata->effect_constraint_summary)
		);
		prototype_verification_db_clear(&metadata->verification);
	}
	for (size_t i = 0; i < occurrence_count; ++i) {
		size_t id;
		struct prototype_typed_occurrence operation;
		char source_name[256];
		char binder_name[256];
		memset(&operation, 0, sizeof(operation));
		if (fscanf(stream, "%255s %zu %d %d %d %u %d %u %u %255s %255s"
				" %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
				word, &id, &operation.tag, &operation.category,
				&operation.application_role,
				&operation.core_term,
				&operation.classifier_status,
				&operation.classifier,
				&operation.classifier_verification_obligation,
				source_name, binder_name,
				&operation.binding_id, &operation.wrapped_occurrence,
				&operation.binder_classifier, &operation.ih_owner_occurrence,
				&operation.ih_scope_id,
				&operation.ih_case_index,
				&operation.ih_field_index,
				&operation.fold_return_binder_id,
				&operation.implicit_effect_row_count,
				&operation.first_case, &operation.case_count,
				&operation.first_fold_clause, &operation.fold_clause_count,
				&operation.context_id,
				&operation.context_action_substitution,
				&operation.source_core_term,
				&operation.source_classifier) != 28 ||
			strcmp(word, "typed_occurrence") != 0 || id != i) {
			return -1;
		}
		if (operation.tag < PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
			operation.tag > PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			operation.classifier_status <
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED ||
			operation.classifier_status >
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION ||
			(operation.classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED &&
			 operation.classifier == PROTOTYPE_INVALID_ID) ||
			(operation.classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION &&
			 (operation.classifier != PROTOTYPE_INVALID_ID ||
			  operation.classifier_verification_obligation == PROTOTYPE_INVALID_ID)) ||
			operation.category < PROTOTYPE_TERM_CATEGORY_INVALID ||
			operation.category > PROTOTYPE_TERM_CATEGORY_TYPE ||
			operation.application_role < PROTOTYPE_TERM_APPLICATION_NONE ||
			operation.application_role >
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION ||
			(operation.tag != PROTOTYPE_TYPED_OCCURRENCE_APP &&
			 operation.application_role != PROTOTYPE_TERM_APPLICATION_NONE)) {
			return -1;
		}
		operation.source_ast = PROTOTYPE_INVALID_ID;
		operation.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
		operation.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID;
		operation.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		operation.first_edge = PROTOTYPE_INVALID_ID;
		operation.edge_count = 0;
		if (operation.classifier != PROTOTYPE_INVALID_ID) {
			struct prototype_term_classifier_view classifier_view;
			if (prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					operation.classifier,
					&classifier_view
				) != 0) {
				fprintf(
					stderr,
					"artifact typed-occurrence graph: classifier view failed "
					"occurrence=%zu classifier=%u\n",
					i, operation.classifier
				);
				return -1;
			}
			operation.computation_kind =
				classifier_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
					classifier_view.computation_kind :
					PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		}
		if ((operation.context_action_substitution == PROTOTYPE_INVALID_ID &&
			 (operation.source_core_term != PROTOTYPE_INVALID_ID ||
			  operation.source_classifier != PROTOTYPE_INVALID_ID)) ||
			(operation.context_action_substitution != PROTOTYPE_INVALID_ID &&
			 operation.source_core_term == PROTOTYPE_INVALID_ID) ||
			(metadata && (operation.context_id >= context_count ||
			 (operation.context_action_substitution != PROTOTYPE_INVALID_ID &&
			  (operation.context_action_substitution >= substitution_count ||
			   operation.source_core_term >= terms->term_count ||
			   (operation.source_classifier != PROTOTYPE_INVALID_ID &&
			    operation.source_classifier >= terms->term_count)))))) {
			return -1;
		}
		if (strcmp(source_name, "-") == 0) {
			operation.source_symbol_id = -1;
		} else if ((operation.source_symbol_id = symbol_intern(
				symbols, source_name, strlen(source_name)
			)) < 0) {
			return -1;
		}
		if (strcmp(binder_name, "-") == 0) {
			operation.binder_symbol_id = -1;
		} else if ((operation.binder_symbol_id = symbol_intern(
				symbols, binder_name, strlen(binder_name)
			)) < 0) {
			return -1;
		}
		size_t row_id;
		if (fscanf(stream, "%255s %zu", word, &row_id) != 2 ||
			strcmp(word, "typed_occurrence_rows") != 0 || row_id != i ||
			operation.implicit_effect_row_count > 16) {
			return -1;
		}
		for (uint32_t j = 0; j < operation.implicit_effect_row_count; ++j) {
			if (fscanf(stream, "%u", &operation.implicit_effect_row_binders[j]) != 1) {
				return -1;
			}
		}
		if (metadata) {
			if (prototype_typed_occurrence_graph_add(
					graph, &metadata->contexts, operation, NULL
				) != 0) {
				fprintf(
					stderr,
					"artifact typed-occurrence graph: occurrence validation failed "
					"occurrence=%zu tag=%d core=%u classifier=%u context=%u\n",
					i, operation.tag, operation.core_term, operation.classifier,
					operation.context_id
				);
				return -1;
			}
		}
	}
	if (metadata && graph->occurrence_count != occurrence_count) {
		fprintf(stderr, "artifact typed-occurrence graph: occurrence count mismatch\n");
		return -1;
	}
	if (expect_artifact_count(
			stream, "occurrence_edges", &occurrence_edge_count
		) != 0 || (metadata && occurrence_edge_count > graph->edge_capacity)) {
		return -1;
	}
	for (size_t i = 0; i < occurrence_edge_count; ++i) {
		size_t id;
		uint32_t parent_occurrence;
		struct prototype_typed_occurrence_edge edge;
		if (fscanf(
				stream,
				"%255s %zu %u %d %u %u",
				word,
				&id,
				&parent_occurrence,
				&edge.role,
				&edge.ordinal,
				&edge.child_occurrence
			) != 6 || strcmp(word, "occurrence_edge") != 0 || id != i ||
			parent_occurrence >= occurrence_count ||
			edge.child_occurrence >= occurrence_count ||
			(metadata && prototype_typed_occurrence_graph_add_edge(
				graph, parent_occurrence, edge
			) != 0)) {
			fprintf(
				stderr,
				"artifact typed-occurrence graph: edge validation failed edge=%zu\n",
				i
			);
			return -1;
		}
	}
	if (selected_entry_occurrence != PROTOTYPE_INVALID_ID) {
		if (selected_entry_occurrence >= occurrence_count ||
			selected_entry_term >= terms->term_count ||
			terms->terms[selected_entry_term].tag != PROTOTYPE_TERM_FORCE) {
			return -1;
		}
		if (metadata) {
			const struct prototype_typed_occurrence* entry =
				&graph->occurrences[selected_entry_occurrence];
			if (entry->tag != PROTOTYPE_TYPED_OCCURRENCE_FORCE ||
				entry->core_term != selected_entry_term ||
				entry->classifier != selected_entry_classifier) {
				return -1;
			}
		}
	}
	if (expect_artifact_count(
			stream, "occurrence_fold_clauses", &fold_clause_count
		) != 0 || (metadata && fold_clause_count > graph->fold_clause_capacity)) {
		return -1;
	}
	for (size_t i = 0; i < fold_clause_count; ++i) {
		size_t id;
		struct prototype_typed_occurrence_fold_clause clause;
		memset(&clause, 0xff, sizeof(clause));
		if (fscanf(
				stream,
				"%255s %zu %u %u %u",
				word,
				&id,
				&clause.context_id,
				&clause.argument_binder_id,
				&clause.continuation_binder_id
			) != 5 || strcmp(word, "occurrence_fold_clause") != 0 || id != i ||
			(metadata && clause.context_id >= context_count)) {
			return -1;
		}
		if (metadata && prototype_typed_occurrence_graph_add_fold_clause(
				graph, &metadata->contexts, clause, NULL
			) != 0) {
			return -1;
		}
	}
	if (metadata) {
		if (prototype_context_db_validate(&metadata->contexts, terms) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: Context validation failed\n");
			return -1;
		}
		if (prototype_substitution_db_validate_classifier_coherence(
				&metadata->substitutions,
				&metadata->contexts,
				terms,
				type_declarations
				) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: Substitution validation failed\n");
			return -1;
		}
		if (prototype_cwf_validate_accepted_semantic_action_coverage(
				&metadata->substitutions, judgement
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: semantic action validation failed\n");
			return -1;
		}
	}
	if (expect_artifact_count(stream, "occurrence_match_cases", &case_count) != 0 ||
		(metadata && case_count > graph->case_capacity)) {
		return -1;
	}
	for (size_t i = 0; i < case_count; ++i) {
		size_t id;
		struct prototype_typed_occurrence_match_case operation_case;
		char label[256];
		memset(&operation_case, 0, sizeof(operation_case));
		if (fscanf(stream, "%255s %zu %u %d %u %u %u %255s", word, &id,
				&operation_case.context_id,
				&operation_case.refinement_status,
				&operation_case.refinement_substitution,
				&operation_case.constructor_owner,
				&operation_case.constructor_id, label) != 8 ||
			strcmp(word, "occurrence_match_case") != 0 || id != i) {
			return -1;
		}
		if ((operation_case.refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED &&
			 operation_case.refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE &&
			 operation_case.refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT) ||
			(operation_case.refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE &&
			 operation_case.refinement_substitution != PROTOTYPE_INVALID_ID) ||
			(operation_case.refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT &&
			 operation_case.refinement_substitution != PROTOTYPE_INVALID_ID) ||
			(metadata && (operation_case.context_id >= context_count ||
			 (operation_case.refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED &&
			  operation_case.refinement_substitution >= substitution_count)))) {
			return -1;
		}
		if (strcmp(label, "-") == 0) {
			operation_case.case_label_symbol_id = -1;
		} else if ((operation_case.case_label_symbol_id = symbol_intern(
			symbols, label, strlen(label))) < 0) {
			return -1;
		}
		size_t binder_case_id;
		if (fscanf(
				stream,
				"%255s %zu %u",
				word,
				&binder_case_id,
				&operation_case.binder_count
			) != 3 || strcmp(word, "occurrence_match_case_binders") != 0 ||
			binder_case_id != i || operation_case.binder_count >
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY) {
			return -1;
		}
		for (uint32_t j = 0; j < operation_case.binder_count; ++j) {
			operation_case.ast_binder_ids[j] = PROTOTYPE_INVALID_ID;
			if (fscanf(stream, "%u", &operation_case.binder_ids[j]) != 1) {
				return -1;
			}
		}
		if (metadata) {
			if (prototype_typed_occurrence_graph_add_case(
					graph, &metadata->contexts, operation_case, NULL
				) != 0) {
				fprintf(stderr,
					"artifact typed-occurrence graph: case validation failed case=%zu context=%u binders=%u\n",
					i, operation_case.context_id, operation_case.binder_count);
				return -1;
			}
		}
	}
	if (expect_artifact_count(stream, "verification_obligations", &obligation_count) != 0 ||
		(metadata && obligation_count >
			prototype_verification_db_capacity(&metadata->verification))) {
		return -1;
	}
	/* Import and linker callers that do not supply an occurrence graph cannot
	 * preserve a residual obligation. Reject it here rather than silently
	 * producing a linked artifact with weaker verification coverage. */
	if (!metadata && obligation_count != 0) {
		return -1;
	}
	if (compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT && obligation_count != 0) {
		return -1;
	}
	for (size_t i = 0; i < obligation_count; ++i) {
		size_t id;
		struct prototype_verification_obligation obligation;
		if (fscanf(stream, "%255s %zu %d %d %u %u %u %u %u %u %u %u %d %d %u", word, &id,
				&obligation.kind, &obligation.state, &obligation.occurrence,
				&obligation.core_term, &obligation.computation_occurrence,
				&obligation.continuation_occurrence, &obligation.continuation_binder_id,
				&obligation.input_classifier, &obligation.classifier_family,
				&obligation.effect_row, &obligation.effect_constraint_kind,
				&obligation.normalization_profile, &obligation.schema_version) != 15 ||
			strcmp(word, "verification") != 0 || id != i ||
			obligation.schema_version !=
				prototype_verification_obligation_schema_version(obligation.kind) ||
			obligation.schema_version == 0 ||
			obligation.state != PROTOTYPE_VERIFICATION_OBLIGATION_PENDING ||
			obligation.normalization_profile < PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
			obligation.normalization_profile >
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
			(metadata && prototype_verification_db_add(
				&metadata->verification, obligation, NULL
			) != 0)) {
			return -1;
		}
	}
	size_t verification_dependency_count;
	if (expect_artifact_count(
			stream,
			"verification_dependencies",
			&verification_dependency_count
		) != 0 ||
		(metadata && verification_dependency_count >
			metadata->verification.dependency_capacity) ||
		(!metadata && verification_dependency_count != 0)) {
		return -1;
	}
	for (size_t i = 0; i < verification_dependency_count; ++i) {
		size_t id;
		uint32_t occurrence;
		uint32_t obligation_id;
		if (fscanf(
				stream,
				"%255s %zu %u %u",
				word,
				&id,
				&occurrence,
				&obligation_id
			) != 4 || strcmp(word, "verification_dependency") != 0 ||
			id != i || !metadata ||
			prototype_verification_db_add_dependency(
				&metadata->verification, occurrence, obligation_id
			) != 0) {
			return -1;
		}
	}
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 || strcmp(section_name, "typed_occurrences") != 0) {
		return -1;
	}
	if (metadata) {
		if (prototype_typed_occurrence_graph_freeze(
				graph, terms, &metadata->contexts
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: freeze failed\n");
			return -1;
		}
		for (size_t i = 0; i < prototype_typed_occurrence_graph_count(graph); ++i) {
			const struct prototype_typed_occurrence* operation =
				prototype_typed_occurrence_graph_get(graph, (uint32_t)i);
			if (!operation) {
				return -1;
			}
			uint32_t term_references[] = {
				operation->core_term,
				operation->classifier,
				operation->binder_classifier
			};
			for (size_t j = 0; j < sizeof(term_references) / sizeof(term_references[0]); ++j) {
				if (term_references[j] != PROTOTYPE_INVALID_ID &&
					!artifact_read_term_present(terms, term_references[j])) {
					return -1;
				}
			}
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
				terms->terms[operation->core_term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD &&
				terms->terms[operation->core_term].as.computation_fold.clause_count > 0 &&
				(operation->fold_clause_count !=
					terms->terms[operation->core_term].as.computation_fold.clause_count ||
				 (size_t)operation->first_fold_clause + operation->fold_clause_count >
					metadata->typed_occurrences.fold_clause_count ||
					 operation->fold_return_binder_id == PROTOTYPE_INVALID_ID ||
				 terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD)) {
				return -1;
			}
		}
		if (prototype_verification_db_validate(
				&metadata->verification, graph, terms
			) != 0) {
			return -1;
		}
		uint64_t declared_capabilities = metadata->required_runtime_capabilities;
		compile_metadata_refresh_runtime_capabilities(metadata, terms);
		if (metadata->required_runtime_capabilities != declared_capabilities) {
			return -1;
		}
		if (prototype_type_declaration_rebuild_representations(
				terms, type_declarations, &metadata->contexts
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: representation rebuild failed\n");
			return -1;
		}
		if (artifact_resolve_representation_handles(
				terms, type_declarations
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: representation resolution failed\n");
			return -1;
		}
		if (prototype_constructor_curried_caches_validate(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache, &metadata->contexts, terms
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: constructor cache validation failed\n");
			return -1;
		}
		if (artifact_validate_term_graph_refs(
				terms,
				type_declarations,
				&metadata->dimension_operators,
				1
			) != 0) {
			fprintf(stderr, "artifact typed-occurrence graph: Term reference validation failed\n");
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_read_text_universe(
	FILE* stream,
	struct prototype_universe_db* universe
) {
	if (!stream || !universe) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "universe") != 0) {
		return -1;
	}

	char label_node_slots[32];
	char label_nodes[32];
	char label_edge_slots[32];
	char label_edges[32];
	char label_levels[32];
	char label_constraints[32];
	char label_obligations[32];
	char label_certificate_state[32];
	size_t node_slot_count;
	size_t present_node_count;
	size_t edge_slot_count;
	size_t present_edge_count;
	size_t level_count;
	size_t constraint_count;
	size_t obligation_count;
	int certificate_state;
	if (fscanf(
			stream,
			"%255s %31s %zu %31s %zu %31s %zu %31s %zu %31s %zu %31s %zu %31s %zu %31s %d",
			word,
			label_node_slots,
			&node_slot_count,
			label_nodes,
			&present_node_count,
			label_edge_slots,
			&edge_slot_count,
			label_edges,
			&present_edge_count,
			label_levels,
			&level_count,
			label_constraints,
			&constraint_count,
			label_obligations,
			&obligation_count,
			label_certificate_state,
			&certificate_state
		) != 17 ||
		strcmp(word, "counts") != 0 ||
		strcmp(label_node_slots, "node_slots") != 0 ||
		strcmp(label_nodes, "nodes") != 0 ||
		strcmp(label_edge_slots, "edge_slots") != 0 ||
		strcmp(label_edges, "edges") != 0 ||
		strcmp(label_levels, "levels") != 0 ||
		strcmp(label_constraints, "constraints") != 0 ||
		strcmp(label_obligations, "obligations") != 0 ||
		strcmp(label_certificate_state, "certificate_state") != 0 ||
		node_slot_count > universe->node_capacity ||
		edge_slot_count > universe->edge_capacity ||
		level_count > universe->level_capacity ||
		constraint_count > universe->constraint_capacity ||
		obligation_count > universe->obligation_span_capacity ||
		certificate_state != PROTOTYPE_UNIVERSE_CERTIFICATE_CLOSED) {
		return -1;
	}

	size_t count;
	memset(universe->nodes, 0, sizeof(*universe->nodes) * node_slot_count);
	memset(universe->edges, 0, sizeof(*universe->edges) * edge_slot_count);
	if (expect_artifact_count(stream, "universe_nodes", &count) != 0 ||
		count != present_node_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_node read_node;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u %d %u",
				word,
				&id,
				&read_node.tag,
				&read_node.type_id,
				&read_node.parameter_id,
				&read_node.symbol_id,
				&read_node.type_expr
			) != 7 ||
				strcmp(word, "universe_node") != 0 ||
				id >= node_slot_count) {
				return -1;
			}
			if (artifact_universe_node_present(&universe->nodes[id])) {
				return -1;
			}
			universe->nodes[id] = read_node;
		}
		universe->node_count = node_slot_count;

	if (expect_artifact_count(stream, "universe_edges", &count) != 0 ||
		count != present_edge_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_edge read_edge;
		if (fscanf(
				stream,
				"%255s %zu %d %u %u",
				word,
				&id,
				&read_edge.tag,
				&read_edge.from_node,
				&read_edge.to_node
			) != 5 ||
			strcmp(word, "universe_edge") != 0 ||
				id >= edge_slot_count ||
				read_edge.from_node >= node_slot_count ||
				read_edge.to_node >= node_slot_count) {
				return -1;
			}
			if (artifact_universe_edge_present(&universe->edges[id])) {
				return -1;
			}
			universe->edges[id] = read_edge;
		}
		universe->edge_count = edge_slot_count;

	if (expect_artifact_count(stream, "universe_levels", &count) != 0 ||
		count != level_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_level* level = &universe->levels[i];
		if (fscanf(
				stream,
				"%255s %zu %u %d",
				word,
				&id,
				&level->level_var,
				&level->value
			) != 4 ||
			strcmp(word, "universe_level") != 0 ||
			id != i) {
			return -1;
		}
	}
	universe->level_count = count;

	if (expect_artifact_count(stream, "universe_constraints", &count) != 0 ||
		count != constraint_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_constraint* constraint = &universe->constraints[i];
		if (fscanf(
				stream,
				"%255s %zu %u %u %d %u %u %d %u %u %d %u %u %u",
				word,
				&id,
				&constraint->lower_level_var,
				&constraint->upper_level_var,
				&constraint->offset,
				&constraint->subject,
				&constraint->classifier,
				&constraint->reason,
				&constraint->source_claim_id,
				&constraint->source_derivation_id,
				&constraint->source_authority_kind,
				&constraint->source_authority_id,
				&constraint->source_subject,
				&constraint->source_classifier
			) != 14 ||
			strcmp(word, "universe_constraint") != 0 ||
			id != i) {
			return -1;
		}
		}
		universe->constraint_count = count;

	if (expect_artifact_count(stream, "universe_obligations", &count) != 0 ||
		count != obligation_count) {
		return -1;
	}
	for (size_t i = 0; i < count; ++i) {
		size_t id;
		struct prototype_universe_obligation_span* span =
			&universe->obligation_spans[i];
		if (fscanf(
				stream,
				"%255s %zu %u %u %u %u",
				word,
				&id,
				&span->source_claim_id,
				&span->source_derivation_id,
				&span->first_constraint,
				&span->constraint_count
			) != 6 || strcmp(word, "universe_obligation") != 0 || id != i ||
			span->first_constraint > constraint_count ||
			span->constraint_count > constraint_count - span->first_constraint) {
			return -1;
		}
	}
	universe->obligation_span_count = count;
	if (fscanf(
			stream,
			"%255s %" SCNu64 " %" SCNu64 " %u %u %d",
			word,
			&universe->certificate.constraint_fingerprint,
			&universe->certificate.solution_fingerprint,
			&universe->certificate.constraint_count,
			&universe->certificate.level_count,
			&universe->certificate.state
		) != 6 || strcmp(word, "universe_certificate") != 0 ||
		universe->certificate.state != certificate_state ||
		universe->certificate.constraint_count != constraint_count ||
		universe->certificate.level_count != level_count) {
		return -1;
	}
		if (artifact_validate_read_universe_refs(universe) != 0) {
			return -1;
		}

		if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "universe") != 0) {
		return -1;
	}
	return 0;
}

int prototype_artifact_read_text_debug(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_debug_table* debug
) {
	if (!stream || !symbols || !debug) {
		return -1;
	}
	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "debug") != 0) {
		return -1;
	}

	size_t count;
	if (expect_artifact_count(stream, "term_names", &count) != 0 ||
		count > debug->term_name_capacity) {
		return -1;
	}
	debug->term_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		size_t term;
		size_t classifier;
		size_t source_entry_id;
		unsigned name_line;
		unsigned name_column;
		unsigned body_line;
		unsigned body_column;
		if (fscanf(
				stream,
				"%255s %255s %zu %zu %zu %u %u %u %u",
				word,
				name,
				&term,
				&classifier,
				&source_entry_id,
				&name_line,
				&name_column,
				&body_line,
				&body_column
			) != 9 ||
			strcmp(word, "term_name") != 0 ||
			term > UINT32_MAX ||
			classifier > UINT32_MAX ||
			source_entry_id > UINT32_MAX) {
			return -1;
		}
		debug->term_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->term_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->term_names[i].term = (uint32_t)term;
		debug->term_names[i].classifier = (uint32_t)classifier;
		debug->term_names[i].source_entry_id = (uint32_t)source_entry_id;
		debug->term_names[i].name_span.line = name_line;
		debug->term_names[i].name_span.column = name_column;
		debug->term_names[i].body_span.line = body_line;
		debug->term_names[i].body_span.column = body_column;
	}

	if (expect_artifact_count(stream, "type_names", &count) != 0 ||
		count > debug->type_name_capacity) {
		return -1;
	}
	debug->type_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		size_t local_type_id;
		unsigned name_line;
		unsigned name_column;
		unsigned body_line;
		unsigned body_column;
		if (fscanf(
				stream,
				"%255s %255s %zu %u %u %u %u",
				word,
				name,
				&local_type_id,
				&name_line,
				&name_column,
				&body_line,
				&body_column
			) != 7 ||
			strcmp(word, "type_name") != 0 ||
			local_type_id > UINT32_MAX) {
			return -1;
		}
		debug->type_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->type_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->type_names[i].local_type_id = (uint32_t)local_type_id;
		debug->type_names[i].name_span.line = name_line;
		debug->type_names[i].name_span.column = name_column;
		debug->type_names[i].body_span.line = body_line;
		debug->type_names[i].body_span.column = body_column;
	}

	if (expect_artifact_count(stream, "constructor_names", &count) != 0 ||
		count > debug->constructor_name_capacity) {
		return -1;
	}
	debug->constructor_name_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		uint32_t type_export_index;
		uint32_t ordinal;
		unsigned name_line;
		unsigned name_column;
		if (fscanf(
				stream,
				"%255s %u %255s %u %u %u",
				word,
				&type_export_index,
				name,
				&ordinal,
				&name_line,
				&name_column
			) != 6 ||
			strcmp(word, "constructor_name") != 0) {
			return -1;
		}
		debug->constructor_names[i].type_export_index = type_export_index;
		debug->constructor_names[i].name_symbol_id =
			symbol_intern(symbols, name, strlen(name));
		if (debug->constructor_names[i].name_symbol_id < 0) {
			return -1;
		}
		debug->constructor_names[i].ordinal = ordinal;
		debug->constructor_names[i].name_span.line = name_line;
		debug->constructor_names[i].name_span.column = name_column;
	}
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "debug") != 0) {
		return -1;
	}
	return 0;
}

int prototype_artifact_read_text_relocation(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_relocation_table* relocation
) {
	if (!stream || !symbols || !relocation) {
		return -1;
	}

	char word[256];
	char section_name[256];
	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "SECTION") != 0 ||
		strcmp(section_name, "relocation") != 0) {
		return -1;
	}

	size_t count;
	if (expect_artifact_count(stream, "external_term_refs", &count) != 0 ||
		count > relocation->external_term_ref_capacity) {
		return -1;
	}
	relocation->external_term_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t term;
		if (fscanf(
					stream,
					"%255s %zu %255s %255s",
					word,
					&term,
					namespace_name,
					name
			) != 4 ||
			strcmp(word, "external_term_ref") != 0 ||
			term > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_term_refs[i].term = (uint32_t)term;
		relocation->external_term_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		relocation->external_term_refs[i].name.name_symbol_id = interned;
		if (relocation->external_term_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "resolved_external_term_refs", &count) != 0 ||
		count > relocation->resolved_external_term_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_term_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t term;
		uint32_t term_export_index;
		if (fscanf(
				stream,
					"%255s %zu %u %255s %255s",
				word,
				&term,
				&term_export_index,
					namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_term_ref") != 0 ||
			term > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->resolved_external_term_refs[i].term = (uint32_t)term;
		relocation->resolved_external_term_refs[i].term_export_index =
			term_export_index;
		relocation->resolved_external_term_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		relocation->resolved_external_term_refs[i].name.name_symbol_id = interned;
		if (relocation->resolved_external_term_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "external_type_expr_refs", &count) != 0 ||
		count > relocation->external_type_expr_ref_capacity) {
		return -1;
	}
	relocation->external_type_expr_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		int symbol_id;
		size_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %d %255s",
				word,
				&type_expr,
				&symbol_id,
				name
			) != 4 ||
			strcmp(word, "external_type_expr_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_type_expr_refs[i].type_expr = (uint32_t)type_expr;
		relocation->external_type_expr_refs[i].name_symbol_id = interned;
		(void)symbol_id;
	}

	if (expect_artifact_count(stream, "resolved_external_type_expr_refs", &count) != 0 ||
		count > relocation->resolved_external_type_expr_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_type_expr_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t type_expr;
		uint32_t type_export_index;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %255s",
				word,
				&type_expr,
				&type_export_index,
				namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_type_expr_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0 ||
			read_artifact_type_representation_fingerprint(
				stream,
				&relocation->resolved_external_type_expr_refs[i].representation_fingerprint
			) != 0) {
			return -1;
		}
		relocation->resolved_external_type_expr_refs[i].type_expr =
			(uint32_t)type_expr;
		relocation->resolved_external_type_expr_refs[i].type_export_index =
			type_export_index;
		relocation->resolved_external_type_expr_refs[i].name.name_symbol_id = interned;
		relocation->resolved_external_type_expr_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (relocation->resolved_external_type_expr_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}

	if (expect_artifact_count(stream, "external_type_former_refs", &count) != 0 ||
		count > relocation->external_type_former_ref_capacity) {
		return -1;
	}
	relocation->external_type_former_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char name[256];
		int symbol_id;
		size_t type_expr;
		if (fscanf(
				stream,
				"%255s %zu %d %255s",
				word,
				&type_expr,
				&symbol_id,
				name
			) != 4 ||
			strcmp(word, "external_type_former_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0) {
			return -1;
		}
		relocation->external_type_former_refs[i].type_expr = (uint32_t)type_expr;
		relocation->external_type_former_refs[i].name_symbol_id = interned;
		(void)symbol_id;
	}
	if (expect_artifact_count(stream, "resolved_external_type_former_refs", &count) != 0 ||
		count > relocation->resolved_external_type_former_ref_capacity) {
		return -1;
	}
	relocation->resolved_external_type_former_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		char namespace_name[256];
		char name[256];
		size_t type_expr;
		uint32_t type_export_index;
		if (fscanf(
				stream,
				"%255s %zu %u %255s %255s",
				word,
				&type_expr,
				&type_export_index,
				namespace_name,
				name
			) != 5 ||
			strcmp(word, "resolved_external_type_former_ref") != 0 ||
			type_expr > UINT32_MAX) {
			return -1;
		}
		int interned = symbol_intern(symbols, name, strlen(name));
		if (interned < 0 ||
			read_artifact_type_representation_fingerprint(
				stream,
				&relocation->resolved_external_type_former_refs[i].representation_fingerprint
			) != 0) {
			return -1;
		}
		relocation->resolved_external_type_former_refs[i].type_expr =
			(uint32_t)type_expr;
		relocation->resolved_external_type_former_refs[i].type_export_index =
			type_export_index;
		relocation->resolved_external_type_former_refs[i].name.name_symbol_id = interned;
		relocation->resolved_external_type_former_refs[i].name.namespace_symbol_id =
			strcmp(namespace_name, "-") == 0 ? -1 :
			symbol_intern(symbols, namespace_name, strlen(namespace_name));
		if (relocation->resolved_external_type_former_refs[i].name.namespace_symbol_id < -1) {
			return -1;
		}
	}
	if (expect_artifact_count(stream, "resolved_constructor_owner_refs", &count) != 0 ||
		count > relocation->resolved_constructor_owner_ref_capacity) {
		return -1;
	}
	relocation->resolved_constructor_owner_ref_count = count;
	for (size_t i = 0; i < count; ++i) {
		struct prototype_artifact_resolved_constructor_owner_ref* ref =
			&relocation->resolved_constructor_owner_refs[i];
		size_t source;
		if (fscanf(
				stream,
				"%255s %d %zu %u %u",
				word,
				&ref->source_kind,
				&source,
				&ref->owner,
				&ref->ordinal
			) != 5 ||
			strcmp(word, "resolved_constructor_owner_ref") != 0 ||
			source > UINT32_MAX ||
			read_artifact_term_key(stream, &ref->owner_key) != 0) {
			return -1;
		}
		ref->source = (uint32_t)source;
	}
	if (expect_artifact_count(stream, "external_constructor_owner_refs", &count) != 0) {
		return -1;
	}
	relocation->external_constructor_owner_ref_count = count;
	if (count != 0) {
		return -1;
	}

	if (fscanf(stream, "%255s %255s", word, section_name) != 2 ||
		strcmp(word, "END") != 0 ||
		strcmp(section_name, "relocation") != 0) {
		return -1;
	}
	return 0;
}
