#include "a_program/frontend/reader.h"

#include "a_program/artifact/wire_v90.h"
#include "a_program/driver/compiler_session.h"
#include "compiler_session_internal.h"
#include "a_program/driver/diagnostics.h"
#include "a_program/frontend/function_graph.h"
#include "a_program/frontend/universe_collection.h"
#include "a_program/graph/typed_occurrence_graph.h"

#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "a_program/support/symbol.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/universe.h"
#include "a_program/kernel/type_term_debug.h"

#define SYMBOL_MAP_CAPACITY 1024
#define SYMBOL_STORAGE_CAPACITY 512
#define TYPE_CAPACITY 64
#define CONSTRUCTOR_CAPACITY 256
#define PARAMETER_CAPACITY 128
#define FIELD_TYPE_CAPACITY 512
#define TYPE_EXPR_CAPACITY 1024
#define AST_CAPACITY 4096
#define AST_DEF_CAPACITY 256
#define AST_MATCH_CASE_CAPACITY 256
#define AST_MATCH_BINDER_CAPACITY 512
#define AST_COMPUTATION_FOLD_CLAUSE_CAPACITY 256
#define AST_BLOCK_ITEM_CAPACITY 4096
#define AST_DEFINITION_ITEM_CAPACITY 4096
#define AST_TYPE_EXPR_CAPACITY 4096
#define AST_TYPE_DEF_CAPACITY 64
#define AST_FAMILY_BINDER_CAPACITY 128
#define AST_TYPE_CONSTRUCTOR_CAPACITY 256
#define AST_TYPE_FIELD_EXPR_CAPACITY 512
#define AST_ACCEPTED_BINDING_SUBSTITUTION_CAPACITY 8192
#define TERM_CAPACITY 262144
#define MATCH_CASE_CAPACITY 262144
#define MATCH_BINDER_CAPACITY 262144
#define MATCH_FRAME_CAPACITY TERM_CAPACITY
#define JUDGEMENT_CAPACITY 4096
#define COMPILE_LABEL_CAPACITY 512
#define COMPILE_TYPE_EXPORT_CAPACITY 256
#define COMPILE_CONSTRUCTOR_EXPORT_CAPACITY 512
#define RESOLVE_ERROR_CAPACITY 512
#define COMPILE_DIAGNOSTIC_CAPACITY 512
#define RESOLUTION_ITEM_CAPACITY 2048
#define RESOLUTION_ITERATION_CAPACITY 128
#define RESOLUTION_EVENT_CAPACITY 2048
#define OPERATION_CAPACITY 4096
#define OCCURRENCE_EDGE_CAPACITY (OPERATION_CAPACITY * 8)
#define OPERATION_CASE_CAPACITY 4096
#define OPERATION_FOLD_CLAUSE_CAPACITY 4096
#define VERIFICATION_OBLIGATION_CAPACITY 4096
#define VERIFICATION_DEPENDENCY_CAPACITY 8192
#define DIMENSION_OPERATOR_CAPACITY 256
#define DIMENSION_IMAGE_CAPACITY 4096
#define FUNCTION_GRAPH_REQUEST_CAPACITY 128
#define FUNCTION_GRAPH_ASSOCIATION_CAPACITY 128
#define ARTIFACT_TERM_EXPORT_CAPACITY 512
#define ARTIFACT_TYPE_EXPORT_CAPACITY 256
#define ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY 512
#define ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY 512
#define ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY 1024
#define ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY 2048
#define ARTIFACT_IDENTITY_ROOT_CAPACITY 512
#define ARTIFACT_DEPENDENCY_CAPACITY 512
#define ARTIFACT_EXTERNAL_TERM_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_EXTERNAL_TERM_REF_CAPACITY 512
#define ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_EXTERNAL_TYPE_EXPR_REF_CAPACITY 512
#define ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY 1024
#define ARTIFACT_DEBUG_NAME_CAPACITY 1024
#define LINK_PROVIDER_CAPACITY 16
#define LINK_SEARCH_DIR_CAPACITY 8
#define LINK_AUTO_PROVIDER_PATH_CAPACITY 512
#define IMPORT_INTERFACE_CAPACITY 8
#define OPAQUE_EXPORT_CAPACITY 128
#define ARTIFACT_DEFINITION_CAPACITY 512


static const char* path_basename(const char* path) {
	const char* base = path;
	if (!path) {
		return "";
	}
	for (const char* p = path; *p; ++p) {
		if (*p == '/') {
			base = p + 1;
		}
	}
	return base;
}

static int namespace_symbol_from_text(
	struct symbol_table* symbols,
	const char* name
) {
	if (!symbols || !name || !*name) {
		return -1;
	}
	return symbol_intern(symbols, name, strlen(name));
}

static int namespace_symbol_from_path(
	struct symbol_table* symbols,
	const char* path
) {
	if (!symbols || !path || !*path) {
		return -1;
	}
	char buffer[256];
	size_t len = 0;
	const char* base = path_basename(path);
	while (base[len] && base[len] != '.' && len + 1 < sizeof(buffer)) {
		buffer[len] = base[len];
		len++;
	}
	if (len == 0) {
		return -1;
	}
	buffer[len] = '\0';
	return symbol_intern(symbols, buffer, len);
}

static int mark_opaque_export(
	struct symbol_table* symbols,
	struct prototype_artifact_interface* interface,
	const char* name
) {
	if (!symbols || !interface || !name) {
		return -1;
	}
	int symbol_id = symbol_intern(symbols, name, strlen(name));
	if (symbol_id < 0) {
		return -1;
	}
	uint32_t export_id;
	int found = prototype_artifact_interface_find_term_export(
		interface,
		symbol_id,
		&export_id
	);
	if (found != 0) {
		return -1;
	}
	interface->term_exports[export_id].transparency =
		PROTOTYPE_ARTIFACT_EXPORT_OPAQUE;
	return 0;
}

static void mark_reachable_external_refs(
	const struct prototype_term_db* term_db,
	unsigned char* reachable_external_refs,
	uint32_t term_id,
	unsigned depth
) {
	if (!term_db || !reachable_external_refs ||
		term_id >= term_db->term_count || depth > 256) {
		return;
	}
	const struct prototype_term* term = &term_db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			reachable_external_refs[term_id] = 1;
			break;
		case PROTOTYPE_TERM_APP:
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.app.function, depth + 1
			);
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.app.argument, depth + 1
			);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.lambda.body, depth + 1
			);
			break;
		case PROTOTYPE_TERM_MATCH:
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.match.scrutinee, depth + 1
			);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id < term_db->case_count) {
					mark_reachable_external_refs(
						term_db, reachable_external_refs,
						term_db->cases[case_id].body, depth + 1
					);
				}
			}
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.constructor.owner, depth + 1
			);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.type_view.core, depth + 1
			);
			mark_reachable_external_refs(
				term_db, reachable_external_refs, term->as.type_view.source, depth + 1
			);
			break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			mark_reachable_external_refs(
				term_db,
				reachable_external_refs,
				term->as.induction_hypothesis.argument,
				depth + 1
			);
			break;
		default:
			break;
	}
}

static const char* operation_tag_name(int tag) {
	switch (tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM: return "atom";
		case PROTOTYPE_TYPED_OCCURRENCE_VAR: return "var";
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE: return "name";
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR: return "constructor";
		case PROTOTYPE_TYPED_OCCURRENCE_APP: return "app";
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA: return "lambda";
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH: return "match";
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN: return "return";
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK: return "thunk";
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE: return "force";
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD: return "computation-fold";
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST: return "operation-request";
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS: return "induction-hypothesis";
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE: return "ascription";
		default: return "unknown";
	}
}

static const char* substitution_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY: return "identity";
		case PROTOTYPE_SUBSTITUTION_EMPTY: return "empty";
		case PROTOTYPE_SUBSTITUTION_PROJECTION: return "projection";
		case PROTOTYPE_SUBSTITUTION_EXTEND: return "extend";
		case PROTOTYPE_SUBSTITUTION_COMPOSE: return "compose";
		default: return "unknown";
	}
}

static void print_artifact_context_and_substitution_inspection(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	const struct prototype_artifact_interface* interface,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* term_db,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !intrinsic_environment || !interface ||
		!type_declarations || !term_db || !metadata) {
		return;
	}
	int default_integer_width = prototype_term_host_type_bit_width(
		interface->default_integer_host_type
	);
	fprintf(
		stream,
		"\n#### Static Context and Substitution ####\n"
		"contexts=%zu substitutions=%zu\n",
		metadata->contexts.context_count,
		metadata->substitutions.substitution_count
	);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		const struct prototype_context* context = &metadata->contexts.contexts[i];
		fprintf(
			stream,
			"context#%zu parent=context#%u depth=%u binding=",
			i,
			context->parent,
			context->depth
		);
		if (context->binding_id == PROTOTYPE_INVALID_ID) {
			fprintf(stream, "none");
		} else {
			fprintf(stream, "binding#%u", context->binding_id);
		}
		fprintf(stream, " classifier-equation=");
		if (context->classifier_equation == PROTOTYPE_INVALID_ID) {
			fprintf(stream, "none");
		} else {
			fprintf(stream, "equation#%u", context->classifier_equation);
		}
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		const struct prototype_substitution* substitution =
			&metadata->substitutions.substitutions[i];
		uint32_t evidence = prototype_compile_metadata_accepted_substitution_claim(
			metadata, (uint32_t)i
		);
		fprintf(
			stream,
			"substitution#%zu kind=%s source=context#%u target=context#%u "
			"first=substitution#%u second=substitution#%u term=term#%u "
			"evidence=",
			i,
			substitution_kind_name(substitution->kind),
			substitution->source_context,
			substitution->target_context,
			substitution->first,
			substitution->second,
			substitution->term
		);
		if (evidence == PROTOTYPE_INVALID_ID) {
			fprintf(stream, "none");
		} else {
			fprintf(stream, "claim#%u", evidence);
		}
		fprintf(stream, "\n");
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
			substitution->term < term_db->term_count) {
			fprintf(stream, "  core-value term#%u = ", substitution->term);
			prototype_type_term_print_debug(
				stream,
				symbols,
				intrinsic_environment,
				type_declarations,
				term_db,
				substitution->term
			);
			const struct prototype_term* term = &term_db->terms[substitution->term];
			if (term->tag == PROTOTYPE_TERM_INT_LITERAL) {
				fprintf(stream, " human-value=#%" PRId64, term->as.int_literal.value);
			} else if (term->tag == PROTOTYPE_TERM_TEXT_LITERAL) {
				fprintf(
					stream,
					" human-value=#\"%s\"",
					symbol_to_string(symbols, term->as.text_literal.text_symbol_id)
				);
			}
			fprintf(stream, "\n");
		}
	}
	fprintf(
		stream,
		"\n#### Runtime Environment Boundary ####\n"
		"intrinsic-environment operational=%" PRIu64 " typing=%" PRIu64
		" default-integer=#.Int%d\n",
		interface->operational_intrinsic_fingerprint,
		interface->typing_intrinsic_fingerprint,
		default_integer_width
	);
}

static int artifact_export_claim_ids_match_loaded_image(
	const struct prototype_artifact_interface* interface,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export =
			&interface->term_exports[i];
		if (export->source_evidence.kind ==
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CONDITIONAL) {
			if (export->source_condition_count == 0 ||
				(export->source_evidence.id != PROTOTYPE_INVALID_ID &&
				 export->source_evidence.id >= judgement->claim_count)) {
				return -1;
			}
			if (export->source_evidence.id == PROTOTYPE_INVALID_ID) {
				continue;
			}
		}
		if ((export->source_evidence.kind !=
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM &&
			 export->source_evidence.kind !=
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CONDITIONAL) ||
			export->source_evidence.id >= judgement->claim_count) {
			return -1;
		}
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, export->source_evidence.id);
		const struct prototype_judgement_proposition* proposition = claim ?
			prototype_judgement_proposition_get(judgement, claim->proposition_id) :
			NULL;
		if (!proposition || proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->subject != export->local_term ||
			proposition->classifier != export->classifier ||
			(proposition->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
			 (proposition->authority_id != export->occurrence ||
			  proposition->occurrence_id != export->occurrence))) {
			return -1;
		}
	}
	return 0;
}

static int artifact_exports_have_accepted_claims(
	struct prototype_artifact_interface* interface,
	struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	int rebind_source_claim_ids
) {
	if (!interface || !terms || !judgement || !metadata) {
		return -1;
	}
	(void)terms;
	return rebind_source_claim_ids ?
		prototype_artifact_interface_refresh_term_export_evidence(
			interface, metadata, judgement
		) :
		prototype_artifact_interface_validate_term_export_evidence(
			interface, metadata, judgement
		);
}

static int decode_artifact_interface_and_graph(
	const char* path,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	struct prototype_artifact_interface* artifact_interface,
	struct prototype_term_db* term_db,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement_db,
	struct prototype_universe_db* universe_db,
	struct prototype_compile_metadata* metadata
) {
	if (!path || !symbols || !intrinsic_environment || !artifact_interface || !term_db ||
		!type_declarations || !judgement_db || !universe_db || !metadata) {
		return -1;
	}
	FILE* artifact_file = fopen(path, "r");
	if (!artifact_file) {
		return -1;
	}
	struct prototype_artifact_graph_decode_state decode_state;
	prototype_artifact_graph_decode_state_init(&decode_state);
	int status = 0;
	if (prototype_artifact_read_text_interface(
			artifact_file,
			symbols,
			intrinsic_environment,
			artifact_interface
		) != 0 ||
		prototype_artifact_read_text_graph(
			artifact_file,
			symbols,
			intrinsic_environment,
			artifact_interface,
			&metadata->dimension_operators,
			term_db,
			type_declarations,
			judgement_db,
			&decode_state
		) != 0 ||
		prototype_artifact_read_text_typed_occurrences(
			artifact_file,
			symbols,
			term_db,
			type_declarations,
			judgement_db,
			metadata,
			&decode_state
		) != 0 ||
		prototype_artifact_read_text_universe(
			artifact_file,
			universe_db
		) != 0 || prototype_artifact_interface_validate_semantic_keys(
			artifact_interface,
			symbols,
			term_db,
			type_declarations,
			&metadata->dimension_operators
		) != 0 || artifact_export_claim_ids_match_loaded_image(
			artifact_interface, judgement_db
		) != 0 ||
		(metadata && prototype_constructor_curried_caches_validate(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache,
			&(const struct prototype_context_classifier_view) {
				.contexts = &metadata->contexts
			},
			term_db
		) != 0) ||
		prototype_judgement_validate_accepted_graph(
			term_db,
			type_declarations,
			intrinsic_environment,
			&metadata->contexts,
			&(const struct prototype_context_classifier_view) {
				.contexts = &metadata->contexts
			},
			&metadata->substitutions,
			&metadata->dimension_operators,
			&metadata->typed_occurrences,
			&(const struct prototype_judgement_occurrence_context_view) {
				.publication = &metadata->typed_publication
			},
			judgement_db
		) != 0 || prototype_artifact_interface_validate_identity_roots(
			artifact_interface,
			term_db,
			type_declarations,
			&metadata->contexts,
			&metadata->dimension_operators,
			judgement_db
		) != 0 || prototype_artifact_interface_validate_function_graph_associations(
			artifact_interface,
			term_db,
			type_declarations,
			&metadata->contexts,
			judgement_db
	) != 0 || prototype_universe_validate_replay(
			universe_db,
			type_declarations,
			term_db,
			&metadata->typed_occurrences,
			judgement_db
		) != 0 || artifact_exports_have_accepted_claims(
			artifact_interface, term_db, judgement_db, metadata, 0
		) != 0) {
		status = -1;
	}
	if (status == 0 && metadata->selected_entry_symbol_id >= 0) {
		uint32_t export_id;
		if (prototype_artifact_interface_find_term_export(
				artifact_interface,
				metadata->selected_entry_symbol_id,
				&export_id
			) != 0 || export_id >= artifact_interface->term_export_count ||
			metadata->selected_entry_term >= term_db->term_count ||
			term_db->terms[metadata->selected_entry_term].tag !=
				PROTOTYPE_TERM_FORCE ||
			term_db->terms[metadata->selected_entry_term].as.force.value !=
				artifact_interface->term_exports[export_id].local_term) {
			status = -1;
		}
	}
	if (fclose(artifact_file) != 0) {
		status = -1;
	}
	prototype_artifact_graph_decode_state_destroy(&decode_state);
	return status;
}

static int clone_symbol_table_contents(
	struct symbol_table* target,
	const struct symbol_table* source
) {
	if (!target || !source || target->storage.count != 0) {
		return -1;
	}
	for (size_t i = 0; i < source->storage.count; ++i) {
		const char* name = symbol_to_string(source, (int)i);
		if (!name || symbol_intern(target, name, strlen(name)) != (int)i) {
			return -1;
		}
	}
	return 0;
}

static int publish_symbol_table_suffix(
	struct symbol_table* target,
	struct symbol_table* staged
) {
	if (!target || !staged || target->storage.count > staged->storage.count ||
		target->map.capacity != staged->map.capacity ||
		staged->storage.count > target->storage.capacity ||
		staged->map.count > target->map.capacity) {
		return -1;
	}
	for (size_t i = 0; i < target->storage.count; ++i) {
		const char* existing = symbol_to_string(target, (int)i);
		const char* candidate = symbol_to_string(staged, (int)i);
		if (!existing || !candidate || strcmp(existing, candidate) != 0) {
			return -1;
		}
	}
	for (size_t i = target->storage.count; i < staged->storage.count; ++i) {
		target->storage.strings[i] = staged->storage.strings[i];
		staged->storage.strings[i] = NULL;
	}
	memcpy(target->map.symbol_ids, staged->map.symbol_ids,
		staged->map.capacity * sizeof(*target->map.symbol_ids));
	memcpy(target->map.hashes, staged->map.hashes,
		staged->map.capacity * sizeof(*target->map.hashes));
	target->storage.count = staged->storage.count;
	target->map.count = staged->map.count;
	return 0;
}

static int publish_universe_image(
	struct prototype_universe_db* target,
	const struct prototype_universe_db* staged
) {
	if (!target || !staged || staged->node_count > target->node_capacity ||
		staged->edge_count > target->edge_capacity ||
		staged->level_count > target->level_capacity ||
		staged->constraint_count > target->constraint_capacity ||
		staged->obligation_span_count > target->obligation_span_capacity) {
		return -1;
	}
	memcpy(target->nodes, staged->nodes,
		staged->node_count * sizeof(*target->nodes));
	memcpy(target->edges, staged->edges,
		staged->edge_count * sizeof(*target->edges));
	memcpy(target->levels, staged->levels,
		staged->level_count * sizeof(*target->levels));
	memcpy(target->constraints, staged->constraints,
		staged->constraint_count * sizeof(*target->constraints));
	memcpy(target->obligation_spans, staged->obligation_spans,
		staged->obligation_span_count * sizeof(*target->obligation_spans));
	target->node_count = staged->node_count;
	target->edge_count = staged->edge_count;
	target->level_count = staged->level_count;
	target->constraint_count = staged->constraint_count;
	target->obligation_span_count = staged->obligation_span_count;
	target->certificate = staged->certificate;
	return 0;
}

static int read_artifact_interface_and_graph(
	const char* path,
	struct symbol_table* symbol_authority,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	struct prototype_artifact_interface_storage* artifact_storage,
	struct prototype_program_storage* program_storage,
	struct prototype_universe_db* universe_authority
) {
	if (!path || !symbol_authority || !intrinsic_environment ||
		!artifact_storage || !program_storage || !universe_authority) {
		return -1;
	}
	struct prototype_program_storage staged_program;
	struct prototype_artifact_interface_storage staged_artifact;
	memset(&staged_program, 0, sizeof(staged_program));
	memset(&staged_artifact, 0, sizeof(staged_artifact));
	if (prototype_program_storage_init(&staged_program) != 0 ||
		prototype_artifact_interface_storage_init(&staged_artifact) != 0 ||
		clone_symbol_table_contents(
			&staged_program.private->symbols, symbol_authority
		) != 0) {
		prototype_artifact_interface_storage_destroy(&staged_artifact);
		prototype_program_storage_destroy(&staged_program);
		return -1;
	}
	staged_program.private->program.intrinsic_environment = intrinsic_environment;
	int status = decode_artifact_interface_and_graph(
		path,
		&staged_program.private->symbols,
		intrinsic_environment,
		&staged_artifact.interface,
		&staged_program.private->core.terms,
		&staged_program.private->type_declarations,
		&staged_program.private->judgement,
		&staged_program.private->universe,
		&staged_program.private->metadata
	);
	if (status == 0 && symbol_authority != &program_storage->private->symbols) {
		status = publish_symbol_table_suffix(
			symbol_authority, &staged_program.private->symbols
		);
	}
	if (status == 0 && universe_authority != &program_storage->private->universe) {
		status = publish_universe_image(
			universe_authority, &staged_program.private->universe
		);
	}
	if (status == 0) {
		prototype_program_storage_exchange(program_storage, &staged_program);
		prototype_artifact_interface_storage_exchange(
			artifact_storage, &staged_artifact
		);
	}
	prototype_artifact_interface_storage_destroy(&staged_artifact);
	prototype_program_storage_destroy(&staged_program);
	return status;
}

static uint32_t offset_link_graph_id(uint32_t id, uint32_t offset) {
	return id == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : id + offset;
}

static int append_link_typed_occurrence_graph(
	struct prototype_compile_metadata* target,
	const struct prototype_compile_metadata* source,
	const struct prototype_term_db* target_terms,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	const uint32_t* context_relocation,
	size_t context_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
	const uint32_t* substitution_relocation,
	size_t substitution_relocation_count
) {
	if (!target || !source || !target_terms || !term_relocation || !context_relocation ||
		!binding_relocation || !substitution_relocation ||
		!source->typed_publication.sealed ||
		prototype_verification_db_count(&target->verification) +
			prototype_verification_db_count(&source->verification) >
			prototype_verification_db_capacity(&target->verification)) {
		return -1;
	}
	if (prototype_typed_publication_view_begin_extension(
			&target->typed_publication
		) != 0) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* target_graph =
		prototype_compile_metadata_typed_occurrences(target);
	const struct prototype_typed_occurrence_graph* source_graph =
		prototype_compile_metadata_typed_occurrences_const(source);
	if (prototype_typed_occurrence_graph_count(target_graph) +
			prototype_typed_occurrence_graph_count(source_graph) >
			target_graph->occurrence_capacity ||
		target_graph->edge_count + source_graph->edge_count >
			target_graph->edge_capacity ||
		prototype_typed_occurrence_graph_case_count(target_graph) +
			prototype_typed_occurrence_graph_case_count(source_graph) >
			target_graph->case_capacity ||
		target_graph->fold_clause_count + source_graph->fold_clause_count >
			target_graph->fold_clause_capacity ||
		target->verification.obligation_count +
			source->verification.obligation_count >
			target->verification.obligation_capacity ||
		target->verification.dependency_count +
			source->verification.dependency_count >
			target->verification.dependency_capacity) {
		return -1;
	}
	int target_is_empty = prototype_typed_occurrence_graph_count(target_graph) == 0 &&
		target->solver_constraint_count == 0 &&
		prototype_verification_db_count(&target->verification) == 0;
	if (target->selected_entry_occurrence != PROTOTYPE_INVALID_ID &&
		source->selected_entry_occurrence != PROTOTYPE_INVALID_ID) {
		/* Aggregation has no syntax for choosing between executable entries. */
		return -1;
	}
	if (target_is_empty) {
		target->compile_policy = source->compile_policy;
		target->definition_thunk_policy = source->definition_thunk_policy;
	} else if (target->compile_policy != source->compile_policy) {
		return -1;
	}
	if (UINT64_MAX - target->solver_constraint_count < source->solver_constraint_count ||
		UINT64_MAX - target->solver_solved_count < source->solver_solved_count ||
		UINT64_MAX - target->solver_residual_count < source->solver_residual_count ||
		UINT64_MAX - target->solver_incomplete_count < source->solver_incomplete_count) {
		return -1;
	}
	if (prototype_effort_account_accumulate_snapshot(
			&target->effort, &source->effort
		) != 0) {
		return -1;
	}
	target->solver_constraint_count += source->solver_constraint_count;
	target->solver_solved_count += source->solver_solved_count;
	target->solver_residual_count += source->solver_residual_count;
	target->solver_incomplete_count += source->solver_incomplete_count;
	target->required_runtime_capabilities |= source->required_runtime_capabilities;
	uint32_t occurrence_offset =
		(uint32_t)prototype_typed_occurrence_graph_count(target_graph);
	uint32_t edge_offset = (uint32_t)target_graph->edge_count;
	uint32_t case_offset =
		(uint32_t)prototype_typed_occurrence_graph_case_count(target_graph);
	uint32_t fold_clause_offset = (uint32_t)target_graph->fold_clause_count;
	uint32_t obligation_offset =
		(uint32_t)target->verification.obligation_count;
	for (size_t i = 0; i < prototype_typed_occurrence_graph_count(source_graph); ++i) {
		const struct prototype_typed_occurrence* source_operation =
			prototype_typed_occurrence_graph_get(source_graph, (uint32_t)i);
		uint32_t source_accepted_context = PROTOTYPE_INVALID_ID;
		const struct prototype_typed_publication_projection* source_projection;
		int publication_status = prototype_typed_publication_view_lookup(
			&source->typed_publication,
			(uint32_t)i,
			&source_projection,
			&source_accepted_context
		);
		if (!source_operation || publication_status < 0 ||
			(publication_status == 0 &&
			 (source_accepted_context >= context_relocation_count ||
			  context_relocation[source_accepted_context] ==
				PROTOTYPE_INVALID_ID))) {
			return -1;
		}
		struct prototype_typed_occurrence operation = *source_operation;
		if (operation.edge_count == 0) {
			operation.first_edge = PROTOTYPE_INVALID_ID;
		} else if (operation.first_edge == PROTOTYPE_INVALID_ID ||
			operation.first_edge > source_graph->edge_count ||
			operation.edge_count > source_graph->edge_count -
				operation.first_edge) {
			return -1;
		} else {
			operation.first_edge += edge_offset;
		}
		if (operation.context_id >= source->contexts.context_count ||
			operation.context_id >= context_relocation_count ||
			context_relocation[operation.context_id] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		operation.context_id = context_relocation[operation.context_id];
#define RELOCATE_TERM_FIELD(field) \
	do { \
		if ((field) != PROTOTYPE_INVALID_ID) { \
			if ((field) >= term_relocation_count || \
				term_relocation[(field)] == PROTOTYPE_INVALID_ID) { \
				return -1; \
			} \
			(field) = term_relocation[(field)]; \
		} \
	} while (0)
#define RELOCATE_BINDING_FIELD(field) \
	do { \
		if ((field) != PROTOTYPE_INVALID_ID) { \
			if ((field) >= binding_relocation_count || \
				binding_relocation[(field)] == PROTOTYPE_INVALID_ID) { \
				return -1; \
			} \
			(field) = binding_relocation[(field)]; \
		} \
	} while (0)
		RELOCATE_TERM_FIELD(operation.core_term);
		RELOCATE_TERM_FIELD(operation.classifier);
		RELOCATE_TERM_FIELD(operation.binder_classifier);
		RELOCATE_TERM_FIELD(operation.source_core_term);
		RELOCATE_TERM_FIELD(operation.source_classifier);
		if (operation.context_action_substitution != PROTOTYPE_INVALID_ID) {
			if (operation.context_action_substitution >=
					substitution_relocation_count ||
				substitution_relocation[
					operation.context_action_substitution
				] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			operation.context_action_substitution = substitution_relocation[
				operation.context_action_substitution
			];
		}
		if (operation.tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			if (operation.core_term >= target_terms->term_count ||
				target_terms->terms[operation.core_term].tag !=
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				return -1;
			}
			operation.ih_scope_id = target_terms->terms[
				operation.core_term
			].as.induction_hypothesis.ih_scope_id;
		}
		RELOCATE_BINDING_FIELD(operation.fold_return_binder_id);
		operation.wrapped_occurrence = offset_link_graph_id(
			operation.wrapped_occurrence, occurrence_offset
		);
		operation.ih_owner_occurrence = offset_link_graph_id(
			operation.ih_owner_occurrence, occurrence_offset
		);
		operation.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
		RELOCATE_BINDING_FIELD(operation.binding_id);
		operation.fold_return_ast_binder_id = PROTOTYPE_INVALID_ID;
		if (operation.tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			operation.first_case = offset_link_graph_id(
				operation.first_case, case_offset
			);
		}
		operation.first_fold_clause = offset_link_graph_id(
			operation.first_fold_clause, fold_clause_offset
		);
		for (uint32_t j = 0; j < operation.implicit_effect_row_count; ++j) {
			RELOCATE_BINDING_FIELD(operation.implicit_effect_row_binders[j]);
		}
		if (publication_status == 1) {
			if (prototype_typed_publication_view_mark_unreachable(
					&target->typed_publication,
					occurrence_offset + (uint32_t)i
				) != 0) {
				return -1;
			}
		} else if (source_projection->substitution >=
				substitution_relocation_count ||
			substitution_relocation[source_projection->substitution] ==
				PROTOTYPE_INVALID_ID ||
			source_projection->subject >= term_relocation_count ||
			term_relocation[source_projection->subject] == PROTOTYPE_INVALID_ID ||
			source_projection->classifier >= term_relocation_count ||
			term_relocation[source_projection->classifier] == PROTOTYPE_INVALID_ID ||
			prototype_typed_publication_view_put(
				&target->typed_publication,
				occurrence_offset + (uint32_t)i,
				PROTOTYPE_INVALID_ID,
				target->substitutions.substitutions[
					substitution_relocation[source_projection->substitution]
				].source_context,
				substitution_relocation[source_projection->substitution],
				term_relocation[source_projection->subject],
				term_relocation[source_projection->classifier]
			) != 0) {
			return -1;
		}
		if (prototype_typed_occurrence_graph_add(
				target_graph, &target->contexts, operation, NULL
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < source_graph->edge_count; ++i) {
		const struct prototype_typed_occurrence_edge* source_edge =
			prototype_typed_occurrence_graph_get_edge(source_graph, (uint32_t)i);
		if (!source_edge || source_edge->child_occurrence >=
			prototype_typed_occurrence_graph_count(source_graph)) {
			return -1;
		}
		struct prototype_typed_occurrence_edge edge = *source_edge;
		edge.child_occurrence += occurrence_offset;
		target_graph->edges[target_graph->edge_count++] = edge;
	}
	for (size_t i = 0; i < source_graph->fold_clause_count; ++i) {
		const struct prototype_typed_occurrence_fold_clause* source_clause =
			prototype_typed_occurrence_graph_get_fold_clause(source_graph, (uint32_t)i);
		if (!source_clause || source_clause->context_id >=
				source->contexts.context_count || source_clause->context_id >=
				context_relocation_count) {
			return -1;
		}
		struct prototype_typed_occurrence_fold_clause clause = *source_clause;
		clause.context_id = context_relocation[clause.context_id];
		clause.argument_ast_binder_id = PROTOTYPE_INVALID_ID;
		clause.continuation_ast_binder_id = PROTOTYPE_INVALID_ID;
		RELOCATE_BINDING_FIELD(clause.argument_binder_id);
		RELOCATE_BINDING_FIELD(clause.continuation_binder_id);
		if (prototype_typed_occurrence_graph_add_fold_clause(
				target_graph, &target->contexts, clause, NULL
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0;
		i < prototype_typed_occurrence_graph_case_count(source_graph);
		++i) {
		const struct prototype_typed_occurrence_match_case* source_case =
			prototype_typed_occurrence_graph_get_case(source_graph, (uint32_t)i);
		const struct prototype_typed_publication_match_case_projection*
			source_projection = prototype_typed_publication_view_get_match_case(
				&source->typed_publication, (uint32_t)i
			);
		if (!source_case || !source_projection ||
			source_case->context_id >= context_relocation_count ||
			context_relocation[source_case->context_id] == PROTOTYPE_INVALID_ID ||
			source_projection->concrete_context >= context_relocation_count ||
			context_relocation[source_projection->concrete_context] ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		struct prototype_typed_occurrence_match_case operation_case = *source_case;
		operation_case.context_id =
			context_relocation[source_case->context_id];
		if (operation_case.refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED) {
			if (source_projection->refinement_substitution !=
					operation_case.refinement_substitution ||
				operation_case.refinement_substitution >=
					substitution_relocation_count ||
				substitution_relocation[
					operation_case.refinement_substitution
				] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			operation_case.refinement_substitution = substitution_relocation[
				operation_case.refinement_substitution
			];
		}
		RELOCATE_TERM_FIELD(operation_case.constructor_owner);
		for (uint32_t j = 0; j < operation_case.binder_count; ++j) {
			operation_case.ast_binder_ids[j] = PROTOTYPE_INVALID_ID;
			RELOCATE_BINDING_FIELD(operation_case.binder_ids[j]);
		}
		uint32_t publication_case_context = context_relocation[
			source_projection->concrete_context
		];
		if (operation_case.refinement_substitution != PROTOTYPE_INVALID_ID) {
			publication_case_context = target->substitutions.substitutions[
				operation_case.refinement_substitution
			].source_context;
		}
		if (prototype_typed_publication_view_put_match_case(
				&target->typed_publication,
				case_offset + (uint32_t)i,
				PROTOTYPE_INVALID_ID,
				publication_case_context,
				operation_case.refinement_substitution
			) != 0) {
			return -1;
		}
		if (prototype_typed_occurrence_graph_add_case(
				target_graph, &target->contexts, operation_case, NULL
			) != 0) {
			return -1;
		}
	}
	target->effect_constraint_summary.solved_count +=
		source->effect_constraint_summary.solved_count;
	target->effect_constraint_summary.residual_count +=
		source->effect_constraint_summary.residual_count;
	target->effect_constraint_summary.failed_count +=
		source->effect_constraint_summary.failed_count;
	for (size_t i = 0;
		i < prototype_verification_db_count(&source->verification);
		++i) {
		const struct prototype_verification_obligation* source_obligation =
			prototype_verification_db_get(&source->verification, (uint32_t)i);
		if (!source_obligation) {
			return -1;
		}
		struct prototype_verification_obligation obligation = *source_obligation;
		obligation.occurrence = offset_link_graph_id(obligation.occurrence, occurrence_offset);
		RELOCATE_TERM_FIELD(obligation.core_term);
		obligation.computation_occurrence = offset_link_graph_id(
			obligation.computation_occurrence, occurrence_offset
		);
		obligation.continuation_occurrence = offset_link_graph_id(
			obligation.continuation_occurrence, occurrence_offset
		);
		RELOCATE_BINDING_FIELD(obligation.continuation_binder_id);
		RELOCATE_TERM_FIELD(obligation.input_classifier);
		RELOCATE_TERM_FIELD(obligation.classifier_family);
		RELOCATE_TERM_FIELD(obligation.effect_row);
		uint32_t obligation_id;
		if (prototype_verification_db_add(
				&target->verification, obligation, &obligation_id
			) != 0 || obligation_id != obligation_offset + i) {
			return -1;
		}
	}
	for (size_t i = 0; i < source->verification.dependency_count; ++i) {
		const struct prototype_verification_dependency* dependency =
			&source->verification.dependencies[i];
		if (dependency->obligation_id >= source->verification.obligation_count ||
			prototype_verification_db_add_dependency(
				&target->verification,
				offset_link_graph_id(dependency->occurrence, occurrence_offset),
				obligation_offset + dependency->obligation_id
			) != 0) {
			return -1;
		}
	}
	if (source->selected_entry_occurrence != PROTOTYPE_INVALID_ID) {
		target->selected_entry_symbol_id = source->selected_entry_symbol_id;
		target->selected_entry_term = source->selected_entry_term;
		target->selected_entry_classifier = source->selected_entry_classifier;
		RELOCATE_TERM_FIELD(target->selected_entry_term);
		RELOCATE_TERM_FIELD(target->selected_entry_classifier);
		target->selected_entry_occurrence = offset_link_graph_id(
			source->selected_entry_occurrence, occurrence_offset
		);
	}
#undef RELOCATE_BINDING_FIELD
#undef RELOCATE_TERM_FIELD
	return prototype_typed_publication_view_seal(
		&target->typed_publication,
		target_graph,
		0,
		0
	);
}

static int read_artifact_interface_only(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* artifact_interface
) {
	if (!path || !symbols || !artifact_interface) {
		return -1;
	}
	FILE* artifact_file = fopen(path, "r");
	if (!artifact_file) {
		return -1;
	}
	int status = prototype_artifact_read_text_interface(
		artifact_file,
		symbols,
		prototype_default_intrinsic_typing_environment(),
		artifact_interface
	);
	if (fclose(artifact_file) != 0) {
		status = -1;
	}
	return status;
}

struct read_artifact_graph_storage {
	struct prototype_program_storage program;
	struct prototype_artifact_interface_storage artifact;
};

struct read_driver_storage {
	struct prototype_program_storage local_program;
	struct prototype_program_storage provider_program;
	struct prototype_artifact_interface_storage artifact;
	struct prototype_artifact_interface_storage provider_artifact;
	struct prototype_artifact_interface_storage appended_artifact;
	struct prototype_artifact_interface_storage
		imported_artifacts[IMPORT_INTERFACE_CAPACITY];
	char auto_link_provider_paths[LINK_PROVIDER_CAPACITY][
		LINK_AUTO_PROVIDER_PATH_CAPACITY
	];
	unsigned char* reachable_external_refs;
};

static void read_driver_storage_destroy(struct read_driver_storage* storage) {
	if (!storage) {
		return;
	}
	for (size_t i = 0; i < IMPORT_INTERFACE_CAPACITY; ++i) {
		prototype_artifact_interface_storage_destroy(
			&storage->imported_artifacts[i]
		);
	}
	prototype_artifact_interface_storage_destroy(&storage->appended_artifact);
	prototype_artifact_interface_storage_destroy(&storage->provider_artifact);
	prototype_artifact_interface_storage_destroy(&storage->artifact);
	prototype_program_storage_destroy(&storage->provider_program);
	prototype_program_storage_destroy(&storage->local_program);
	free(storage->reachable_external_refs);
	storage->reachable_external_refs = NULL;
}

static int read_driver_storage_init(struct read_driver_storage* storage) {
	if (!storage) {
		return -1;
	}
	memset(storage, 0, sizeof(*storage));
	storage->reachable_external_refs = calloc(
		TERM_CAPACITY, sizeof(*storage->reachable_external_refs)
	);
	if (!storage->reachable_external_refs) {
		return -1;
	}
	if (prototype_program_storage_init(&storage->local_program) != 0 ||
		prototype_program_storage_init(&storage->provider_program) != 0 ||
		prototype_artifact_interface_storage_init(&storage->artifact) != 0 ||
		prototype_artifact_interface_storage_init(&storage->provider_artifact) != 0 ||
		prototype_artifact_interface_storage_init(&storage->appended_artifact) != 0) {
		read_driver_storage_destroy(storage);
		return -1;
	}
	for (size_t i = 0; i < IMPORT_INTERFACE_CAPACITY; ++i) {
		if (prototype_artifact_interface_storage_init(
				&storage->imported_artifacts[i]
			) != 0) {
			read_driver_storage_destroy(storage);
			return -1;
		}
	}
	return 0;
}

static int read_artifact_graph_storage_init(
	struct read_artifact_graph_storage* storage
) {
	if (!storage) {
		return -1;
	}
	memset(storage, 0, sizeof(*storage));
	if (prototype_program_storage_init(&storage->program) != 0) {
		return -1;
	}
	if (prototype_artifact_interface_storage_init(&storage->artifact) != 0) {
		prototype_program_storage_destroy(&storage->program);
		return -1;
	}
	return 0;
}

static void read_artifact_graph_storage_destroy(
	struct read_artifact_graph_storage* storage
) {
	if (!storage) {
		return;
	}
	prototype_artifact_interface_storage_destroy(&storage->artifact);
	prototype_program_storage_destroy(&storage->program);
}

static int read_artifact_graph_storage_load(
	const char* path,
	struct read_artifact_graph_storage* storage
) {
	if (!path || !storage) {
		return -1;
	}
	return read_artifact_interface_and_graph(
		path,
		&storage->program.private->symbols,
		storage->program.private->program.intrinsic_environment,
		&storage->artifact,
		&storage->program,
		&storage->program.private->universe
	);
}

static void artifact_audit_hash_bytes(
	uint64_t* p_hash,
	const void* data,
	size_t size
) {
	const unsigned char* bytes = data;
	for (size_t i = 0; i < size; ++i) {
		*p_hash ^= bytes[i];
		*p_hash *= UINT64_C(1099511628211);
	}
}

static uint64_t artifact_loaded_image_audit_fingerprint(
	const struct read_artifact_graph_storage* storage
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	if (!storage) {
		return 0;
	}
#define HASH_VALUE(value) artifact_audit_hash_bytes(&hash, &(value), sizeof(value))
#define HASH_ARRAY(pointer, count) \
	artifact_audit_hash_bytes(&hash, (pointer), (count) * sizeof(*(pointer)))
	const struct prototype_program_storage_private* program =
		storage->program.private;
	if (!program) {
		return 0;
	}
	HASH_VALUE(program->core.terms);
	HASH_ARRAY(program->core.terms.terms, program->core.terms.term_count);
	HASH_ARRAY(program->core.terms.cases, program->core.terms.case_count);
	HASH_ARRAY(
		program->core.terms.case_label_symbols,
		program->core.terms.case_count
	);
	HASH_ARRAY(
		program->core.terms.case_binders,
		program->core.terms.case_binder_count
	);
	HASH_ARRAY(program->core.terms.ih_scopes, program->core.terms.ih_scope_count);
	HASH_VALUE(program->type_declarations);
	HASH_ARRAY(
		program->type_declarations.semantic_schema.type_declarations,
		program->type_declarations.semantic_schema.type_count
	);
	HASH_ARRAY(
		program->type_declarations.semantic_schema.constructor_declarations,
		program->type_declarations.semantic_schema.constructor_count
	);
	HASH_ARRAY(
		program->type_declarations.readback.parameter_declarations,
		program->type_declarations.readback.parameter_count
	);
	HASH_ARRAY(
		program->type_declarations.readback.field_types,
		program->type_declarations.readback.field_type_count
	);
	HASH_ARRAY(
		program->type_declarations.readback.exprs,
		program->type_declarations.readback.expr_count
	);
	HASH_ARRAY(
		program->type_declarations.representation_db.representations,
		program->type_declarations.representation_db.representation_count
	);
	HASH_VALUE(program->judgement);
	HASH_ARRAY(program->judgement.propositions, program->judgement.proposition_count);
	HASH_ARRAY(program->judgement.claims, program->judgement.claim_count);
	HASH_ARRAY(program->judgement.derivations, program->judgement.derivation_count);
	HASH_ARRAY(
		program->judgement.accepted_premises,
		program->judgement.accepted_premise_count
	);
	HASH_VALUE(program->metadata);
	HASH_ARRAY(
		program->metadata.typed_occurrences.occurrences,
		program->metadata.typed_occurrences.occurrence_count
	);
	HASH_ARRAY(
		program->metadata.typed_occurrences.edges,
		program->metadata.typed_occurrences.edge_count
	);
	HASH_ARRAY(
		program->metadata.contexts.contexts,
		program->metadata.contexts.context_count
	);
	HASH_ARRAY(
		program->metadata.substitutions.substitutions,
		program->metadata.substitutions.substitution_count
	);
	HASH_VALUE(program->universe);
	HASH_ARRAY(program->universe.nodes, program->universe.node_count);
	HASH_ARRAY(program->universe.edges, program->universe.edge_count);
	HASH_ARRAY(program->universe.levels, program->universe.level_count);
	HASH_ARRAY(program->universe.constraints, program->universe.constraint_count);
	HASH_VALUE(storage->artifact.interface);
	HASH_ARRAY(
		storage->artifact.interface.term_exports,
		storage->artifact.interface.term_export_count
	);
	HASH_ARRAY(
		storage->artifact.interface.type_exports,
		storage->artifact.interface.type_export_count
	);
	HASH_ARRAY(
		storage->artifact.interface.constructor_exports,
		storage->artifact.interface.constructor_export_count
	);
	HASH_VALUE(program->symbols);
	for (size_t i = 0; i < program->symbols.storage.count; ++i) {
		const char* name = symbol_to_string(&program->symbols, (int)i);
		if (name) {
			artifact_audit_hash_bytes(&hash, name, strlen(name) + 1);
		}
	}
#undef HASH_ARRAY
#undef HASH_VALUE
	return hash;
}

static int audit_artifact_transaction_rejection(
	const char* valid_path,
	const char* malformed_path
) {
	struct read_artifact_graph_storage storage;
	if (read_artifact_graph_storage_init(&storage) != 0) {
		return 1;
	}
	int status = read_artifact_graph_storage_load(valid_path, &storage);
	uint64_t before = status == 0 ?
		artifact_loaded_image_audit_fingerprint(&storage) : 0;
	if (status != 0 || read_artifact_graph_storage_load(
			malformed_path, &storage
		) == 0 || before != artifact_loaded_image_audit_fingerprint(&storage)) {
		status = 1;
	} else {
		status = 0;
	}
	read_artifact_graph_storage_destroy(&storage);
	return status;
}

static int check_export_normalization_equal(
	const char* path,
	const char* name
) {
	if (!path || !name) {
		return -1;
	}
	struct read_artifact_graph_storage storage;
	struct prototype_term_definition_env definition_env;
	if (read_artifact_graph_storage_init(&storage) != 0) {
		return 1;
	}
	if (read_artifact_graph_storage_load(path, &storage) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
			&storage.artifact.interface,
			prototype_artifact_interface_storage_definitions(&storage.artifact),
			prototype_artifact_interface_storage_definition_capacity(
				&storage.artifact
			),
			&definition_env
		) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int symbol_id = symbol_intern(&storage.program.private->symbols, name, strlen(name));
	uint32_t export_id;
	if (symbol_id < 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			symbol_id,
			&export_id
		) != 0) {
		fprintf(stderr, "%s: unknown term export: %s\n", path, name);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	uint32_t external_ref;
	struct prototype_term_conversion_result conversion;
	if (prototype_term_external_ref(
			&storage.program.private->core.terms,
			(struct prototype_qualified_name){
				storage.artifact.interface.term_exports[
					export_id
				].namespace_symbol_id,
				symbol_id
			},
			&external_ref
		) != 0 ||
		prototype_term_compare_with_options(
			&storage.program.private->core.terms,
			&definition_env,
			(struct prototype_term_reduction_options){
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT |
					PROTOTYPE_TERM_REDUCE_DEFINITIONS
			},
			external_ref,
			storage.artifact.interface.term_exports[export_id].local_term,
			UINT64_MAX,
			&conversion
		) != 0) {
		fprintf(stderr, "%s: failed to check export normalization equality: %s\n", path, name);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	printf("export-normalization-equal %s %s\n",
		name,
		conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL ? "yes" : "no");
	printf("conversion-status %s reason=%s steps=%" PRIu64 "\n",
		prototype_term_conversion_status_name(conversion.status),
		prototype_term_conversion_reason_name(conversion.reason),
		conversion.steps_used);
	read_artifact_graph_storage_destroy(&storage);
	return 0;
}

static int reduction_options_from_mode(
	const char* mode,
	struct prototype_term_reduction_options* p_options
) {
	if (p_options) {
		memset(p_options, 0, sizeof(*p_options));
	}
	if (!mode || !p_options || strcmp(mode, "default") == 0) {
		if (p_options) {
			p_options->flags =
				PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		}
		return p_options ? 0 : -1;
	}
	if (strcmp(mode, "beta") == 0) {
		p_options->flags =
			PROTOTYPE_TERM_REDUCE_BETA | PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	if (strcmp(mode, "match") == 0) {
		p_options->flags =
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION |
			PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	if (strcmp(mode, "none") == 0) {
		p_options->flags = PROTOTYPE_TERM_REDUCE_DEFINITIONS;
		return 0;
	}
	return -1;
}

static int check_exports_normalization_equal(
	const char* path,
	const char* left_name,
	const char* right_name,
	const char* reduction_mode
) {
	if (!path || !left_name || !right_name) {
		return -1;
	}
	struct prototype_term_reduction_options options;
	if (reduction_options_from_mode(reduction_mode, &options) != 0) {
		fprintf(stderr, "unknown reduction mode: %s\n", reduction_mode ? reduction_mode : "<null>");
		return 1;
	}
	struct read_artifact_graph_storage storage;
	struct prototype_term_definition_env definition_env;
	if (read_artifact_graph_storage_init(&storage) != 0) {
		return 1;
	}
	if (read_artifact_graph_storage_load(path, &storage) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
			&storage.artifact.interface,
			prototype_artifact_interface_storage_definitions(&storage.artifact),
			prototype_artifact_interface_storage_definition_capacity(
				&storage.artifact
			),
			&definition_env
		) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int left_symbol = symbol_intern(
		&storage.program.private->symbols, left_name, strlen(left_name)
	);
	int right_symbol = symbol_intern(
		&storage.program.private->symbols, right_name, strlen(right_name)
	);
	uint32_t left_export;
	uint32_t right_export;
	if (left_symbol < 0 || right_symbol < 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			left_symbol,
			&left_export
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			right_symbol,
			&right_export
		) != 0) {
		fprintf(stderr, "%s: unknown term export in normalization equality check\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	struct prototype_term_conversion_result conversion;
	if (prototype_term_compare_with_options(
			&storage.program.private->core.terms,
			&definition_env,
			options,
			storage.artifact.interface.term_exports[left_export].local_term,
			storage.artifact.interface.term_exports[right_export].local_term,
			UINT64_MAX,
			&conversion
		) != 0) {
		fprintf(stderr, "%s: failed to check export normalization equality: %s %s\n",
			path,
			left_name,
			right_name);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	printf("exports-normalization-equal %s %s mode=%s %s\n",
		left_name,
		right_name,
		reduction_mode ? reduction_mode : "default",
		conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL ? "yes" : "no");
	printf("conversion-status %s reason=%s steps=%" PRIu64 "\n",
		prototype_term_conversion_status_name(conversion.status),
		prototype_term_conversion_reason_name(conversion.reason),
		conversion.steps_used);
	read_artifact_graph_storage_destroy(&storage);
	return 0;
}

static int check_compiled_exports_normalization_equal(
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	struct prototype_term_db* term_db,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	const char* left_name,
	const char* right_name,
	const char* reduction_mode
) {
	struct prototype_term_reduction_options options;
	struct prototype_term_definition_env definition_env;
	int left_symbol;
	int right_symbol;
	uint32_t left_export;
	uint32_t right_export;
	struct prototype_term_conversion_result conversion;

	if (!symbols || !interface || !term_db || !type_declarations ||
		!definitions || definition_capacity == 0 ||
		!left_name || !right_name ||
		reduction_options_from_mode(reduction_mode, &options) != 0) {
		return -1;
	}
	if (prototype_artifact_interface_build_definition_env(
			interface,
		definitions,
		definition_capacity,
			&definition_env
		) != 0) {
		return -1;
	}
	left_symbol = symbol_intern(symbols, left_name, strlen(left_name));
	right_symbol = symbol_intern(symbols, right_name, strlen(right_name));
	if (left_symbol < 0 || right_symbol < 0 ||
		prototype_artifact_interface_find_term_export(interface, left_symbol, &left_export) != 0 ||
		prototype_artifact_interface_find_term_export(interface, right_symbol, &right_export) != 0 ||
		prototype_term_compare_with_options(
			term_db,
			&definition_env,
			options,
			interface->term_exports[left_export].local_term,
			interface->term_exports[right_export].local_term,
			UINT64_MAX,
			&conversion
		) != 0) {
		return -1;
	}
	printf("source-exports-normalization-equal %s %s mode=%s %s\n",
		left_name,
		right_name,
		reduction_mode ? reduction_mode : "default",
		conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL ? "yes" : "no");
	printf("conversion-status %s reason=%s steps=%" PRIu64 "\n",
		prototype_term_conversion_status_name(conversion.status),
		prototype_term_conversion_reason_name(conversion.reason),
		conversion.steps_used);
	return conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : 1;
}

static int trace_compiled_export_evaluation(
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	struct prototype_term_db* term_db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	const char* name
) {
	struct prototype_term_definition_env definition_env;
	struct prototype_term_reduction_environment reduction_environment;
	uint64_t induction_hypothesis_reductions = 0;
	uint32_t result;
	int name_symbol;
	uint32_t export_id;

	if (!symbols || !interface || !term_db || !type_declarations ||
		!intrinsic_environment || !definitions || definition_capacity == 0 ||
		!name || prototype_artifact_interface_build_definition_env(
			interface,
			definitions,
			definition_capacity,
			&definition_env
		) != 0 || prototype_type_declaration_project_reduction_environment(
			term_db,
			type_declarations,
			symbols,
			&reduction_environment
		) != 0) {
		return -1;
	}
	name_symbol = symbol_intern(symbols, name, strlen(name));
	if (name_symbol < 0 || prototype_artifact_interface_find_term_export(
			interface, name_symbol, &export_id
		) != 0 || prototype_term_perform_with_options(
			term_db,
			&definition_env,
			(struct prototype_term_reduction_options) {
				.flags = PROTOTYPE_TERM_EVALUATE_DEFAULT |
					PROTOTYPE_TERM_REDUCE_DEFINITIONS,
				.symbols = symbols,
				.reduction_environment = &reduction_environment,
				.p_induction_hypothesis_reductions =
					&induction_hypothesis_reductions
			},
			interface->term_exports[export_id].local_term,
			&result
		) != 0) {
		return -1;
	}
	printf(
		"evaluation-trace export=%s induction-hypothesis-reductions=%" PRIu64 "\n",
		name,
		induction_hypothesis_reductions
	);
	printf("evaluation-result ");
	prototype_type_term_print_debug(
		stdout, symbols, intrinsic_environment, type_declarations, term_db, result
	);
	printf("\n");
	return 0;
}

static int check_exports_shape_equal(
	const char* path,
	const char* left_name,
	const char* right_name,
	int core_shape
) {
	if (!path || !left_name || !right_name) {
		return -1;
	}
	struct read_artifact_graph_storage storage;
	if (read_artifact_graph_storage_init(&storage) != 0) {
		return 1;
	}
	if (read_artifact_graph_storage_load(path, &storage) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int left_symbol = symbol_intern(
		&storage.program.private->symbols, left_name, strlen(left_name)
	);
	int right_symbol = symbol_intern(
		&storage.program.private->symbols, right_name, strlen(right_name)
	);
	uint32_t left_export;
	uint32_t right_export;
	if (left_symbol < 0 || right_symbol < 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			left_symbol,
			&left_export
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			right_symbol,
			&right_export
		) != 0) {
		fprintf(stderr, "%s: unknown term export in shape equality check\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int equal = 0;
	int status = core_shape ?
		prototype_term_core_shape_equal(
			&storage.program.private->core.terms,
			storage.artifact.interface.term_exports[left_export].local_term,
			storage.artifact.interface.term_exports[right_export].local_term,
			&equal
		) :
		prototype_term_view_shape_equal(
			&storage.program.private->core.terms,
			storage.artifact.interface.term_exports[left_export].local_term,
			storage.artifact.interface.term_exports[right_export].local_term,
			&equal
		);
	if (status != 0) {
		fprintf(stderr, "%s: failed to check export shape equality: %s %s\n",
			path,
			left_name,
			right_name);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	printf("exports-%s-shape-equal %s %s %s\n",
		core_shape ? "core" : "view",
		left_name,
		right_name,
		equal ? "yes" : "no");
	read_artifact_graph_storage_destroy(&storage);
	return 0;
}

static int check_export_classifier_compatible(
	const char* path,
	const char* expected_name,
	const char* actual_name
) {
	if (!path || !expected_name || !actual_name) {
		return -1;
	}
	struct read_artifact_graph_storage storage;
	struct prototype_term_definition_env definition_env;
	if (read_artifact_graph_storage_init(&storage) != 0) {
		return 1;
	}
	if (read_artifact_graph_storage_load(path, &storage) != 0) {
		fprintf(stderr, "%s: failed to read artifact\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	if (prototype_artifact_interface_build_definition_env(
		&storage.artifact.interface,
		prototype_artifact_interface_storage_definitions(&storage.artifact),
		prototype_artifact_interface_storage_definition_capacity(
			&storage.artifact
		),
		&definition_env
	) != 0) {
		fprintf(stderr, "%s: failed to build definition environment\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int expected_symbol_id = symbol_intern(
		&storage.program.private->symbols, expected_name, strlen(expected_name)
	);
	int actual_symbol_id = symbol_intern(
		&storage.program.private->symbols, actual_name, strlen(actual_name)
	);
	uint32_t expected_export_id;
	uint32_t actual_export_id;
	if (expected_symbol_id < 0 || actual_symbol_id < 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			expected_symbol_id,
			&expected_export_id
		) != 0 ||
		prototype_artifact_interface_find_term_export(
			&storage.artifact.interface,
			actual_symbol_id,
			&actual_export_id
		) != 0) {
		fprintf(stderr, "%s: unknown term export in classifier check\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	uint32_t expected_classifier =
		storage.artifact.interface.term_exports[expected_export_id].classifier;
	uint32_t actual_classifier =
		storage.artifact.interface.term_exports[actual_export_id].classifier;
	if (expected_classifier >= storage.program.private->core.terms.term_count ||
		actual_classifier >= storage.program.private->core.terms.term_count) {
		fprintf(stderr, "%s: missing classifier in classifier check\n", path);
		read_artifact_graph_storage_destroy(&storage);
		return 1;
	}
	int compatible = prototype_judgement_classifier_compatible_with_definitions(
		&storage.program.private->core.terms,
		&storage.program.private->type_declarations,
		&definition_env,
		expected_classifier,
		actual_classifier
	);
	printf("export-classifiers-compatible %s %s %s\n",
		expected_name,
		actual_name,
		compatible ? "yes" : "no");
	read_artifact_graph_storage_destroy(&storage);
	return 0;
}

static int artifact_path_has_supported_suffix(const char* path) {
	size_t length;
	if (!path) {
		return 0;
	}
	length = strlen(path);
	if (length >= 3 && strcmp(path + length - 3, ".ao") == 0) {
		return 1;
	}
	if (length >= 4 && strcmp(path + length - 4, ".apo") == 0) {
		return 1;
	}
	return 0;
}

static int interface_exports_symbol(
	const struct prototype_artifact_interface* artifact_interface,
	int symbol_id
) {
	uint32_t export_id;
	if (!artifact_interface || symbol_id < 0) {
		return 0;
	}
	if (prototype_artifact_interface_find_term_export(
			artifact_interface,
			symbol_id,
			&export_id
		) == 0) {
		return 1;
	}
	return prototype_artifact_interface_find_type_export(
		artifact_interface,
		symbol_id,
		&export_id
	) == 0;
}

static int interface_exports_dependency(
	const struct prototype_artifact_interface* artifact_interface,
	const struct prototype_artifact_dependency* dependency
) {
	uint32_t export_id;
	if (!artifact_interface || !dependency || dependency->name_symbol_id < 0) {
		return 0;
	}
	if (dependency->namespace_symbol_id < 0) {
		return interface_exports_symbol(artifact_interface, dependency->name_symbol_id);
	}
	if (prototype_artifact_interface_find_term_export_in_namespace(
			artifact_interface,
			dependency->namespace_symbol_id,
			dependency->name_symbol_id,
			&export_id
		) == 0) {
		return 1;
	}
	return prototype_artifact_interface_find_type_export_in_namespace(
		artifact_interface,
		dependency->namespace_symbol_id,
		dependency->name_symbol_id,
		&export_id
	) == 0;
}

static int build_search_candidate_path(
	char* buffer,
	size_t buffer_size,
	const char* directory,
	const char* filename
);

static int imported_interfaces_export_symbol(
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	int symbol_id
) {
	if (!imported_interfaces || symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < imported_interface_count; ++i) {
		if (interface_exports_symbol(imported_interfaces[i], symbol_id)) {
			return 1;
		}
	}
	return 0;
}

static int read_import_artifact_into_slot(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_program* program,
	size_t slot,
	struct prototype_universe_db* universe,
	struct read_driver_storage* storage
) {
	if (!path || !symbols || slot >= IMPORT_INTERFACE_CAPACITY ||
		!program || !program->core || !program->type_declarations ||
		!program->judgement || !universe || !storage) {
		return -1;
	}
	struct prototype_term_db* program_terms =
		prototype_core_pipeline_terms(program->core);
	if (!program_terms) {
		return -1;
	}
	if (prototype_program_storage_reset(&storage->provider_program) != 0 ||
		prototype_artifact_interface_storage_reset(
			&storage->provider_artifact
		) != 0 ||
		prototype_artifact_interface_storage_reset(
			&storage->imported_artifacts[slot]
		) != 0) {
		return -1;
	}
	struct prototype_program_storage* provider = &storage->provider_program;
	struct prototype_artifact_interface* provider_interface =
		&storage->provider_artifact.interface;
	struct prototype_artifact_interface* imported_interface =
		&storage->imported_artifacts[slot].interface;
	if (read_artifact_interface_and_graph(
			path,
			symbols,
			prototype_default_intrinsic_typing_environment(),
			&storage->provider_artifact,
			provider,
			universe
		) != 0) {
		return -1;
	}
	size_t provider_term_relocation_count = provider->private->core.terms.term_count == 0 ?
		1 : provider->private->core.terms.term_count;
	size_t provider_context_relocation_count =
		provider->private->metadata.contexts.context_count == 0 ?
			1 : provider->private->metadata.contexts.context_count;
	uint32_t provider_term_relocation[provider_term_relocation_count];
	uint32_t provider_context_relocation[provider_context_relocation_count];
	size_t provider_substitution_relocation_count =
		provider->private->metadata.substitutions.substitution_count == 0 ? 1 :
		provider->private->metadata.substitutions.substitution_count;
	uint32_t provider_substitution_relocation[
		provider_substitution_relocation_count
	];
	size_t provider_claim_relocation_count =
		provider->private->judgement.claim_count == 0 ? 1 :
		provider->private->judgement.claim_count;
	uint32_t provider_claim_relocation[provider_claim_relocation_count];
	size_t provider_binding_relocation_count =
		provider->private->core.terms.next_binding_id == 0 ?
			1 : provider->private->core.terms.next_binding_id;
	uint32_t provider_binding_relocation[provider_binding_relocation_count];
	struct prototype_artifact_graph_relocation provider_additional = {
		.binding_ids = provider_binding_relocation,
		.binding_id_capacity = provider_binding_relocation_count,
		.claim_ids = provider_claim_relocation,
		.claim_id_capacity = provider_claim_relocation_count,
		.substitution_ids = provider_substitution_relocation,
		.substitution_id_capacity = provider_substitution_relocation_count
	};
	uint32_t occurrence_offset =
		(uint32_t)program->metadata->typed_occurrences.occurrence_count;
	int append_status = prototype_artifact_append_graph(
			imported_interface,
			program->symbols,
			program_terms,
			program->type_declarations,
			program->judgement,
			&program->metadata->contexts,
			&program->metadata->substitutions,
			&program->metadata->dimension_operators,
			provider_interface,
			&provider->private->core.terms,
			&provider->private->type_declarations,
			&provider->private->judgement,
			&provider->private->metadata.contexts,
			&provider->private->metadata.substitutions,
			&provider->private->metadata.dimension_operators,
			occurrence_offset,
			provider_term_relocation,
			provider_term_relocation_count,
		provider_context_relocation,
		provider_context_relocation_count,
			&provider_additional,
			1
		);
	if (append_status != 0) {
		fprintf(stderr, "%s: imported graph append failed\n", path);
		return -1;
	}
	if (append_link_typed_occurrence_graph(
			program->metadata,
			&provider->private->metadata,
			program_terms,
			provider_term_relocation,
			provider_term_relocation_count,
			provider_context_relocation,
			provider_context_relocation_count,
			provider_binding_relocation,
			provider_binding_relocation_count,
			provider_substitution_relocation,
			provider_substitution_relocation_count
		) != 0 ||
		prototype_compile_metadata_append_accepted_substitution_claims(
			program->metadata,
			&provider->private->metadata,
			provider_substitution_relocation,
			provider_substitution_relocation_count,
			provider_claim_relocation,
			provider_claim_relocation_count
		) != 0 ||
		prototype_artifact_interface_refresh_term_export_evidence(
			imported_interface,
			program->metadata,
			program->judgement
		) != 0) {
		fprintf(stderr, "%s: imported semantic graph append failed\n", path);
		return -1;
	}
	return 0;
}

static int add_source_import_from_search_dirs(
	const struct prototype_ast_import_def* import,
	const char* const* import_search_dirs,
	size_t import_search_dir_count,
	struct symbol_table* symbols,
	struct prototype_program* program,
	const struct prototype_artifact_interface** imported_interface_refs,
	size_t* p_import_interface_count,
	struct prototype_universe_db* universe,
	struct read_driver_storage* storage
) {
	if (!import || !import_search_dirs || !symbols || !program ||
		!imported_interface_refs || !p_import_interface_count || !universe ||
		!storage) {
		return -1;
	}
	if (imported_interfaces_export_symbol(
			imported_interface_refs,
			*p_import_interface_count,
			import->name_symbol_id
		)) {
		return 0;
	}
	for (size_t dir_index = 0; dir_index < import_search_dir_count; ++dir_index) {
		DIR* directory = opendir(import_search_dirs[dir_index]);
		if (!directory) {
			return -1;
		}
		struct dirent* entry;
		while ((entry = readdir(directory)) != NULL) {
			char candidate_path[LINK_AUTO_PROVIDER_PATH_CAPACITY];
			if (entry->d_name[0] == '.' ||
				!artifact_path_has_supported_suffix(entry->d_name) ||
				build_search_candidate_path(
					candidate_path,
					sizeof(candidate_path),
					import_search_dirs[dir_index],
					entry->d_name
				) != 0) {
				continue;
			}
			if (*p_import_interface_count >= IMPORT_INTERFACE_CAPACITY) {
				closedir(directory);
				return -1;
			}
			if (prototype_artifact_interface_storage_reset(
					&storage->provider_artifact
				) != 0) {
				closedir(directory);
				return -1;
			}
			struct prototype_artifact_interface* probe_interface =
				&storage->provider_artifact.interface;
			if (read_artifact_interface_only(
					candidate_path,
					symbols,
					probe_interface
				) != 0) {
				continue;
			}
			if (!interface_exports_symbol(probe_interface, import->name_symbol_id)) {
				continue;
			}
			size_t slot = *p_import_interface_count;
			if (read_import_artifact_into_slot(
					candidate_path,
					symbols,
					program,
					slot,
					universe,
					storage
				) != 0) {
				closedir(directory);
				return -1;
			}
			imported_interface_refs[slot] =
				&storage->imported_artifacts[slot].interface;
			(*p_import_interface_count)++;
			return 0;
		}
		closedir(directory);
	}
	return 1;
}

static int provider_path_already_added(
	const char* path,
	const char* link_target_path,
	const char* const* link_provider_paths,
	size_t link_provider_count
) {
	if (!path) {
		return 1;
	}
	if (link_target_path && strcmp(path, link_target_path) == 0) {
		return 1;
	}
	for (size_t i = 0; i < link_provider_count; ++i) {
		if (link_provider_paths[i] && strcmp(path, link_provider_paths[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

static int build_search_candidate_path(
	char* buffer,
	size_t buffer_size,
	const char* directory,
	const char* filename
) {
	size_t directory_length;
	if (!buffer || !directory || !filename || buffer_size == 0) {
		return -1;
	}
	directory_length = strlen(directory);
	if (directory_length > 0 && directory[directory_length - 1] == '/') {
		if (snprintf(buffer, buffer_size, "%s%s", directory, filename) >= (int)buffer_size) {
			return -1;
		}
	} else if (snprintf(buffer, buffer_size, "%s/%s", directory, filename) >= (int)buffer_size) {
		return -1;
	}
	return 0;
}

static int count_provider_paths_for_dependency(
	const char* const* link_provider_paths,
	size_t link_provider_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_dependency* dependency,
	struct prototype_artifact_interface_storage* probe_storage
) {
	if (!link_provider_paths || !symbols || !dependency || !probe_storage) {
		return -1;
	}
	int count = 0;
	for (size_t i = 0; i < link_provider_count; ++i) {
		if (!link_provider_paths[i]) {
			continue;
		}
		if (prototype_artifact_interface_storage_reset(probe_storage) != 0) {
			return -1;
		}
		if (read_artifact_interface_only(
				link_provider_paths[i],
				symbols,
				&probe_storage->interface
			) == 0 && interface_exports_dependency(
				&probe_storage->interface, dependency
			)) {
			count++;
		}
	}
	return count;
}

static int add_provider_from_search_dirs(
	const char** link_provider_paths,
	size_t* p_link_provider_count,
	const char* link_target_path,
	const char* const* link_search_dirs,
	size_t link_search_dir_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* target_interface,
	struct prototype_artifact_interface_storage* probe_storage,
	char auto_link_provider_paths[LINK_PROVIDER_CAPACITY][
		LINK_AUTO_PROVIDER_PATH_CAPACITY
	]
) {
	if (!link_provider_paths || !p_link_provider_count || !symbols ||
		!target_interface || !probe_storage || !auto_link_provider_paths) {
		return -1;
	}
	for (size_t dep = 0; dep < target_interface->dependency_count; ++dep) {
		const struct prototype_artifact_dependency* dependency =
			&target_interface->dependencies[dep];
		int found_dependency = interface_exports_dependency(target_interface, dependency);
		if (!found_dependency) {
			int provider_count = count_provider_paths_for_dependency(
				link_provider_paths,
				*p_link_provider_count,
				symbols,
				dependency,
				probe_storage
			);
			if (provider_count < 0 || provider_count > 1) {
				return -1;
			}
			found_dependency = provider_count == 1;
		}
		if (found_dependency) {
			continue;
		}
		char selected_candidate[LINK_AUTO_PROVIDER_PATH_CAPACITY];
		int candidate_count = 0;
		for (size_t dir_index = 0; !found_dependency && dir_index < link_search_dir_count; ++dir_index) {
			DIR* directory = opendir(link_search_dirs[dir_index]);
			if (!directory) {
				return -1;
			}
			struct dirent* entry;
			while ((entry = readdir(directory)) != NULL) {
				char candidate_path[LINK_AUTO_PROVIDER_PATH_CAPACITY];
				if (entry->d_name[0] == '.' ||
					!artifact_path_has_supported_suffix(entry->d_name) ||
					build_search_candidate_path(
						candidate_path,
						sizeof(candidate_path),
						link_search_dirs[dir_index],
						entry->d_name
					) != 0 ||
					provider_path_already_added(
						candidate_path,
						link_target_path,
						link_provider_paths,
						*p_link_provider_count
					)) {
					continue;
				}
				if (prototype_artifact_interface_storage_reset(probe_storage) != 0) {
					closedir(directory);
					return -1;
				}
				if (read_artifact_interface_only(
						candidate_path,
						symbols,
						&probe_storage->interface
					) != 0 ||
					!interface_exports_dependency(
						&probe_storage->interface, dependency
					)) {
					continue;
				}
				candidate_count++;
				if (candidate_count > 1 || snprintf(
						selected_candidate,
						sizeof(selected_candidate),
						"%s",
						candidate_path
					) >= (int)sizeof(selected_candidate)) {
					closedir(directory);
					return -1;
				}
			}
			closedir(directory);
		}
		if (!found_dependency && candidate_count == 1) {
			if (*p_link_provider_count >= LINK_PROVIDER_CAPACITY ||
				snprintf(
					auto_link_provider_paths[*p_link_provider_count],
					LINK_AUTO_PROVIDER_PATH_CAPACITY,
					"%s",
					selected_candidate
				) >= LINK_AUTO_PROVIDER_PATH_CAPACITY) {
				return -1;
			}
			link_provider_paths[*p_link_provider_count] =
				auto_link_provider_paths[*p_link_provider_count];
			(*p_link_provider_count)++;
			printf("found provider for %s: %s\n",
				symbol_to_string(symbols, dependency->name_symbol_id),
				selected_candidate);
		}
	}
	return 0;
}

static int read_provider_interface_for_ordering(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface_storage* storage
);

static int add_provider_closure_from_search_dirs(
	const char** link_provider_paths,
	size_t* p_link_provider_count,
	const char* link_target_path,
	const char* const* link_search_dirs,
	size_t link_search_dir_count,
	struct symbol_table* symbols,
	const struct prototype_artifact_interface* target_interface,
	struct prototype_artifact_interface_storage* provider_storage,
	struct prototype_artifact_interface_storage* probe_storage,
	char auto_link_provider_paths[LINK_PROVIDER_CAPACITY][
		LINK_AUTO_PROVIDER_PATH_CAPACITY
	]
) {
	if (!link_provider_paths || !p_link_provider_count || !symbols ||
		!target_interface || !provider_storage || !probe_storage ||
		!auto_link_provider_paths) {
		return -1;
	}
	if (add_provider_from_search_dirs(
			link_provider_paths,
			p_link_provider_count,
			link_target_path,
			link_search_dirs,
		link_search_dir_count,
		symbols,
		target_interface,
		probe_storage,
		auto_link_provider_paths
		) != 0) {
		return -1;
	}

	size_t scanned_provider_count = 0;
	while (scanned_provider_count < *p_link_provider_count) {
		if (read_provider_interface_for_ordering(
			link_provider_paths[scanned_provider_count],
			symbols,
			provider_storage
		) != 0) {
			return -1;
		}
		if (add_provider_from_search_dirs(
				link_provider_paths,
				p_link_provider_count,
				link_target_path,
				link_search_dirs,
				link_search_dir_count,
				symbols,
			&provider_storage->interface,
			probe_storage,
			auto_link_provider_paths
			) != 0) {
			return -1;
		}
		scanned_provider_count++;
	}
	return 0;
}

static int read_provider_interface_for_ordering(
	const char* path,
	struct symbol_table* symbols,
	struct prototype_artifact_interface_storage* storage
) {
	if (!storage || prototype_artifact_interface_storage_reset(storage) != 0) {
		return -1;
	}
	return read_artifact_interface_only(path, symbols, &storage->interface);
}

static int provider_depends_on_provider(
	const struct prototype_artifact_interface* dependent,
	const struct prototype_artifact_interface* provider
) {
	if (!dependent || !provider) {
		return 0;
	}
	for (size_t i = 0; i < dependent->dependency_count; ++i) {
		if (interface_exports_dependency(provider, &dependent->dependencies[i])) {
			return 1;
		}
	}
	return 0;
}

static int order_link_providers_by_interface_dependencies(
	const char** link_provider_paths,
	size_t link_provider_count,
	struct symbol_table* symbols,
	struct prototype_artifact_interface_storage* left_storage,
	struct prototype_artifact_interface_storage* right_storage
) {
	if (!link_provider_paths || !symbols || !left_storage || !right_storage) {
		return -1;
	}
	for (size_t pass = 0; pass < link_provider_count; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < link_provider_count; ++i) {
			for (size_t j = i + 1; j < link_provider_count; ++j) {
				if (read_provider_interface_for_ordering(
						link_provider_paths[i],
						symbols,
						left_storage
					) != 0 ||
					read_provider_interface_for_ordering(
						link_provider_paths[j],
						symbols,
						right_storage
					) != 0) {
					return -1;
				}
				if (provider_depends_on_provider(
						&left_storage->interface,
						&right_storage->interface
					)) {
					const char* tmp = link_provider_paths[i];
					link_provider_paths[i] = link_provider_paths[j];
					link_provider_paths[j] = tmp;
					changed = 1;
				}
			}
		}
		if (!changed) {
			return 0;
		}
	}
	return 0;
}

static uint32_t offset_optional_id(uint32_t id, uint32_t offset) {
	return id == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : id + offset;
}

static int interface_type_expr_present(const struct prototype_type_expr* expr) {
	return expr && expr->tag != 0;
}

static int interface_type_parameter_present(
	const struct prototype_artifact_type_parameter_export* parameter
) {
	return parameter && parameter->binding_id != PROTOTYPE_INVALID_ID;
}

static int interface_field_ref_present(const uint32_t* field_ref) {
	return field_ref && *field_ref != PROTOTYPE_INVALID_ID;
}

static int reexport_appended_interface(
	struct prototype_artifact_interface* target,
	const struct prototype_artifact_interface* appended
) {
	if (!target || !appended ||
		appended->type_export_count > ARTIFACT_TYPE_EXPORT_CAPACITY ||
		target->type_expr_count + appended->type_expr_count > target->type_expr_capacity ||
		target->type_parameter_count + appended->type_parameter_count >
			target->type_parameter_capacity ||
		target->constructor_field_type_expr_count +
			appended->constructor_field_type_expr_count >
			target->constructor_field_type_expr_capacity) {
		return -1;
	}

	uint32_t type_expr_offset = (uint32_t)target->type_expr_count;
	uint32_t type_parameter_offset = (uint32_t)target->type_parameter_count;
	uint32_t constructor_field_offset =
		(uint32_t)target->constructor_field_type_expr_count;
	uint32_t universe_offset = prototype_artifact_interface_next_universe_var(target);
	for (size_t i = 0; i < appended->type_expr_count; ++i) {
		struct prototype_type_expr expr = appended->type_exprs[i];
		if (interface_type_expr_present(&expr)) {
			switch (expr.tag) {
				case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
					expr.as.universe_var.level_var += universe_offset;
					break;
				case PROTOTYPE_TYPE_EXPR_APP:
					expr.as.app.function =
						offset_optional_id(expr.as.app.function, type_expr_offset);
					expr.as.app.argument =
						offset_optional_id(expr.as.app.argument, type_expr_offset);
					break;
				case PROTOTYPE_TYPE_EXPR_ARROW:
					expr.as.arrow.domain =
						offset_optional_id(expr.as.arrow.domain, type_expr_offset);
					expr.as.arrow.codomain =
						offset_optional_id(expr.as.arrow.codomain, type_expr_offset);
					break;
				case PROTOTYPE_TYPE_EXPR_PI:
					expr.as.pi.domain =
						offset_optional_id(expr.as.pi.domain, type_expr_offset);
					expr.as.pi.codomain =
						offset_optional_id(expr.as.pi.codomain, type_expr_offset);
					break;
				default:
					break;
			}
		}
		target->type_exprs[target->type_expr_count++] = expr;
	}
	for (size_t i = 0; i < appended->type_parameter_count; ++i) {
		target->type_parameters[target->type_parameter_count] =
			appended->type_parameters[i];
		if (interface_type_parameter_present(
				&target->type_parameters[target->type_parameter_count]
			)) {
			target->type_parameters[target->type_parameter_count].type_expr =
				offset_optional_id(
					target->type_parameters[target->type_parameter_count].type_expr,
					type_expr_offset
				);
		}
		target->type_parameter_count++;
	}
	for (size_t i = 0; i < appended->constructor_field_type_expr_count; ++i) {
		uint32_t field_ref = appended->constructor_field_type_exprs[i];
		if (interface_field_ref_present(&field_ref)) {
			field_ref = offset_optional_id(field_ref, type_expr_offset);
		}
		target->constructor_field_type_exprs[
			target->constructor_field_type_expr_count++
		] = field_ref;
	}

	uint32_t type_index_map[ARTIFACT_TYPE_EXPORT_CAPACITY];
	int type_added[ARTIFACT_TYPE_EXPORT_CAPACITY];
	for (size_t i = 0; i < ARTIFACT_TYPE_EXPORT_CAPACITY; ++i) {
		type_index_map[i] = PROTOTYPE_INVALID_ID;
		type_added[i] = 0;
	}

	for (size_t i = 0; i < appended->term_export_count; ++i) {
		uint32_t existing;
		int found = prototype_artifact_interface_find_term_export_in_namespace(
			target,
			appended->term_exports[i].namespace_symbol_id,
			appended->term_exports[i].name_symbol_id,
			&existing
		);
		if (found < 0) {
			return -1;
		}
		if (found == 0) {
			continue;
		}
		if (target->term_export_count >= target->term_export_capacity) {
			return -1;
		}
		target->term_exports[target->term_export_count++] = appended->term_exports[i];
	}

	for (size_t i = 0; i < appended->type_export_count; ++i) {
		uint32_t existing;
		int found = prototype_artifact_interface_find_type_export_in_namespace(
			target,
			appended->type_exports[i].namespace_symbol_id,
			appended->type_exports[i].name_symbol_id,
			&existing
		);
		if (found < 0) {
			return -1;
		}
		if (found == 0) {
			type_index_map[i] = existing;
			continue;
		}
		if (target->type_export_count >= target->type_export_capacity) {
			return -1;
		}
		uint32_t target_type_index = (uint32_t)target->type_export_count;
		target->type_exports[target->type_export_count] = appended->type_exports[i];
		target->type_exports[target->type_export_count].first_parameter +=
			type_parameter_offset;
		target->type_exports[target->type_export_count].first_constructor_export =
			PROTOTYPE_INVALID_ID;
		target->type_exports[target->type_export_count].constructor_count = 0;
		target->type_export_count++;
		type_index_map[i] = target_type_index;
		type_added[i] = 1;
	}

	for (size_t i = 0; i < appended->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* constructor =
			&appended->constructor_exports[i];
		if (constructor->type_export_index >= ARTIFACT_TYPE_EXPORT_CAPACITY ||
			constructor->type_export_index >= appended->type_export_count ||
			!type_added[constructor->type_export_index]) {
			continue;
		}
		uint32_t target_type_index = type_index_map[constructor->type_export_index];
		if (target_type_index == PROTOTYPE_INVALID_ID ||
			target_type_index >= target->type_export_count ||
			target->constructor_export_count >= target->constructor_export_capacity) {
			return -1;
		}
		struct prototype_artifact_type_export* type_export =
			&target->type_exports[target_type_index];
		if (type_export->constructor_count == 0) {
			type_export->first_constructor_export =
				(uint32_t)target->constructor_export_count;
		}
		target->constructor_exports[target->constructor_export_count] = *constructor;
		target->constructor_exports[target->constructor_export_count].type_export_index =
			target_type_index;
		target->constructor_exports[target->constructor_export_count].readback_first_field_type +=
			constructor_field_offset;
		target->constructor_export_count++;
		type_export->constructor_count++;
	}

	for (size_t i = 0; i < target->type_export_count; ++i) {
		if (target->type_exports[i].constructor_count == 0 &&
			target->type_exports[i].first_constructor_export == PROTOTYPE_INVALID_ID) {
			target->type_exports[i].first_constructor_export =
				(uint32_t)target->constructor_export_count;
		}
	}
	return 0;
}

static int parse_step_limit(const char* text, uint64_t* p_value) {
	char trailing;
	return text && p_value && text[0] != '-' &&
		sscanf(text, "%" SCNu64 "%c", p_value, &trailing) == 1 ? 0 : -1;
}

static int read_file_command(
	int argc,
	char** argv,
	struct read_driver_storage* storage
) {
	int file_arg = 1;
	const char* artifact_output_path = NULL;
	const char* interface_input_path = NULL;
	const char* check_export_normalization_equal_path = NULL;
	const char* check_export_normalization_equal_name = NULL;
	const char* check_exports_normalization_equal_path = NULL;
	const char* check_exports_normalization_equal_left_name = NULL;
	const char* check_exports_normalization_equal_right_name = NULL;
	const char* check_source_exports_normalization_equal_left_name = NULL;
	const char* check_source_exports_normalization_equal_right_name = NULL;
	const char* trace_source_export_evaluation_name = NULL;
	const char* show_function_graph_name = NULL;
	const char* reduction_mode = "default";
	const char* check_exports_shape_equal_path = NULL;
	const char* check_exports_shape_equal_left_name = NULL;
	const char* check_exports_shape_equal_right_name = NULL;
	int check_exports_shape_equal_core = 0;
	const char* check_classifier_path = NULL;
	const char* check_classifier_expected_name = NULL;
	const char* check_classifier_actual_name = NULL;
	const char* link_target_path = NULL;
	const char* link_provider_paths[LINK_PROVIDER_CAPACITY];
	size_t link_provider_count = 0;
	const char* link_search_dirs[LINK_SEARCH_DIR_CAPACITY];
	size_t link_search_dir_count = 0;
	const char* link_output_path = NULL;
	const char* aggregate_output_path = NULL;
	const char* check_backend_name = NULL;
	const char* namespace_name = NULL;
	const char* import_interface_paths[IMPORT_INTERFACE_CAPACITY];
	size_t import_interface_count = 0;
	const char* import_search_dirs[LINK_SEARCH_DIR_CAPACITY];
	size_t import_search_dir_count = 0;
	const char* opaque_export_names[OPAQUE_EXPORT_CAPACITY];
	size_t opaque_export_count = 0;
	int read_graph = 0;
	int link_reexport_providers = 0;
	int solve_effort_is_set = 0;
	uint64_t solve_effort_steps = 0;
	int compile_policy = PROTOTYPE_COMPILE_POLICY_HYBRID;
	int definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;
	int quiet = 0;

	for (; file_arg < argc && argv[file_arg][0] == '-'; ++file_arg) {
		if (strcmp(argv[file_arg], "--quiet") == 0 ||
			strcmp(argv[file_arg], "--check-only") == 0) {
			quiet = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--implicit-definition-thunks") == 0) {
			definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;
			continue;
		}
		if (strcmp(argv[file_arg], "--no-implicit-definition-thunks") == 0) {
			definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_EXPLICIT;
			continue;
		}
		if (strcmp(argv[file_arg], "--solve-effort") == 0) {
			if (file_arg + 1 >= argc || parse_step_limit(
					argv[file_arg + 1], &solve_effort_steps
				) != 0) {
				fprintf(stderr, "--solve-effort requires an unsigned integer\n");
				return 1;
			}
			solve_effort_is_set = 1;
			file_arg++;
			continue;
		}
		if (strcmp(argv[file_arg], "--policy") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--policy requires strict, hybrid, or exploratory\n");
				return 1;
			}
			const char* policy_name = argv[++file_arg];
			if (strcmp(policy_name, "strict") == 0) {
				compile_policy = PROTOTYPE_COMPILE_POLICY_STRICT;
			} else if (strcmp(policy_name, "hybrid") == 0) {
				compile_policy = PROTOTYPE_COMPILE_POLICY_HYBRID;
			} else if (strcmp(policy_name, "exploratory") == 0) {
				compile_policy = PROTOTYPE_COMPILE_POLICY_EXPLORATORY;
			} else {
				fprintf(stderr, "unknown compile policy: %s\n", policy_name);
				return 1;
			}
			continue;
		}
		if (strcmp(argv[file_arg], "--write-artifact") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--write-artifact requires a path\n");
				return 1;
			}
			artifact_output_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--namespace") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--namespace requires a name\n");
				return 1;
			}
			namespace_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--read-interface") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--read-interface requires a path\n");
				return 1;
			}
			interface_input_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--read-graph") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--read-graph requires a path\n");
				return 1;
			}
			interface_input_path = argv[++file_arg];
			read_graph = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--check-backend") == 0) {
			if (file_arg + 2 >= argc) {
				fprintf(stderr, "--check-backend requires interpreter, c, or verilog and an artifact path\n");
				return 1;
			}
			check_backend_name = argv[++file_arg];
			interface_input_path = argv[++file_arg];
			read_graph = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--check-export-normalization-equal") == 0) {
			if (file_arg + 2 >= argc) {
				fprintf(stderr, "--check-export-normalization-equal requires artifact path and export name\n");
				return 1;
			}
			check_export_normalization_equal_path = argv[++file_arg];
			check_export_normalization_equal_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--check-exports-normalization-equal") == 0) {
			if (file_arg + 3 >= argc) {
				fprintf(stderr, "--check-exports-normalization-equal requires artifact path and two export names\n");
				return 1;
			}
			check_exports_normalization_equal_path = argv[++file_arg];
			check_exports_normalization_equal_left_name = argv[++file_arg];
			check_exports_normalization_equal_right_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--check-source-exports-normalization-equal") == 0) {
			if (file_arg + 2 >= argc) {
				fprintf(stderr, "--check-source-exports-normalization-equal requires two term names\n");
				return 1;
			}
			check_source_exports_normalization_equal_left_name = argv[++file_arg];
			check_source_exports_normalization_equal_right_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--trace-source-export-evaluation") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--trace-source-export-evaluation requires a term name\n");
				return 1;
			}
			trace_source_export_evaluation_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--show-function-graph") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--show-function-graph requires a definition name\n");
				return 1;
			}
			show_function_graph_name = argv[++file_arg];
			continue;
		}
			if (strcmp(argv[file_arg], "--reduction-mode") == 0) {
				if (file_arg + 1 >= argc) {
					fprintf(stderr, "--reduction-mode requires default, beta, match, or none\n");
					return 1;
				}
				reduction_mode = argv[++file_arg];
				continue;
			}
			if (strcmp(argv[file_arg], "--check-exports-view-shape-equal") == 0 ||
				strcmp(argv[file_arg], "--check-exports-core-shape-equal") == 0) {
				if (file_arg + 3 >= argc) {
					fprintf(stderr, "%s requires artifact path and two export names\n", argv[file_arg]);
					return 1;
				}
				check_exports_shape_equal_core =
					strcmp(argv[file_arg], "--check-exports-core-shape-equal") == 0;
				check_exports_shape_equal_path = argv[++file_arg];
				check_exports_shape_equal_left_name = argv[++file_arg];
				check_exports_shape_equal_right_name = argv[++file_arg];
				continue;
			}
			if (strcmp(argv[file_arg], "--check-export-classifiers-compatible") == 0) {
				if (file_arg + 3 >= argc) {
					fprintf(stderr, "--check-export-classifiers-compatible requires artifact path and two export names\n");
					return 1;
				}
				check_classifier_path = argv[++file_arg];
				check_classifier_expected_name = argv[++file_arg];
				check_classifier_actual_name = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-artifacts") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-artifacts requires target.ao\n");
				return 1;
			}
			link_target_path = argv[++file_arg];
			if (file_arg + 1 < argc && argv[file_arg + 1][0] != '-') {
				if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
					fprintf(stderr, "too many link providers\n");
					return 1;
				}
				link_provider_paths[link_provider_count++] = argv[++file_arg];
			}
			continue;
		}
		if (strcmp(argv[file_arg], "--link-provider") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-provider requires provider.ao\n");
				return 1;
			}
			if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
				fprintf(stderr, "too many link providers\n");
				return 1;
			}
			link_provider_paths[link_provider_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-search-dir") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-search-dir requires a directory\n");
				return 1;
			}
			if (link_search_dir_count >= LINK_SEARCH_DIR_CAPACITY) {
				fprintf(stderr, "too many link search dirs\n");
				return 1;
			}
			link_search_dirs[link_search_dir_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--import-interface") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--import-interface requires an artifact path\n");
				return 1;
			}
			if (import_interface_count >= IMPORT_INTERFACE_CAPACITY) {
				fprintf(stderr, "too many import interfaces\n");
				return 1;
			}
			import_interface_paths[import_interface_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--import-search-dir") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--import-search-dir requires a directory\n");
				return 1;
			}
			if (import_search_dir_count >= LINK_SEARCH_DIR_CAPACITY) {
				fprintf(stderr, "too many import search dirs\n");
				return 1;
			}
			import_search_dirs[import_search_dir_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--opaque-export") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--opaque-export requires a term name\n");
				return 1;
			}
			if (opaque_export_count >= OPAQUE_EXPORT_CAPACITY) {
				fprintf(stderr, "too many opaque exports\n");
				return 1;
			}
			opaque_export_names[opaque_export_count++] = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-output") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--link-output requires a path\n");
				return 1;
			}
			link_output_path = argv[++file_arg];
			continue;
		}
		if (strcmp(argv[file_arg], "--link-reexport-providers") == 0) {
			link_reexport_providers = 1;
			continue;
		}
		if (strcmp(argv[file_arg], "--aggregate-artifact") == 0) {
			if (file_arg + 1 >= argc) {
				fprintf(stderr, "--aggregate-artifact requires an output path\n");
				return 1;
			}
			aggregate_output_path = argv[++file_arg];
			continue;
		}
		fprintf(stderr, "unknown option: %s\n", argv[file_arg]);
		fprintf(stderr, "Usage: %s [--policy strict|hybrid|exploratory] [--implicit-definition-thunks|--no-implicit-definition-thunks] [--normalization-steps N] [--solver-steps N] [--show-function-graph name] [--write-artifact out.ao] [--namespace name] [--opaque-export name ...] [--import-interface import.ao ...] [--import-search-dir dir ...] <file.p>...\n", argv[0]);
		fprintf(stderr, "       %s --read-interface file.ao\n", argv[0]);
			fprintf(stderr, "       %s --read-graph file.ao\n", argv[0]);
			fprintf(stderr, "       %s --check-backend interpreter|c|verilog file.ao\n", argv[0]);
			fprintf(stderr, "       %s --check-export-normalization-equal file.ao name\n", argv[0]);
		fprintf(stderr, "       %s --check-exports-normalization-equal file.ao left right [--reduction-mode mode]\n", argv[0]);
		fprintf(stderr, "       %s --check-source-exports-normalization-equal left right [--reduction-mode mode] file.p\n", argv[0]);
		fprintf(stderr, "       %s --trace-source-export-evaluation name file.p\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-view-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-core-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-export-classifiers-compatible file.ao expected actual\n", argv[0]);
			fprintf(stderr, "       %s --link-artifacts target.ao provider.ao [--link-provider provider2.ao ...] [--link-search-dir dir] [--link-reexport-providers] [--link-output linked.ao]\n", argv[0]);
			fprintf(stderr, "       %s --aggregate-artifact out.ao base.ao [provider.ao ...]\n", argv[0]);
		return 1;
	}

	if (check_export_normalization_equal_path) {
		return check_export_normalization_equal(check_export_normalization_equal_path, check_export_normalization_equal_name);
	}
	if (check_exports_normalization_equal_path) {
		return check_exports_normalization_equal(
			check_exports_normalization_equal_path,
			check_exports_normalization_equal_left_name,
			check_exports_normalization_equal_right_name,
			reduction_mode
		);
	}
	if (check_exports_shape_equal_path) {
		return check_exports_shape_equal(
			check_exports_shape_equal_path,
			check_exports_shape_equal_left_name,
			check_exports_shape_equal_right_name,
			check_exports_shape_equal_core
		);
	}
	if (check_classifier_path) {
		return check_export_classifier_compatible(
			check_classifier_path,
			check_classifier_expected_name,
			check_classifier_actual_name
		);
	}

	if (aggregate_output_path) {
		if (link_target_path || link_output_path || link_provider_count != 0) {
			fprintf(stderr, "--aggregate-artifact cannot be combined with explicit link options\n");
			return 1;
		}
		if (file_arg >= argc) {
			fprintf(stderr, "--aggregate-artifact requires at least one input artifact\n");
			return 1;
		}
		link_target_path = argv[file_arg++];
		link_output_path = aggregate_output_path;
		link_reexport_providers = 1;
		for (; file_arg < argc; ++file_arg) {
			if (link_provider_count >= LINK_PROVIDER_CAPACITY) {
				fprintf(stderr, "too many aggregate providers\n");
				return 1;
			}
			link_provider_paths[link_provider_count++] = argv[file_arg];
		}
	}

	if (!interface_input_path && !link_target_path && argc - file_arg < 1) {
		fprintf(stderr, "Usage: %s [--implicit-definition-thunks|--no-implicit-definition-thunks] [--normalization-steps N] [--solver-steps N] [--write-artifact out.ao] [--namespace name] [--opaque-export name ...] [--import-interface import.ao ...] [--import-search-dir dir ...] <file.p>...\n", argv[0]);
		fprintf(stderr, "       %s --read-interface file.ao\n", argv[0]);
			fprintf(stderr, "       %s --read-graph file.ao\n", argv[0]);
			fprintf(stderr, "       %s --check-export-normalization-equal file.ao name\n", argv[0]);
		fprintf(stderr, "       %s --check-exports-normalization-equal file.ao left right [--reduction-mode mode]\n", argv[0]);
		fprintf(stderr, "       %s --check-source-exports-normalization-equal left right [--reduction-mode mode] file.p\n", argv[0]);
		fprintf(stderr, "       %s --trace-source-export-evaluation name file.p\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-view-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --check-exports-core-shape-equal file.ao left right\n", argv[0]);
			fprintf(stderr, "       %s --link-artifacts target.ao provider.ao [--link-provider provider2.ao ...] [--link-search-dir dir] [--link-reexport-providers] [--link-output linked.ao]\n", argv[0]);
			fprintf(stderr, "       %s --aggregate-artifact out.ao base.ao [provider.ao ...]\n", argv[0]);
			return 1;
	}

	if (!storage) {
		return 1;
	}
	struct prototype_program_storage* local = &storage->local_program;
	struct symbol_table* symbols = &local->private->symbols;
	struct prototype_ast_db* ast_db = &local->private->asts;
	struct prototype_type_declaration_db* type_declarations =
		&local->private->type_declarations;
	struct prototype_term_db* term_db = &local->private->core.terms;
	struct prototype_universe_db* universe_db = &local->private->universe;
	struct prototype_judgement_db* judgement_db = &local->private->judgement;
	struct prototype_compile_metadata* metadata = &local->private->metadata;
	struct prototype_program* program = &local->private->program;
	struct prototype_read_error error;
	if (link_target_path) {
		struct prototype_program_storage* provider = &storage->provider_program;
		struct prototype_artifact_interface* artifact_interface =
			&storage->artifact.interface;
		struct prototype_artifact_interface* provider_interface =
			&storage->provider_artifact.interface;
		struct prototype_artifact_interface* appended_interface =
			&storage->appended_artifact.interface;

		if (read_artifact_interface_and_graph(
				link_target_path,
				symbols,
				prototype_default_intrinsic_typing_environment(),
				&storage->artifact,
				local,
				universe_db
			) != 0) {
			fprintf(stderr, "%s: failed to read target artifact\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (link_search_dir_count > 0 &&
			add_provider_closure_from_search_dirs(
				link_provider_paths,
				&link_provider_count,
				link_target_path,
				link_search_dirs,
				link_search_dir_count,
				symbols,
				artifact_interface,
				&storage->provider_artifact,
				&storage->appended_artifact,
				storage->auto_link_provider_paths
			) != 0) {
			fprintf(stderr, "%s: failed to search provider artifacts\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (order_link_providers_by_interface_dependencies(
				link_provider_paths,
				link_provider_count,
				symbols,
				&storage->provider_artifact,
				&storage->appended_artifact
			) != 0) {
			fprintf(stderr, "%s: failed to order provider artifacts\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		struct prototype_typed_occurrence_graph* linked_occurrences =
			prototype_compile_metadata_typed_occurrences(metadata);
		size_t discharged_link_effect_equations = 0;
		if (prototype_verification_db_try_discharge_phase(
				&metadata->verification,
				term_db,
				PROTOTYPE_VERIFICATION_PHASE_LINK,
				&discharged_link_effect_equations
			) != 0) {
			fprintf(stderr, "%s: linked effect-row discharge failed\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (link_provider_count != 0 &&
			prototype_typed_occurrence_graph_begin_transaction(
				linked_occurrences, term_db, &metadata->contexts
			) != 0) {
			fprintf(stderr, "%s: failed to begin linked occurrence transaction\n",
				link_target_path);
			symbol_table_free(symbols);
			return 1;
		}

		size_t before_terms = term_db->term_count;
		size_t before_types = type_declarations->semantic_schema.type_count;
		size_t total_provider_exports = 0;
		for (size_t provider_index = 0; provider_index < link_provider_count; ++provider_index) {
			const char* provider_path = link_provider_paths[provider_index];
			if (prototype_program_storage_reset(provider) != 0 ||
				prototype_artifact_interface_storage_reset(
					&storage->provider_artifact
				) != 0 ||
				prototype_artifact_interface_storage_reset(
					&storage->appended_artifact
				) != 0) {
				fprintf(stderr, "%s: failed to reset provider storage\n", provider_path);
				return 1;
			}
			provider_interface = &storage->provider_artifact.interface;
			appended_interface = &storage->appended_artifact.interface;
			if (read_artifact_interface_and_graph(
					provider_path,
					symbols,
					prototype_default_intrinsic_typing_environment(),
					&storage->provider_artifact,
					provider,
					universe_db
				) != 0) {
				fprintf(stderr, "%s: failed to read provider artifact\n", provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (prototype_artifact_apply_type_expr_relocations(
					artifact_interface,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					&metadata->contexts,
					&metadata->dimension_operators,
					provider_interface
				) != 0) {
				fprintf(stderr, "%s + %s: failed to link artifacts\n", link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			uint32_t provider_occurrence_offset =
				(uint32_t)metadata->typed_occurrences.occurrence_count;
			size_t provider_term_relocation_count =
				provider->private->core.terms.term_count == 0 ?
					1 : provider->private->core.terms.term_count;
			size_t provider_context_relocation_count =
				provider->private->metadata.contexts.context_count == 0 ?
					1 : provider->private->metadata.contexts.context_count;
			uint32_t provider_term_relocation[provider_term_relocation_count];
			uint32_t provider_context_relocation[provider_context_relocation_count];
				size_t provider_binding_relocation_count =
					provider->private->core.terms.next_binding_id == 0 ?
						1 : provider->private->core.terms.next_binding_id;
				uint32_t provider_binding_relocation[
					provider_binding_relocation_count
				];
				size_t provider_substitution_relocation_count =
					provider->private->metadata.substitutions.substitution_count == 0 ? 1 :
					provider->private->metadata.substitutions.substitution_count;
				uint32_t provider_substitution_relocation[
					provider_substitution_relocation_count
				];
				size_t provider_claim_relocation_count =
					provider->private->judgement.claim_count == 0 ? 1 :
					provider->private->judgement.claim_count;
				uint32_t provider_claim_relocation[
					provider_claim_relocation_count
				];
				struct prototype_artifact_graph_relocation provider_additional = {
					.binding_ids = provider_binding_relocation,
					.binding_id_capacity = provider_binding_relocation_count,
					.claim_ids = provider_claim_relocation,
					.claim_id_capacity = provider_claim_relocation_count,
					.substitution_ids = provider_substitution_relocation,
					.substitution_id_capacity =
						provider_substitution_relocation_count
				};
			if (prototype_artifact_append_graph(
					appended_interface,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					&metadata->contexts,
					&metadata->substitutions,
					&metadata->dimension_operators,
					provider_interface,
					&provider->private->core.terms,
					&provider->private->type_declarations,
					&provider->private->judgement,
					&provider->private->metadata.contexts,
					&provider->private->metadata.substitutions,
					&provider->private->metadata.dimension_operators,
					provider_occurrence_offset,
					provider_term_relocation,
					provider_term_relocation_count,
					provider_context_relocation,
					provider_context_relocation_count,
					&provider_additional,
					1
					) != 0 ||
					prototype_compile_metadata_append_accepted_substitution_claims(
						metadata,
						&provider->private->metadata,
						provider_substitution_relocation,
						provider_substitution_relocation_count,
						provider_claim_relocation,
						provider_claim_relocation_count
					) != 0 || append_link_typed_occurrence_graph(
					metadata,
					&provider->private->metadata,
					term_db,
					provider_term_relocation,
					provider_term_relocation_count,
					provider_context_relocation,
					provider_context_relocation_count,
					provider_binding_relocation,
					provider_binding_relocation_count,
					provider_substitution_relocation,
					provider_substitution_relocation_count
				) != 0) {
				fprintf(stderr, "%s + %s: failed to link artifacts\n", link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (prototype_artifact_align_export_occurrences(
					appended_interface,
					term_db,
					judgement_db,
					metadata
				) != 0) {
				fprintf(stderr, "%s + %s: failed to align appended export Operations\n",
					link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (prototype_artifact_apply_type_expr_relocations(
					appended_interface,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					&metadata->contexts,
					&metadata->dimension_operators,
					artifact_interface
				) != 0) {
				fprintf(stderr, "%s + %s: failed provider type relocation\n",
					link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (prototype_artifact_apply_term_relocations(
					appended_interface,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					&metadata->contexts,
					metadata,
					artifact_interface
				) != 0) {
				fprintf(stderr, "%s + %s: failed provider term relocation\n",
					link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (prototype_artifact_apply_term_relocations(
					artifact_interface,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					&metadata->contexts,
					metadata,
					appended_interface
				) != 0) {
				fprintf(stderr, "%s + %s: failed target term relocation\n",
					link_target_path, provider_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (link_reexport_providers) {
				if (reexport_appended_interface(
						artifact_interface,
						appended_interface
					) != 0 ||
					prototype_artifact_interface_collect_dependencies(
						artifact_interface,
						term_db,
						type_declarations,
						judgement_db
					) != 0) {
					fprintf(stderr, "%s: failed to re-export provider interface\n", provider_path);
					symbol_table_free(symbols);
					return 1;
				}
			}
			total_provider_exports += provider_interface->term_export_count;
			printf("linked provider artifact %s exports=%zu appended_exports=%zu dependencies=%zu\n",
				provider_path,
				provider_interface->term_export_count,
				appended_interface->term_export_count,
				artifact_interface->dependency_count);
			for (size_t i = 0; i < appended_interface->term_export_count; ++i) {
				const struct prototype_artifact_term_export* export =
					&appended_interface->term_exports[i];
				printf("linked provider term %s -> term#%u\n",
					symbol_to_string(symbols, export->name_symbol_id),
					export->local_term);
			}
		}
		if (link_provider_count != 0 &&
			prototype_typed_occurrence_graph_freeze(
				linked_occurrences, term_db, &metadata->contexts
			) != 0) {
			fprintf(stderr, "%s: failed to freeze linked occurrence snapshot\n",
				link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (prototype_artifact_interface_recompute_keys(
				artifact_interface,
				symbols,
				term_db,
				type_declarations,
				&metadata->contexts,
				&metadata->dimension_operators
			) != 0 ||
			prototype_artifact_interface_collect_dependencies(
				artifact_interface,
				term_db,
				type_declarations,
				judgement_db
			) != 0) {
			fprintf(stderr, "%s: failed to finalize linked artifact interface\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (prototype_judgement_validate_accepted_graph(
				term_db,
				type_declarations,
				prototype_default_intrinsic_typing_environment(),
				&metadata->contexts,
				&(const struct prototype_context_classifier_view) {
					.contexts = &metadata->contexts
				},
				&metadata->substitutions,
				&metadata->dimension_operators,
				linked_occurrences,
				&(const struct prototype_judgement_occurrence_context_view) {
					.publication = &metadata->typed_publication
				},
				judgement_db
			) != 0) {
			fprintf(stderr, "%s: linked artifact proof validation failed\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (artifact_exports_have_accepted_claims(
				artifact_interface,
				term_db,
				judgement_db,
				metadata,
				1
			) != 0) {
			fprintf(stderr, "%s: linked artifact export validation failed\n", link_target_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (link_output_path) {
			if (prototype_universe_build_closed(
					universe_db,
					type_declarations,
					term_db,
					linked_occurrences,
					judgement_db
				) != 0) {
				fprintf(stderr, "%s: failed to collect linked universe graph\n", link_output_path);
				symbol_table_free(symbols);
				return 1;
			}
			char temporary_output_path[4096];
			int temporary_path_length = snprintf(
				temporary_output_path,
				sizeof(temporary_output_path),
				"%s.tmp.%ld",
				link_output_path,
				(long)getpid()
			);
			if (temporary_path_length < 0 ||
				(size_t)temporary_path_length >= sizeof(temporary_output_path)) {
				fprintf(stderr, "%s: linked artifact output path is too long\n", link_output_path);
				symbol_table_free(symbols);
				return 1;
			}
			FILE* output = fopen(temporary_output_path, "w");
			if (!output) {
				fprintf(stderr, "%s: failed to open linked artifact output\n", link_output_path);
				symbol_table_free(symbols);
				return 1;
			}
			int write_status = prototype_artifact_write_text(
				output,
				symbols,
				prototype_default_intrinsic_typing_environment(),
				artifact_interface,
				term_db,
				type_declarations,
				judgement_db,
				NULL,
				universe_db,
				NULL,
				metadata,
				&(const struct prototype_context_classifier_view) {
					.contexts = &metadata->contexts
				}
			);
			if (fclose(output) != 0) {
				write_status = -1;
			}
			if (write_status != 0) {
				remove(temporary_output_path);
				fprintf(stderr, "%s: failed to write linked artifact\n", link_output_path);
				symbol_table_free(symbols);
				return 1;
			}
			if (rename(temporary_output_path, link_output_path) != 0) {
				remove(temporary_output_path);
				fprintf(stderr, "%s: failed to publish linked artifact\n", link_output_path);
				symbol_table_free(symbols);
				return 1;
			}
		}

		printf(
			"#### Artifact Link ####\n"
			"target=%s providers=%zu\n"
			"output=%s\n"
			"terms=%zu->%zu types=%zu->%zu judgements=%zu\n"
			"discharged_link_effect_equations=%zu\n"
			"target_exports=%zu provider_exports=%zu dependencies=%zu reexport=%s\n",
			link_target_path,
			link_provider_count,
			link_output_path ? link_output_path : "<none>",
			before_terms,
			term_db->term_count,
			before_types,
			type_declarations->semantic_schema.type_count,
			judgement_db->proposition_count,
			discharged_link_effect_equations,
			artifact_interface->term_export_count,
			total_provider_exports,
			artifact_interface->dependency_count,
			link_reexport_providers ? "yes" : "no"
		);
		for (size_t i = 0; i < artifact_interface->term_export_count; ++i) {
			const struct prototype_artifact_term_export* export =
				&artifact_interface->term_exports[i];
			printf("linked target term %s -> term#%u\n",
				symbol_to_string(symbols, export->name_symbol_id),
				export->local_term);
		}
		symbol_table_free(symbols);
		return 0;
	}
	if (interface_input_path) {
		struct prototype_artifact_interface* artifact_interface =
			&storage->artifact.interface;
		struct prototype_artifact_relocation_table* relocation_table =
			&storage->artifact.relocation;
		struct prototype_artifact_debug_table* debug_table =
			&storage->artifact.debug;
		struct prototype_compile_metadata* artifact_metadata = metadata;
		FILE* artifact_file = fopen(interface_input_path, "r");
		if (!artifact_file) {
			fprintf(stderr, "%s: failed to open artifact interface\n", interface_input_path);
			symbol_table_free(symbols);
			return 1;
		}
		int read_status = prototype_artifact_read_text_interface(
			artifact_file,
			symbols,
			prototype_default_intrinsic_typing_environment(),
			artifact_interface
		);
		if (read_status != 0) {
			fclose(artifact_file);
			fprintf(stderr, "%s: failed to read artifact interface\n", interface_input_path);
			symbol_table_free(symbols);
			return 1;
		}
		if (read_graph) {
			struct prototype_artifact_graph_decode_state decode_state;
			prototype_artifact_graph_decode_state_init(&decode_state);
			const char* artifact_graph_stage = "graph";
			if (prototype_artifact_read_text_graph(
					artifact_file,
					symbols,
					prototype_default_intrinsic_typing_environment(),
					artifact_interface,
					&artifact_metadata->dimension_operators,
					term_db,
					type_declarations,
					judgement_db,
					&decode_state
				) != 0 ||
				((artifact_graph_stage = "typed-occurrences"),
				prototype_artifact_read_text_typed_occurrences(
					artifact_file,
					symbols,
					term_db,
					type_declarations,
					judgement_db,
					artifact_metadata,
					&decode_state
				) != 0) ||
				((artifact_graph_stage = "universe"),
				prototype_artifact_read_text_universe(
					artifact_file,
					universe_db
				) != 0) ||
				((artifact_graph_stage = "semantic-keys"),
				 prototype_artifact_interface_validate_semantic_keys(
					artifact_interface,
					symbols,
					term_db,
					type_declarations,
					&artifact_metadata->dimension_operators
				) != 0) ||
				((artifact_graph_stage = "export-claims"),
				 artifact_export_claim_ids_match_loaded_image(
					artifact_interface, judgement_db
				) != 0) ||
				((artifact_graph_stage = "debug"),
				prototype_artifact_read_text_debug(
					artifact_file,
					symbols,
					debug_table
				) != 0) ||
				((artifact_graph_stage = "relocation"),
				prototype_artifact_read_text_relocation(
					artifact_file,
					symbols,
					relocation_table
				) != 0) ||
				((artifact_graph_stage = "accepted-graph"),
				prototype_judgement_validate_accepted_graph(
					term_db,
					type_declarations,
					prototype_default_intrinsic_typing_environment(),
					&artifact_metadata->contexts,
					&(const struct prototype_context_classifier_view) {
						.contexts = &artifact_metadata->contexts
					},
					&artifact_metadata->substitutions,
					&artifact_metadata->dimension_operators,
					&artifact_metadata->typed_occurrences,
					&(const struct prototype_judgement_occurrence_context_view) {
						.publication = &artifact_metadata->typed_publication
					},
					judgement_db
				) != 0) ||
				((artifact_graph_stage = "identity-roots"),
				 prototype_artifact_interface_validate_identity_roots(
					artifact_interface,
					term_db,
					type_declarations,
					&artifact_metadata->contexts,
					&artifact_metadata->dimension_operators,
					judgement_db
				) != 0) ||
				((artifact_graph_stage = "function-graph-associations"),
				 prototype_artifact_interface_validate_function_graph_associations(
					artifact_interface,
					term_db,
					type_declarations,
					&artifact_metadata->contexts,
					judgement_db
				) != 0) ||
				((artifact_graph_stage = "universe-provenance"),
				 prototype_universe_validate_replay(
					universe_db,
					type_declarations,
					term_db,
					&artifact_metadata->typed_occurrences,
					judgement_db
				) != 0) ||
				((artifact_graph_stage = "accepted-exports"),
				 artifact_exports_have_accepted_claims(
					artifact_interface,
					term_db,
					judgement_db,
					artifact_metadata,
					0
				) != 0)) {
				prototype_artifact_graph_decode_state_destroy(&decode_state);
				fclose(artifact_file);
				fprintf(stderr,
					"%s: failed to read artifact graph/universe/relocation "
					"stage=%s\n",
					interface_input_path, artifact_graph_stage);
				symbol_table_free(symbols);
				return 1;
			}
			prototype_artifact_graph_decode_state_destroy(&decode_state);
			if (check_backend_name) {
				int backend;
				if (strcmp(check_backend_name, "interpreter") == 0) {
					backend = PROTOTYPE_BACKEND_INTERPRETER;
				} else if (strcmp(check_backend_name, "c") == 0) {
					backend = PROTOTYPE_BACKEND_C;
				} else if (strcmp(check_backend_name, "verilog") == 0) {
					backend = PROTOTYPE_BACKEND_VERILOG;
				} else {
					fclose(artifact_file);
					fprintf(stderr, "unknown backend: %s\n", check_backend_name);
					symbol_table_free(symbols);
					return 1;
				}
				if (prototype_compile_metadata_validate_backend(
						artifact_metadata,
						backend,
						prototype_backend_default_capabilities(backend)
					) != 0) {
					fclose(artifact_file);
					fprintf(
						stderr,
						"backend %s is incompatible with artifact policy or capabilities\n",
						check_backend_name
					);
					symbol_table_free(symbols);
					return 1;
				}
				printf("backend %s compatible yes\n", check_backend_name);
			}
		}
		fclose(artifact_file);
		printf(
			"#### Artifact Interface ####\n"
			"term_exports=%zu type_exports=%zu constructor_exports=%zu dependencies=%zu\n",
			artifact_interface->term_export_count,
			artifact_interface->type_export_count,
			artifact_interface->constructor_export_count,
			artifact_interface->dependency_count
			);
			for (size_t i = 0; i < artifact_interface->term_export_count; ++i) {
				const struct prototype_artifact_term_export* term_export =
					&artifact_interface->term_exports[i];
				printf("interface term %s local_term#%u classifier#%u transparency=%s key=%llu classifier_key=%llu\n",
					symbol_to_string(symbols, term_export->name_symbol_id),
					term_export->local_term,
					term_export->classifier,
					term_export->transparency == PROTOTYPE_ARTIFACT_EXPORT_OPAQUE ?
						"opaque" : "transparent",
					(unsigned long long)term_export->canonical_key.hash,
					(unsigned long long)term_export->classifier_key.hash);
			}
			for (size_t i = 0; i < artifact_interface->type_export_count; ++i) {
				const struct prototype_artifact_type_export* type_export =
					&artifact_interface->type_exports[i];
			printf("interface type %s local_type#%u core_representation_anchor_type#%u constructors=%u representation_fingerprint=%llu\n",
				symbol_to_string(symbols, type_export->name_symbol_id),
				type_export->local_type_id,
				type_export->core_representation_anchor_type_id,
				type_export->constructor_count,
				(unsigned long long)type_export->representation_fingerprint.hash);
		}
		for (size_t i = 0; i < artifact_interface->constructor_export_count; ++i) {
			const struct prototype_artifact_constructor_export* constructor_export =
				&artifact_interface->constructor_exports[i];
			printf("interface constructor type_export#%u.%s ordinal=%u fields=%u constructor_classifier=%u\n",
				constructor_export->type_export_index,
				symbol_to_string(symbols, constructor_export->name_symbol_id),
				constructor_export->ordinal,
			constructor_export->readback_field_count,
				constructor_export->constructor_classifier);
		}
		if (read_graph) {
			printf(
				"\n"
				"#### Artifact Graph ####\n"
				"terms=%zu cases=%zu case_binders=%zu frames=%zu types=%zu constructors=%zu type_exprs=%zu judgements=%zu proofs=%zu\n",
				term_db->term_count,
				term_db->case_count,
				term_db->case_binder_count,
				term_db->ih_scope_count,
				type_declarations->semantic_schema.type_count,
				type_declarations->semantic_schema.constructor_count,
				type_declarations->readback.expr_count,
				judgement_db->proposition_count,
				judgement_db->derivation_candidate_count
			);
			printf(
				"universe_nodes=%zu universe_edges=%zu universe_levels=%zu universe_constraints=%zu universe_obligations=%zu certificate=%s\n",
				universe_db->node_count,
				universe_db->edge_count,
				universe_db->level_count,
				universe_db->constraint_count,
				universe_db->obligation_span_count,
				universe_db->certificate.state ==
					PROTOTYPE_UNIVERSE_CERTIFICATE_CLOSED ? "closed" : "invalid"
			);
			printf(
				"graph_next_level_var=%u judgement_next_universe_var=%u\n",
				type_declarations->readback.next_level_var,
				judgement_db->next_universe_var
			);
			printf(
				"typed_occurrences=%zu occurrence_match_cases=%zu verification_obligations=%zu\n",
				artifact_metadata->typed_occurrences.occurrence_count,
				artifact_metadata->typed_occurrences.case_count,
				prototype_verification_db_count(&artifact_metadata->verification)
			);
			printf(
				"relocation_external_terms=%zu relocation_resolved_external_terms=%zu relocation_external_type_exprs=%zu relocation_resolved_external_type_exprs=%zu relocation_external_type_formers=%zu relocation_resolved_external_type_formers=%zu relocation_resolved_constructor_owners=%zu\n",
				relocation_table->external_term_ref_count,
				relocation_table->resolved_external_term_ref_count,
				relocation_table->external_type_expr_ref_count,
				relocation_table->resolved_external_type_expr_ref_count,
				relocation_table->external_type_former_ref_count,
				relocation_table->resolved_external_type_former_ref_count,
				relocation_table->resolved_constructor_owner_ref_count
			);
			printf(
				"debug_term_names=%zu debug_type_names=%zu debug_constructor_names=%zu\n",
				debug_table->term_name_count,
				debug_table->type_name_count,
				debug_table->constructor_name_count
			);
			print_artifact_context_and_substitution_inspection(
				stdout,
				symbols,
				prototype_default_intrinsic_typing_environment(),
				artifact_interface,
				type_declarations,
				term_db,
				artifact_metadata
			);
			for (size_t i = 0; i < relocation_table->resolved_external_term_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_term_ref* ref =
					&relocation_table->resolved_external_term_refs[i];
				printf(
					"resolved external term term#%u -> export#%u.%s\n",
					ref->term,
					ref->term_export_index,
						symbol_to_string(symbols, ref->name.name_symbol_id)
				);
			}
			for (size_t i = 0; i < relocation_table->resolved_external_type_expr_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_type_expr_ref* ref =
					&relocation_table->resolved_external_type_expr_refs[i];
				printf(
					"resolved external type expr type_expr#%u -> type_export#%u.%s representation_fingerprint=%llu\n",
					ref->type_expr,
					ref->type_export_index,
					symbol_to_string(symbols, ref->name.name_symbol_id),
					(unsigned long long)ref->representation_fingerprint.hash
				);
			}
			for (size_t i = 0; i < relocation_table->external_type_former_ref_count; ++i) {
				const struct prototype_artifact_external_type_former_ref* ref =
					&relocation_table->external_type_former_refs[i];
				printf(
					"external type former type_expr#%u -> %s\n",
					ref->type_expr,
					symbol_to_string(symbols, ref->name_symbol_id)
				);
			}
			for (size_t i = 0; i < relocation_table->resolved_external_type_former_ref_count; ++i) {
				const struct prototype_artifact_resolved_external_type_former_ref* ref =
					&relocation_table->resolved_external_type_former_refs[i];
				printf(
					"resolved external type former type_expr#%u -> type_export#%u.%s representation_fingerprint=%llu\n",
					ref->type_expr,
					ref->type_export_index,
					symbol_to_string(symbols, ref->name.name_symbol_id),
					(unsigned long long)ref->representation_fingerprint.hash
				);
			}
			for (size_t i = 0; i < relocation_table->resolved_constructor_owner_ref_count; ++i) {
				const struct prototype_artifact_resolved_constructor_owner_ref* ref =
					&relocation_table->resolved_constructor_owner_refs[i];
				printf(
					"resolved constructor owner kind=%d source#%u owner#%u ordinal=%u key=%llu\n",
					ref->source_kind,
					ref->source,
					ref->owner,
					ref->ordinal,
					(unsigned long long)ref->owner_key.hash
				);
			}
		}
		symbol_table_free(symbols);
		return 0;
	}
	program->intrinsic_environment = prototype_default_intrinsic_typing_environment();
	program->symbols = symbols;
	program->namespace_symbol_id = -1;
	program->asts = ast_db;
	program->type_declarations = type_declarations;
	program->judgement = judgement_db;
	program->metadata = metadata;
	program->universe = universe_db;
	program->compile_options.compile_policy = compile_policy;
	program->compile_options.definition_thunk_policy = definition_thunk_policy;
	program->compile_options.solve_effort_is_set = solve_effort_is_set;
	program->compile_options.solve_effort_steps = solve_effort_steps;

	for (int i = file_arg; i < argc; ++i) {
		if (prototype_read_ast_file(argv[i], program, &error) != 0) {
			fprintf(
				stderr,
				"%s:%u:%u: %s\n",
				error.filename ? error.filename : argv[i],
				error.line,
				error.column,
				error.message[0] ? error.message : "read failed"
			);
			if (error.diagnostic_code ==
				PROTOTYPE_READ_DIAGNOSTIC_UNSUPPORTED_INDEXED_FAMILY) {
				fprintf(stderr, "read-diagnostic diagnostic-code=unsupported-indexed-family span=%u:%u\n",
					error.line, error.column);
			} else if (error.diagnostic_code ==
				PROTOTYPE_READ_DIAGNOSTIC_NESTED_MATCH_GROUPING) {
				fprintf(stderr, "read-diagnostic diagnostic-code=nested-match-grouping span=%u:%u\n",
					error.line, error.column);
			}
			symbol_table_free(symbols);
			return 1;
		}
	}
	const struct prototype_artifact_interface* imported_interface_refs[IMPORT_INTERFACE_CAPACITY];
	for (size_t i = 0; i < import_interface_count; ++i) {
		if (read_import_artifact_into_slot(
				import_interface_paths[i],
				symbols,
			program,
			i,
			universe_db,
			storage
			) != 0) {
			fprintf(stderr, "%s: failed to read import artifact\n", import_interface_paths[i]);
			symbol_table_free(symbols);
			return 1;
		}
		imported_interface_refs[i] = &storage->imported_artifacts[i].interface;
	}
	for (size_t i = 0; i < ast_db->import_count; ++i) {
		int import_status = add_source_import_from_search_dirs(
			&ast_db->imports[i],
			import_search_dirs,
			import_search_dir_count,
			symbols,
			program,
			imported_interface_refs,
			&import_interface_count,
			universe_db,
			storage
		);
		if (import_status != 0) {
			const char* import_name =
				symbol_to_string(symbols, ast_db->imports[i].name_symbol_id);
			if (import_status > 0) {
				fprintf(stderr, "%s:%u:%u: unresolved import %s\n",
					argv[file_arg],
					ast_db->imports[i].name_span.line,
					ast_db->imports[i].name_span.column,
					import_name ? import_name : "<unknown>");
			} else {
				fprintf(stderr, "%s:%u:%u: failed to load import %s\n",
					argv[file_arg],
					ast_db->imports[i].name_span.line,
					ast_db->imports[i].name_span.column,
					import_name ? import_name : "<unknown>");
			}
			symbol_table_free(symbols);
			return 1;
		}
	}
		const char* namespace_source = namespace_name ? namespace_name : argv[file_arg];
		int namespace_symbol_id = namespace_name ?
			namespace_symbol_from_text(symbols, namespace_source) :
			namespace_symbol_from_path(symbols, namespace_source);
		if (namespace_symbol_id < 0) {
			fprintf(stderr, "%s: failed to determine namespace\n", namespace_source);
			symbol_table_free(symbols);
			return 1;
		}
		program->namespace_symbol_id = namespace_symbol_id;
		int show_function_graph_symbol = -1;
		if (show_function_graph_name) {
			show_function_graph_symbol = symbol_intern(
				symbols,
				show_function_graph_name,
				strlen(show_function_graph_name)
			);
			enum prototype_function_graph_inspection_state request_state =
				show_function_graph_symbol < 0 ?
					PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID :
					prototype_function_graph_request_inspection(
						ast_db, metadata, show_function_graph_symbol
					);
			if (request_state == PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AMBIGUOUS ||
				request_state == PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID) {
				fprintf(
					stderr,
					"function graph inspection request %s: %s\n",
					show_function_graph_name,
					prototype_function_graph_inspection_state_name(request_state)
				);
				symbol_table_free(symbols);
				return 1;
			}
		}
		if (prototype_compile_graph_with_imports(
			program,
			imported_interface_refs,
			import_interface_count,
			&error
		) != 0) {
		fprintf(
			stderr,
			"%s:%u:%u: %s\n",
			error.filename ? error.filename : argv[file_arg],
			error.line,
			error.column,
			error.message[0] ? error.message : "graph compile failed"
		);
		prototype_diagnostic_print_resolve_errors(stderr, symbols, metadata);
		prototype_diagnostic_print_compile_diagnostics(stderr, metadata);
		symbol_table_free(symbols);
		return 1;
	}
	if (show_function_graph_name) {
		enum prototype_function_graph_inspection_state inspection_state =
			prototype_function_graph_inspect(
				stdout,
				symbols,
				program->intrinsic_environment,
				term_db,
				type_declarations,
				metadata,
				show_function_graph_symbol
			);
		if (inspection_state != PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AVAILABLE) {
			fprintf(
				stderr,
				"function graph inspection %s: %s\n",
				show_function_graph_name,
				prototype_function_graph_inspection_state_name(inspection_state)
			);
			symbol_table_free(symbols);
			return 1;
		}
	}
	struct prototype_artifact_interface* artifact_interface =
		&storage->artifact.interface;
	if (prototype_artifact_interface_build_from_metadata(
			artifact_interface,
			symbols,
			program->intrinsic_environment,
			metadata,
			term_db,
			type_declarations,
			judgement_db
		) != 0) {
		fprintf(stderr, "%s: failed to build artifact interface\n", argv[file_arg]);
		symbol_table_free(symbols);
		return 1;
	}
	for (size_t i = 0; i < opaque_export_count; ++i) {
		if (mark_opaque_export(
				symbols,
				artifact_interface,
				opaque_export_names[i]
			) != 0) {
			fprintf(stderr, "%s: unknown opaque export: %s\n",
				artifact_output_path ? artifact_output_path : argv[file_arg],
				opaque_export_names[i]);
			symbol_table_free(symbols);
			return 1;
		}
	}
	if (prototype_artifact_interface_collect_dependencies(
			artifact_interface,
			term_db,
			type_declarations,
			judgement_db
		) != 0) {
		fprintf(stderr, "%s: failed to collect artifact dependencies\n", argv[file_arg]);
		symbol_table_free(symbols);
		return 1;
	}
	for (size_t i = 0; i < ast_db->import_count; ++i) {
		if (prototype_artifact_interface_add_dependency(
				artifact_interface,
				ast_db->imports[i].name_symbol_id
			) != 0) {
			fprintf(stderr, "%s: failed to add source import dependency\n", argv[file_arg]);
			symbol_table_free(symbols);
			return 1;
		}
	}
	prototype_artifact_interface_set_namespace(artifact_interface, namespace_symbol_id);
	if (check_source_exports_normalization_equal_left_name) {
		int check_status = check_compiled_exports_normalization_equal(
			symbols,
			artifact_interface,
			term_db,
			type_declarations,
			prototype_artifact_interface_storage_definitions(&storage->artifact),
			prototype_artifact_interface_storage_definition_capacity(
				&storage->artifact
			),
			check_source_exports_normalization_equal_left_name,
			check_source_exports_normalization_equal_right_name,
			reduction_mode
		);
		if (check_status < 0) {
			fprintf(stderr, "%s: failed to check source export normalization equality\n", argv[file_arg]);
		}
		symbol_table_free(symbols);
		return check_status == 0 ? 0 : 1;
	}
	if (trace_source_export_evaluation_name) {
		int trace_status = trace_compiled_export_evaluation(
			symbols,
			artifact_interface,
			term_db,
			type_declarations,
			program->intrinsic_environment,
			prototype_artifact_interface_storage_definitions(&storage->artifact),
			prototype_artifact_interface_storage_definition_capacity(
				&storage->artifact
			),
			trace_source_export_evaluation_name
		);
		if (trace_status < 0) {
			fprintf(
				stderr,
				"%s: failed to trace source export evaluation: %s\n",
				argv[file_arg],
				trace_source_export_evaluation_name
			);
		}
		symbol_table_free(symbols);
		return trace_status == 0 ? 0 : 1;
	}
	if (artifact_output_path) {
		FILE* artifact_file = fopen(artifact_output_path, "w");
		if (!artifact_file) {
			fprintf(stderr, "%s: failed to open artifact output\n", artifact_output_path);
			symbol_table_free(symbols);
			return 1;
		}
		struct prototype_context_classifier_view context_classifiers;
		int view_status = prototype_typing_context_classifier_view_init(
			&metadata->contexts,
			&context_classifiers
		);
		int write_status = view_status == 0 ? prototype_artifact_write_text(
			artifact_file,
			symbols,
			program->intrinsic_environment,
			artifact_interface,
			term_db,
			type_declarations,
			judgement_db,
			NULL,
			universe_db,
			ast_db,
			metadata,
			&context_classifiers
		) : -1;
		fclose(artifact_file);
		if (write_status != 0) {
			fprintf(stderr, "%s: failed to write artifact\n", artifact_output_path);
			symbol_table_free(symbols);
			return 1;
		}
	}
	if (quiet) {
		symbol_table_free(symbols);
		return 0;
	}
	printf(
		"#### AST ####\n"
		"asts=%zu ast_expectations=%zu ast_assignments=%zu\n"
		"\n"
		"#### Raw Graph ####\n"
		"types=%zu constructors=%zu labels=%zu terms=%zu\n",
		ast_db->node_count,
		ast_db->expectation_count,
		ast_db->assignment_count,
		type_declarations->semantic_schema.type_count,
		type_declarations->semantic_schema.constructor_count,
		metadata->label_count,
		term_db->term_count
	);

	for (size_t i = 0; i < type_declarations->semantic_schema.type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[i];
		if (type->name_symbol_id < 0 || type->type_index == PROTOTYPE_INVALID_ID) {
			continue;
		}
		prototype_diagnostic_print_type_declaration(stdout, symbols, type_declarations, type);
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			uint32_t constructor_id = type->first_constructor + j;
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->semantic_schema.constructor_declarations[constructor_id];
			const struct prototype_type_constructor_readback* readback =
				prototype_type_constructor_readback_get(
				&type_declarations->semantic_schema,
				&type_declarations->readback, constructor_id
				);
			const struct prototype_constructor_classifier_cache_entry* cache =
				prototype_type_constructor_classifier_cache_get(
				&type_declarations->semantic_schema,
				&type_declarations->constructor_classifier_cache, constructor_id
				);
			if (constructor->name_symbol_id < 0 ||
				constructor->owner_type == PROTOTYPE_INVALID_ID || !readback || !cache) {
				continue;
			}
			printf("constructor ");
			prototype_diagnostic_print_type_namespace(stdout, symbols, type_declarations, type);
			printf(".%s readback_fields=%u constructor_classifier=%u\n",
				symbol_to_string(symbols, constructor->name_symbol_id),
				readback->field_count,
				cache->classifier);
		}
	}
	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		printf("term %s := ", symbol_to_string(symbols, label->name_symbol_id));
		prototype_type_term_print_debug(
			stdout, symbols, program->intrinsic_environment,
			type_declarations, term_db, label->term
		);
		printf("\n");
	}
	printf("\n#### Typed Occurrences ####\ntyped-occurrences=%zu cases=%zu\n",
		metadata->typed_occurrences.occurrence_count,
		metadata->typed_occurrences.case_count);
	const struct prototype_typed_occurrence_graph* debug_occurrences =
		prototype_compile_metadata_typed_occurrences_const(metadata);
	printf(
		"compile-budget policy=%d capabilities=%" PRIu64
		" effort-model=%u effort=%" PRIu64 " used=%" PRIu64
		" exhausted=%s exhausted-phase=%d"
		" constraints=%" PRIu64 " solved=%" PRIu64 " residual=%" PRIu64
		" incomplete=%" PRIu64 "\n",
		metadata->compile_policy,
		metadata->required_runtime_capabilities,
		metadata->effort.cost_model_version,
		metadata->effort.limit,
		metadata->effort.used,
		metadata->effort.exhausted ? "yes" : "no",
		metadata->effort.exhausted_phase,
		metadata->solver_constraint_count,
		metadata->solver_solved_count,
		metadata->solver_residual_count,
		metadata->solver_incomplete_count
	);
	printf(
		"effort-phases graph=%" PRIu64 " classifier=%" PRIu64
		" normalization=%" PRIu64 " motive=%" PRIu64
		" proof=%" PRIu64 " function-graph=%" PRIu64
		" checker=%" PRIu64 " merge=%" PRIu64 "\n",
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_GRAPH],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_CLASSIFIER],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_NORMALIZATION],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_MOTIVE],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_PROOF],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_CHECKER],
		metadata->effort.phase_used[PROTOTYPE_EFFORT_PHASE_MERGE]
	);
	if (getenv("A_PROGRAM_PERFORMANCE_COUNTERS")) {
		struct prototype_term_intern_stats intern_stats;
		struct prototype_term_normalization_cache_stats normalization_stats;
		prototype_term_intern_get_stats(term_db, &intern_stats);
		prototype_term_normalization_cache_get_stats(
			term_db, &normalization_stats
		);
		fprintf(
			stderr,
			"A_PROGRAM_PERFORMANCE_COUNTERS 1 "
			"term_formation=%" PRIu64 " term_unique=%" PRIu64 " "
				"intern_probes=%" PRIu64 " exact_probes=%" PRIu64 " "
				"alpha_compares=%" PRIu64 " alpha_compare_node_visits=%" PRIu64
				" max_alpha_bucket_probes=%" PRIu64 " "
			"intern_rebuilds=%" PRIu64 " normalization_hits=%" PRIu64 " "
			"normalization_misses=%" PRIu64 " normalization_probes=%" PRIu64 " "
				"normalization_evictions=%" PRIu64 " normalization_invalidations=%" PRIu64
				" graph_mutation_invalidations=%" PRIu64
				" ih_scope_invalidations=%" PRIu64
				" type_former_invalidations=%" PRIu64
				" empty_cache_invalidations=%" PRIu64 "\n",
			intern_stats.formation_request_count,
			intern_stats.unique_term_count,
			intern_stats.bucket_probe_count,
				intern_stats.exact_probe_count,
				intern_stats.alpha_compare_count,
				intern_stats.alpha_compare_node_visit_count,
				intern_stats.max_alpha_bucket_probe_count,
			intern_stats.index_rebuild_count,
			normalization_stats.hit_count,
			normalization_stats.miss_count,
			normalization_stats.probe_count,
				normalization_stats.eviction_count,
				normalization_stats.invalidation_count,
				normalization_stats.graph_mutation_invalidation_count,
				normalization_stats.ih_scope_invalidation_count,
				normalization_stats.type_former_invalidation_count,
				normalization_stats.empty_cache_invalidation_count
		);
		for (int tag = 1; tag <= PROTOTYPE_TERM_TAG_MAX; ++tag) {
			if (intern_stats.formation_requests_by_tag[tag] == 0 &&
				intern_stats.unique_terms_by_tag[tag] == 0 &&
				intern_stats.bucket_probes_by_tag[tag] == 0 &&
				intern_stats.alpha_compares_by_tag[tag] == 0) {
				continue;
			}
			fprintf(
				stderr,
				"A_PROGRAM_INTERN_TAG 1 tag=%d formations=%" PRIu64
				" unique=%" PRIu64 " probes=%" PRIu64
				" alpha_compares=%" PRIu64 "\n",
				tag,
				intern_stats.formation_requests_by_tag[tag],
				intern_stats.unique_terms_by_tag[tag],
				intern_stats.bucket_probes_by_tag[tag],
				intern_stats.alpha_compares_by_tag[tag]
			);
		}
		fprintf(
			stderr,
			"A_PROGRAM_CONSTRUCTOR_SPECIALIZATION_COUNTERS 1 attempts=%" PRIu64
				" classifier_cache_hits=%" PRIu64
				" classifier_cache_misses=%" PRIu64
				" reindex_requests=%" PRIu64
				" type_declaration_formations=%" PRIu64
				" type_view_formations=%" PRIu64 "\n",
				type_declarations->specialization_stats.specialization_attempt_count,
				type_declarations->specialization_stats.classifier_cache_hit_count,
				type_declarations->specialization_stats.classifier_cache_miss_count,
				type_declarations->specialization_stats.reindex_request_count,
				intern_stats.formation_requests_by_tag[PROTOTYPE_TERM_TYPE_DECLARATION],
			intern_stats.formation_requests_by_tag[PROTOTYPE_TERM_TYPE_VIEW]
		);
		fprintf(
			stderr,
			"A_PROGRAM_COMPILE_PHASE_COUNTERS 1 graph_ns=%" PRIu64
			" fixed_point_ns=%" PRIu64 " materialization_ns=%" PRIu64
			" termination_evidence_ns=%" PRIu64 " evidence_closure_ns=%" PRIu64
			" accepted_replay_ns=%" PRIu64 "\n",
			metadata->graph_build_time_ns,
			metadata->fixed_point_time_ns,
			metadata->proof_materialization_time_ns,
			metadata->termination_evidence_time_ns,
			metadata->evidence_closure_time_ns,
			metadata->accepted_replay_time_ns
		);
		fprintf(
			stderr,
			"A_PROGRAM_PROOF_MATERIALIZATION_COUNTERS 1 passes=%" PRIu64
			" full_scans=%" PRIu64 " rounds=%" PRIu64
			" occurrence_visits=%" PRIu64
			" evidence_retries=%" PRIu64
			" reify_roots=%" PRIu64 " reify_recursive=%" PRIu64
			" reify_success=%" PRIu64 " reify_residual=%" PRIu64
			" reify_failure=%" PRIu64 " accepted_reuse=%" PRIu64
			" current_pass_reuse=%" PRIu64 " cycles=%" PRIu64
			" termination_claims=%" PRIu64 "\n",
			metadata->proof_materialization_pass_count,
			metadata->proof_materialization_full_scan_count,
			metadata->proof_materialization_round_count,
			metadata->proof_materialization_occurrence_visit_count,
			metadata->evidence_consumer_retry_count,
			metadata->proof_reify_root_count,
			metadata->proof_reify_recursive_count,
			metadata->proof_reify_success_count,
			metadata->proof_reify_residual_count,
			metadata->proof_reify_failure_count,
			metadata->proof_reify_accepted_reuse_count,
			metadata->proof_reify_current_pass_reuse_count,
			metadata->proof_reify_cycle_count,
			metadata->termination_evidence_claim_count
		);
		fprintf(
			stderr,
			"A_PROGRAM_SOLVER_COUNTERS 1 constraint_generations=%" PRIu64
			" constraint_indexes=%" PRIu64
			" computation_generations=%" PRIu64
			" enqueue_requests=%" PRIu64
			" enqueue_duplicates=%" PRIu64
			" enqueues=%" PRIu64 " pops=%" PRIu64
			" context_resolutions=%" PRIu64
			" context_index_rebuilds=%" PRIu64
			" substitution_index_rebuilds=%" PRIu64 "\n",
			metadata->constraint_generation_pass_count,
			metadata->constraint_index_pass_count,
			metadata->computation_constraint_generation_pass_count,
			metadata->constraint_enqueue_request_count,
			metadata->constraint_enqueue_duplicate_count,
			metadata->constraint_enqueue_count,
			metadata->constraint_pop_count,
			metadata->context_resolution_pass_count,
			metadata->context_index_rebuild_count,
			metadata->substitution_index_rebuild_count
		);
		for (int kind = 1;
			kind < OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT;
			++kind) {
			if (metadata->constraint_pop_by_kind[kind] == 0 &&
				metadata->constraint_changed_by_kind[kind] == 0 &&
				metadata->constraint_noop_by_kind[kind] == 0) {
				continue;
			}
			fprintf(
				stderr,
				"A_PROGRAM_SOLVER_KIND_COUNTERS 1 kind=%d pops=%" PRIu64
				" changed=%" PRIu64 " noop=%" PRIu64 "\n",
				kind,
				metadata->constraint_pop_by_kind[kind],
				metadata->constraint_changed_by_kind[kind],
				metadata->constraint_noop_by_kind[kind]
			);
		}
		fprintf(
			stderr,
			"A_PROGRAM_LAYER_T_TOPOLOGY_COUNTERS 1 sealed=%u digest=%" PRIu64
			" validations=%" PRIu64 " failures=%" PRIu64
			" external_motive_writes=%" PRIu64
			" post_seal_motive_writes=%" PRIu64 "\n",
			metadata->constraint_topology_sealed,
			metadata->constraint_topology_digest,
			metadata->constraint_topology_validation_count,
			metadata->constraint_topology_validation_failure_count,
			metadata->motive_write_outside_solver_count,
			metadata->motive_write_after_solution_seal_count
		);
		for (int domain = 0;
			domain < PROTOTYPE_COMPILE_SOLUTION_DOMAIN_COUNT;
			++domain) {
			fprintf(
				stderr,
				"A_PROGRAM_LAYER_T_SEAL_GUARD_COUNTERS 1 domain=%d "
				"proof_solver_calls=%" PRIu64 "\n",
				domain,
				metadata->proof_solver_call_by_domain[domain]
			);
		}
		for (int cause = 0;
			cause < PROTOTYPE_COMPILE_ENQUEUE_CAUSE_COUNT;
			++cause) {
			if (metadata->constraint_enqueue_by_cause[cause] == 0 &&
				metadata->constraint_enqueue_duplicate_by_cause[cause] == 0) {
				continue;
			}
			fprintf(
				stderr,
				"A_PROGRAM_ENQUEUE_CAUSE_COUNTERS 1 cause=%d enqueues=%" PRIu64
				" duplicates=%" PRIu64 "\n",
				cause,
				metadata->constraint_enqueue_by_cause[cause],
				metadata->constraint_enqueue_duplicate_by_cause[cause]
			);
		}
		for (int domain = 0;
			domain < PROTOTYPE_COMPILE_SCAN_DOMAIN_COUNT;
			++domain) {
			fprintf(
				stderr,
				"A_PROGRAM_FULL_SCAN_COUNTERS 1 domain=%d scans=%" PRIu64
				" visits=%" PRIu64 "\n",
				domain,
				metadata->full_scan_count[domain],
				metadata->full_scan_visit_count[domain]
			);
		}
		fprintf(
			stderr,
			"A_PROGRAM_FUNCTION_GRAPH_PHASE_COUNTERS 1 source_compile_ns=%" PRIu64
			" generation_ns=%" PRIu64 " generated_compile_ns=%" PRIu64
			" source_ast_nodes=%" PRIu64 " generated_ast_nodes=%" PRIu64
			" generated_assignments=%" PRIu64 " generated_types=%" PRIu64
			" generated_constructors=%" PRIu64 "\n",
			metadata->source_compile_time_ns,
			metadata->function_graph_generation_time_ns,
			metadata->function_graph_generated_compile_time_ns,
			metadata->function_graph_source_ast_node_count,
			metadata->function_graph_generated_ast_node_count,
			metadata->function_graph_generated_assignment_count,
			metadata->function_graph_generated_type_count,
			metadata->function_graph_generated_constructor_count
		);
		fprintf(
			stderr,
			"A_PROGRAM_ACCEPTED_REPLAY_COUNTERS 1 validations=%" PRIu64
			" proposition_scans=%" PRIu64 " claim_scans=%" PRIu64
			" derivation_scans=%" PRIu64
			" propositions=%" PRIu64 " claims=%" PRIu64
			" derivations=%" PRIu64 " premises=%" PRIu64
			" scratch_initializations=%" PRIu64
			" scratch_index_rebuilds=%" PRIu64
			" occurrence_validations=%" PRIu64
			" usage_solves=%" PRIu64 " reachability_queries=%" PRIu64 "\n",
			judgement_db->accepted_replay_stats.validation_count,
			judgement_db->accepted_replay_stats.proposition_full_scan_count,
			judgement_db->accepted_replay_stats.claim_full_scan_count,
			judgement_db->accepted_replay_stats.derivation_full_scan_count,
			judgement_db->accepted_replay_stats.proposition_visit_count,
			judgement_db->accepted_replay_stats.claim_visit_count,
			judgement_db->accepted_replay_stats.derivation_visit_count,
			judgement_db->accepted_replay_stats.premise_visit_count,
			judgement_db->accepted_replay_stats.scratch_initialization_count,
			judgement_db->accepted_replay_stats.scratch_index_rebuild_count,
			judgement_db->accepted_replay_stats.occurrence_validation_count,
			judgement_db->accepted_replay_stats.usage_solve_count,
			judgement_db->accepted_replay_stats.reachability_query_count
		);
		fprintf(
			stderr,
			"A_PROGRAM_CONTEXT_RESOLUTION_COUNTERS 1 requests=%" PRIu64
			" skips=%" PRIu64 " context_visits=%" PRIu64
			" context_changes=%" PRIu64 " context_inserts=%" PRIu64
				" substitution_visits=%" PRIu64 " substitution_rebases=%" PRIu64
				" substitution_inserts=%" PRIu64 " root_projections=%" PRIu64
				" binder_owner_rebuilds=%" PRIu64 "\n",
			metadata->context_resolution_request_count,
			metadata->context_resolution_skip_count,
			metadata->context_resolution_context_visit_count,
			metadata->context_resolution_context_change_count,
			metadata->context_resolution_context_insert_count,
			metadata->context_resolution_substitution_visit_count,
			metadata->context_resolution_substitution_rebase_count,
			metadata->context_resolution_substitution_insert_count,
				metadata->context_resolution_root_projection_count,
				metadata->binder_owner_index_rebuild_count
		);
	}
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&metadata->typed_occurrences.occurrences[i];
		printf("occurrence#%zu %s core#%u classifier#%u ast#%u",
			i,
			operation_tag_name(operation->tag),
			operation->core_term,
			operation->classifier,
			operation->source_ast);
		if (operation->source_symbol_id >= 0) {
			printf(" name=%s", symbol_to_string(symbols, operation->source_symbol_id));
		}
		if (operation->binder_symbol_id >= 0) {
			printf(" binder=%s", symbol_to_string(symbols, operation->binder_symbol_id));
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			uint32_t scrutinee_occurrence;
			if (prototype_typed_occurrence_graph_child(
					debug_occurrences, (uint32_t)i,
					PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, &scrutinee_occurrence
				) == 0) {
				printf(" scrutinee-occurrence#%u cases=%u", scrutinee_occurrence,
					operation->case_count);
			}
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			uint32_t argument_occurrence = PROTOTYPE_INVALID_ID;
			(void)prototype_typed_occurrence_graph_child(
				debug_occurrences, (uint32_t)i,
				PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
				&argument_occurrence
			);
			printf(
				" owner-match-occurrence#%u ih-scope#%u case=%u field=%u "
				"ast-binder#%u argument-occurrence#%u",
				operation->ih_owner_occurrence,
				operation->ih_scope_id,
				operation->ih_case_index,
				operation->ih_field_index,
				operation->referenced_ast_binder_id,
				argument_occurrence
			);
		}
		printf("\n");
	}
	for (size_t i = 0; i < metadata->typed_occurrences.case_count; ++i) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&metadata->typed_occurrences.cases[i];
		uint32_t body_occurrence = PROTOTYPE_INVALID_ID;
		for (uint32_t parent = 0;
			parent < metadata->typed_occurrences.occurrence_count;
			++parent) {
			const struct prototype_typed_occurrence* candidate =
				&metadata->typed_occurrences.occurrences[parent];
			if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				i >= candidate->first_case &&
				i < candidate->first_case + candidate->case_count) {
				(void)prototype_typed_occurrence_graph_child(
					debug_occurrences, parent,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					(uint32_t)i - candidate->first_case, &body_occurrence
				);
				break;
			}
		}
		printf("occurrence-case#%zu body-occurrence#%u owner#%u ordinal#%u",
			i,
			body_occurrence,
			operation_case->constructor_owner,
			operation_case->constructor_id);
		printf(" refinement-status=%d", operation_case->refinement_status);
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			printf(" refinement-substitution#%u",
				operation_case->refinement_substitution);
		}
		if (operation_case->case_label_symbol_id >= 0) {
			printf(" label=%s",
				symbol_to_string(symbols, operation_case->case_label_symbol_id));
		}
		printf("\n");
	}
	printf("\n#### Judgements ####\n");
	prototype_judgement_print(
		stdout, symbols, program->intrinsic_environment,
		type_declarations, term_db, judgement_db
	);
	memset(
		storage->reachable_external_refs,
		0,
		TERM_CAPACITY * sizeof(*storage->reachable_external_refs)
	);
	for (size_t i = 0; i < metadata->label_count; ++i) {
		mark_reachable_external_refs(
			term_db, storage->reachable_external_refs, metadata->labels[i].term, 0
		);
	}
	for (size_t i = 0; i < judgement_db->proposition_count; ++i) {
		mark_reachable_external_refs(
			term_db, storage->reachable_external_refs,
			judgement_db->propositions[i].subject, 0
		);
		mark_reachable_external_refs(
			term_db, storage->reachable_external_refs,
			judgement_db->propositions[i].classifier, 0
		);
	}
	size_t external_ref_count = 0;
	for (size_t i = 0; i < term_db->term_count; ++i) {
		if (storage->reachable_external_refs[i]) {
			external_ref_count++;
		}
	}
	printf(
		"\n"
		"#### Metadata ####\n"
		"labels=%zu resolve_errors=%zu external_refs=%zu self_contained=%s\n",
		metadata->label_count,
		metadata->resolve_error_count,
		external_ref_count,
		metadata->resolve_error_count == 0 && external_ref_count == 0 ? "yes" : "no"
	);
	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		printf("metadata label %s -> occurrence#%u -> term#%u\n",
			symbol_to_string(symbols, label->name_symbol_id),
			label->exposed_occurrence,
			label->term);
	}
	for (size_t i = 0; i < metadata->resolve_error_count; ++i) {
		const struct prototype_resolve_error* resolve_error =
			&metadata->resolve_errors[i];
		printf("metadata resolve-error kind=%s name=%s",
			prototype_diagnostic_resolve_error_kind_name(resolve_error->kind),
			symbol_to_string(symbols, resolve_error->name_symbol_id));
		if (resolve_error->member_symbol_id >= 0) {
			printf(".%s", symbol_to_string(symbols, resolve_error->member_symbol_id));
		}
		printf(
			" ast#%u span=%u:%u\n",
			resolve_error->ast,
			resolve_error->span.line,
			resolve_error->span.column
		);
	}
	for (size_t i = 0; i < term_db->term_count; ++i) {
		if (storage->reachable_external_refs[i]) {
			printf("metadata external-ref %s -> term#%zu\n",
					symbol_to_string(symbols, term_db->terms[i].as.external_ref.name.name_symbol_id),
			i);
		}
	}
	printf(
		"\n"
		"#### Artifact Interface ####\n"
		"term_exports=%zu type_exports=%zu constructor_exports=%zu dependencies=%zu\n",
		artifact_interface->term_export_count,
		artifact_interface->type_export_count,
		artifact_interface->constructor_export_count,
		artifact_interface->dependency_count
	);
	for (size_t i = 0; i < artifact_interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* type_export =
			&artifact_interface->type_exports[i];
		printf("interface type %s local_type#%u core_representation_anchor_type#%u constructors=%u representation_fingerprint=%llu\n",
			symbol_to_string(symbols, type_export->name_symbol_id),
			type_export->local_type_id,
			type_export->core_representation_anchor_type_id,
			type_export->constructor_count,
			(unsigned long long)type_export->representation_fingerprint.hash);
	}
	for (size_t i = 0; i < artifact_interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* constructor_export =
			&artifact_interface->constructor_exports[i];
		printf("interface constructor type_export#%u.%s ordinal=%u fields=%u constructor_classifier=%u\n",
			constructor_export->type_export_index,
			symbol_to_string(symbols, constructor_export->name_symbol_id),
			constructor_export->ordinal,
			constructor_export->readback_field_count,
			constructor_export->constructor_classifier);
	}
	printf("\n#### Resolution ####\n");
	prototype_diagnostic_print_resolution_trace(
		stdout, symbols, program->intrinsic_environment,
		type_declarations, term_db, metadata
	);
	printf("\n#### Universe ####\n");
	prototype_diagnostic_print_universe_graph(
		stdout, symbols, type_declarations, universe_db, 1
	);

	symbol_table_free(symbols);
	return 0;
}

static int run_reader_session(int argc, char** argv) {
	struct read_driver_storage storage;
	if (read_driver_storage_init(&storage) != 0) {
		fprintf(stderr, "failed to initialize reader storage\n");
		return 1;
	}
	int status = read_file_command(argc, argv, &storage);
	read_driver_storage_destroy(&storage);
	return status;
}

static int audit_reader_session_reentry(const char* program, const char* path) {
	char* failed_argv[] = {
		(char*)program,
		"--quiet",
		"/a-program-reader-session-audit/missing.p"
	};
	char* valid_argv[] = {(char*)program, "--quiet", (char*)path};
	if (run_reader_session(3, failed_argv) == 0 ||
		run_reader_session(3, valid_argv) != 0 ||
		run_reader_session(3, valid_argv) != 0) {
		return 1;
	}
	return 0;
}

int main(int argc, char** argv) {
	if (argc == 3 && strcmp(argv[1], "--audit-session-reentry") == 0) {
		return audit_reader_session_reentry(argv[0], argv[2]);
	}
	if (argc == 4 && strcmp(
			argv[1], "--audit-artifact-transaction-rejection"
		) == 0) {
		return audit_artifact_transaction_rejection(argv[2], argv[3]);
	}
	return run_reader_session(argc, argv);
}
