#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-shared-term-hott.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

term_tag_authority=src/prototype/include/a_program/protocol/graph.h
app_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_APP([[:space:]]*=|,)' "$term_tag_authority")
lambda_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_LAMBDA([[:space:]]*=|,)' "$term_tag_authority")
pi_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_PI([[:space:]]*=|,)' "$term_tag_authority")
match_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_MATCH([[:space:]]*=|,)' "$term_tag_authority")

test "$app_tags" -eq 1
test "$lambda_tags" -eq 1
test "$pi_tags" -eq 1
test "$match_tags" -eq 1

if grep -Eq 'PROTOTYPE_TERM_(VALUE|COMPUTATION)_(APP|LAMBDA|PI|MATCH)' \
		"$term_tag_authority"; then
	echo "duplicated value/computation graph tag found" >&2
	exit 1
fi

if rg -n 'effect_operation\.classifier|TERM_CHILD_EFFECT_OPERATION_CLASSIFIER' \
		src/prototype/include/a_program/core \
		src/prototype/src/core \
		src/prototype/src/artifact \
		src/prototype/src/checker; then
	echo "effect operation classifier leaked into the Core calculation graph" >&2
	exit 1
fi

if rg -n 'prototype_(term_classifier_view|term_category|term_computation_kind)|PROTOTYPE_TERM_(CATEGORY|COMPUTATION_KIND)_' \
		src/prototype/include/a_program/core \
		src/prototype/src/core; then
	echo "Layer T classifier vocabulary leaked into Core" >&2
	exit 1
fi

test -f src/prototype/include/a_program/kernel/classifier.h

if ! grep -q '^EFFECT_OPERATION serializes only its intrinsic operation name\.' \
		src/prototype/spec/artifact_v90.schema; then
	echo "artifact schema does not enforce the Core operation boundary" >&2
	exit 1
fi

if grep -Eq '^[[:space:]]*PROTOTYPE_TERM_(OBS_EQ|EQUALITY|PATH|TRANSPORT|COHERENCE)' \
		"$term_tag_authority"; then
	echo "premature object-equality graph tag found" >&2
	exit 1
fi

for graph_api in substitute_bound_var replace_exact reindex_bindings; do
	if sed -n "/int prototype_term_graph_${graph_api}(/,/);/p" \
			src/prototype/include/a_program/core/term.h | \
		grep -q 'prototype_type_declaration_db'; then
		echo "Core graph API exposes aggregate TypeDeclarationDB: $graph_api" >&2
		exit 1
	fi
done

for calculation_api in \
	normalize_complete_with_profile \
	normalize_with_profile \
	normalization_machine_create \
	project_structural_return_value \
	nf_with_options \
	perform_with_options \
	compare_with_options \
	compare_for_conversion; do
	if sed -n "/int prototype_term_${calculation_api}(/,/);/p" \
			src/prototype/include/a_program/core/term.h | \
		grep -q 'prototype_type_declaration_db'; then
		echo "Core calculation API exposes TypeDeclarationDB: $calculation_api" >&2
		exit 1
	fi
done

if sed -n '/typedef int (\*prototype_term_operation_dispatch_fn)(/,/);/p' \
		src/prototype/include/a_program/core/term.h | \
	grep -q 'prototype_type_declaration_db'; then
	echo "Core operation callback exposes TypeDeclarationDB" >&2
	exit 1
fi

prototype_compile c11 warnings graph \
	"$tmp_dir/shared-term-reindex-check" \
	src/prototype/tests/checks/shared_term_reindex_check.c
prototype_compile c11 werror compiler \
	"$tmp_dir/alpha-slot-env-check" \
	src/prototype/tests/checks/alpha_slot_env_check.c

"$tmp_dir/shared-term-reindex-check"
"$tmp_dir/alpha-slot-env-check"
