#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-shared-term-hott.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

app_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_APP([[:space:]]*=|,)' src/prototype/include/a_program/core/term.h)
lambda_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_LAMBDA([[:space:]]*=|,)' src/prototype/include/a_program/core/term.h)
pi_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_PI([[:space:]]*=|,)' src/prototype/include/a_program/core/term.h)
match_tags=$(grep -Ec '^[[:space:]]*PROTOTYPE_TERM_MATCH([[:space:]]*=|,)' src/prototype/include/a_program/core/term.h)

test "$app_tags" -eq 1
test "$lambda_tags" -eq 1
test "$pi_tags" -eq 1
test "$match_tags" -eq 1

if grep -Eq 'PROTOTYPE_TERM_(VALUE|COMPUTATION)_(APP|LAMBDA|PI|MATCH)' \
		src/prototype/include/a_program/core/term.h; then
	echo "duplicated value/computation graph tag found" >&2
	exit 1
fi

if grep -Eq '^[[:space:]]*PROTOTYPE_TERM_(OBS_EQ|EQUALITY|PATH|TRANSPORT|COHERENCE)' \
		src/prototype/include/a_program/core/term.h; then
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

prototype_compile c11 warnings graph \
	"$tmp_dir/shared-term-reindex-check" \
	src/prototype/tests/checks/shared_term_reindex_check.c
prototype_compile c11 werror compiler \
	"$tmp_dir/alpha-slot-env-check" \
	src/prototype/tests/checks/alpha_slot_env_check.c

"$tmp_dir/shared-term-reindex-check"
"$tmp_dir/alpha-slot-env-check"
