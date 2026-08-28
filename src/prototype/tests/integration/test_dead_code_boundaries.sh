#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-dead-boundaries.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader a.out >/dev/null

SOURCE_ROOT=src/prototype/src
INCLUDE_ROOT=src/prototype/include
TEST_ROOT=src/prototype/tests

if rg -n '__attribute__[(][(]unused[)][)]' "$SOURCE_ROOT" "$INCLUDE_ROOT"; then
	echo 'implementation code suppresses an unused declaration' >&2
	exit 1
fi

cat >"$TMP_DIR/retired-symbols.txt" <<'EOF'
imported_type_export_by_local_type
external_type_spine_from_imported_instance
rewrite_imported_type_instances_to_external
prototype_read_options
prototype_read_ast_file_with_options
prototype_read_ast_string_with_options
prototype_artifact_interface_renumber_universe_vars
prototype_ast_compile_pending
prototype_ast_lookup_assignment_const
prototype_ast_pair_type_expectation
prototype_ast_type_expr_universe
prototype_checked_module_set_original_index_at
prototype_compile_metadata_function_graph_origin_group
prototype_compile_metadata_function_graph_request
prototype_cwf_certificate_db_validate_substitution_roots
prototype_dimension_operator_db_clear
prototype_runtime_evaluate_core
prototype_term_effect_row_forall_parts
prototype_term_host_type_source_name
prototype_term_host_type_term_tag
prototype_term_print
prototype_term_source_shape_equal_for_link
prototype_term_type_instance_source_make
prototype_type_declaration_intern_representation
prototype_typed_occurrence_graph_selected_classifier
prototype_universe_find_type_node
symbol_map_is_used_at
prototype_judgement_add_context_weakened_claim
prototype_judgement_add_induction_hypothesis_claim
prototype_judgement_add_is_type
prototype_judgement_classifier_conversion_with_definitions
prototype_judgement_delta_build_match_motive_from_branch_hints
prototype_judgement_delta_build_match_motive_from_known_branches
prototype_judgement_delta_expand_lambda
prototype_judgement_delta_expand_match_motive
prototype_judgement_delta_infer_cbpv_boundaries
prototype_judgement_delta_infer_computation_constraints
prototype_judgement_delta_record_int_literal_admissibility
prototype_judgement_delta_resolve_induction_hypothesis_request
prototype_judgement_delta_solve_computation_constraints
prototype_judgement_expand_app
prototype_judgement_expand_int_literal
prototype_judgement_expand_lambda
prototype_judgement_expand_match
prototype_judgement_expand_match_motive
prototype_judgement_expand_text_literal
prototype_judgement_record_declaration_fact
prototype_judgement_resolve_match_case_request
prototype_judgement_synthesize_match_pattern_classifier
collect_judgement_subject_classifiers
select_judgement_match_branch_classifier_for_motive
select_judgement_universe_classifier
EOF

while IFS= read -r symbol; do
	if rg -n -w "$symbol" "$SOURCE_ROOT" "$INCLUDE_ROOT"; then
		echo "retired implementation symbol returned: $symbol" >&2
		exit 1
	fi
done <"$TMP_DIR/retired-symbols.txt"

cat >"$TMP_DIR/retired-fixtures.txt" <<'EOF'
src/prototype/tests/fixtures/cbpv/execution_demand_check.p
src/prototype/tests/fixtures/cbpv/runtime_strict_dependent_check.p
src/prototype/tests/fixtures/typing/multi_app_occurrence_check.p
src/prototype/tests/fixtures/typing/function_graph_result_package_probe.p
src/prototype/tests/fixtures/typing/computation_indexed_family_check.p
EOF

while IFS= read -r fixture; do
	if [ -e "$fixture" ]; then
		echo "retired duplicate fixture returned: $fixture" >&2
		exit 1
	fi
done <"$TMP_DIR/retired-fixtures.txt"

make -s -f src/prototype/Makefile \
	print-reader-sources print-repl-sources print-hott-sources |
	sort -u >"$TMP_DIR/manifest-sources.txt"
find "$SOURCE_ROOT" -type f -name '*.c' | sort -u >"$TMP_DIR/all-sources.txt"
comm -23 "$TMP_DIR/all-sources.txt" "$TMP_DIR/manifest-sources.txt" \
	>"$TMP_DIR/unmanifested-sources.txt"
if [ -s "$TMP_DIR/unmanifested-sources.txt" ]; then
	echo 'implementation C source is absent from every build manifest:' >&2
	cat "$TMP_DIR/unmanifested-sources.txt" >&2
	exit 1
fi

find "$SOURCE_ROOT" -type f -name '*.inc' | sort -u >"$TMP_DIR/include-fragments.txt"
while IFS= read -r fragment; do
	base=$(basename "$fragment")
	if ! rg -q -F "$base\"" "$SOURCE_ROOT" --glob '*.c' --glob '*.inc'; then
		echo "implementation fragment has no include site: $fragment" >&2
		exit 1
	fi
done <"$TMP_DIR/include-fragments.txt"

nm -g --defined-only read_file.out a.out |
	awk '$2 ~ /^[TDBR]$/ && $3 ~ /^(prototype_|symbol_)/ { print $3 }' |
	sort -u >"$TMP_DIR/global-symbols.txt"

rg -n -o '(prototype|symbol)_[A-Za-z0-9_]+' \
	"$SOURCE_ROOT" "$INCLUDE_ROOT" "$TEST_ROOT" >"$TMP_DIR/symbol-tokens.txt"
awk -F: '
	{
		file = $1
		symbol = $NF
		key = symbol SUBSEP file
		if (!seen[key]++) {
			files[symbol]++
		}
	}
	END {
		for (symbol in files) {
			print symbol, files[symbol]
		}
	}
' "$TMP_DIR/symbol-tokens.txt" | sort >"$TMP_DIR/symbol-file-counts.txt"

# An allowlist entry must name the contract category. Keep this list empty
# until a wire, checker, or test entry genuinely cannot have a direct caller.
: >"$TMP_DIR/public-api-allowlist.txt"

while IFS= read -r symbol; do
	file_count=$(awk -v symbol="$symbol" \
		'$1 == symbol { print $2 }' "$TMP_DIR/symbol-file-counts.txt")
	file_count=${file_count:-0}
	if [ "$file_count" -le 2 ] &&
		! awk -v symbol="$symbol" '$1 == symbol { found = 1 } END { exit !found }' \
			"$TMP_DIR/public-api-allowlist.txt"; then
		echo "global function lacks a cross-module or direct-test contract: $symbol" >&2
		exit 1
	fi
done <"$TMP_DIR/global-symbols.txt"

echo 'dead code boundary checks passed'
