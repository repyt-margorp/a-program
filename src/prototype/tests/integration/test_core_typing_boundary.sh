#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-core-typing-boundary.XXXXXX")
trap 'status=$?; rm -rf "$tmp_dir"; exit "$status"' EXIT

# Layer C must not own typing, proof, Context, or artifact state.
if rg -n 'a_program/(frontend|graph|artifact|kernel/(context|judgement|type_declaration|universe))' \
		src/prototype/include/a_program/core src/prototype/src/core \
		>"$tmp_dir/core-layer-leak"; then
	cat "$tmp_dir/core-layer-leak" >&2
	echo 'Layer C depends on Layer T or publication state' >&2
	exit 1
fi

sed -n '/^struct prototype_term {/,/^struct prototype_computation_fold_clause {/p' \
	src/prototype/include/a_program/protocol/graph.h >"$tmp_dir/core-term"
if rg -n '(classifier|context_id|occurrence_id|proof_id|claim_id|derivation_id|constraint_id)[[:space:]]*;' \
		"$tmp_dir/core-term"; then
	echo 'Core Term contains Layer T authority' >&2
	exit 1
fi

# Layer T may retain opaque Core IDs but not a Core owner or reducer capability.
if rg -n 'a_program/core/(pipeline|term|read_provider|request_provider)\.h|struct prototype_(core_pipeline|term_db)' \
		src/prototype/include/a_program/frontend/typing_pipeline.h \
		>"$tmp_dir/typing-core-owner"; then
	cat "$tmp_dir/typing-core-owner" >&2
	echo 'Layer T owns a Layer C capability' >&2
	exit 1
fi

# Removed experimental routes must not return under another build registration.
if rg -n 'typing_agenda|prototype_t0_typing_topology|typing_(core_work|context_materialization|match_specialization)' \
		src/prototype/build src/prototype/include src/prototype/src \
		>"$tmp_dir/removed-route"; then
	cat "$tmp_dir/removed-route" >&2
	echo 'a removed alternate typing route is registered again' >&2
	exit 1
fi

prototype_compile c11 werror kernel \
	"$tmp_dir/core-calculation-layer-check" \
	src/prototype/tests/checks/core_calculation_layer_check.c
"$tmp_dir/core-calculation-layer-check"

prototype_compile c11 werror compiler \
	"$tmp_dir/source-epoch-check" \
	src/prototype/tests/checks/source_epoch_check.c
"$tmp_dir/source-epoch-check"

cc -std=c11 -Wall -Wextra -Werror \
	-I src/prototype/include -I src/prototype \
	src/prototype/tests/checks/typing_pipeline_header_check.c \
	-o "$tmp_dir/typing-pipeline-header-check"
"$tmp_dir/typing-pipeline-header-check"

echo 'core/typing two-layer boundary checks passed'
