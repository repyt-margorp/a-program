#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-context-projection.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror kernel \
	"$tmp_dir/context-projection-check" \
	src/prototype/tests/checks/context_projection_check.c

"$tmp_dir/context-projection-check"

if rg -n 'context_projection|ContextProjection' \
		src/prototype/include/a_program/core src/prototype/src/core \
		>"$tmp_dir/core-projection-reference"; then
	cat "$tmp_dir/core-projection-reference" >&2
	echo 'Core calculation layer references Layer T Context projection state' >&2
	exit 1
fi

sed -n \
	'/struct prototype_typing_graph_build_context {/,/^};/p' \
	src/prototype/include/a_program/frontend/context_projection_builder.h \
	>"$tmp_dir/typing-graph-capability"
if rg -n 'prototype_term_db|judgement_db|constraint_db|reducer|normalization_cache' \
		"$tmp_dir/typing-graph-capability" \
		>"$tmp_dir/mixed-capability"; then
	cat "$tmp_dir/mixed-capability" >&2
	echo 'Layer T graph capability exposes Layer C or proof-solving authority' >&2
	exit 1
fi
if rg -n 'a_program/(core/|frontend/ast|graph/typed_occurrence_graph)|prototype_typed_occurrence_graph' \
		src/prototype/include/a_program/frontend/context_projection.h \
		src/prototype/src/frontend/context_projection.c \
		>"$tmp_dir/permanent-projection-builder-leak"; then
	cat "$tmp_dir/permanent-projection-builder-leak" >&2
	echo 'permanent Layer T Context projection depends on producer topology' >&2
	exit 1
fi
if rg -n 'prototype_typing_graph_build_rooted_projections\([^)]*compile_context' \
		src/prototype/src/frontend \
		>"$tmp_dir/broad-topology-api"; then
	cat "$tmp_dir/broad-topology-api" >&2
	echo 'Layer T topology builder accepts the broad frontend context' >&2
	exit 1
fi
