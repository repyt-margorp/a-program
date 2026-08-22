#!/bin/sh
set -eu

# Boundary audit: ISSUE-13-FUNCTION-GRAPH-RESULT-EVIDENCE

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader >/dev/null

positive=src/prototype/tests/fixtures/typing/function_graph_generated_length_check.p
model=src/prototype/tests/fixtures/typing/function_graph_certified_length_model.p

test -f "$model"

prototype_test_phase generated_graph
./read_file.out "$positive" >"$tmp_dir/positive.out"
grep -q '^term certifiedMain :=' "$tmp_dir/positive.out"
grep -q '^term main :=' "$tmp_dir/positive.out"

prototype_test_phase executable_projection
./read_file.out --check-source-exports-normalization-equal \
	main expected "$positive" >"$tmp_dir/equality.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/equality.out"

prototype_test_phase artifact_association
./read_file.out --write-artifact "$tmp_dir/function-graph.apo" "$positive" \
	>"$tmp_dir/artifact-write.out"
grep -q '^function_graph_associations 1$' "$tmp_dir/function-graph.apo"
grep -q '^function_graph_association 0 ' "$tmp_dir/function-graph.apo"
./read_file.out --read-graph "$tmp_dir/function-graph.apo" \
	>"$tmp_dir/artifact-read.out"
grep -q '^interface term \$certified\.length ' "$tmp_dir/artifact-read.out"

sed -E \
	's/^(function_graph_association [0-9]+ [0-9]+) [0-9]+ /\1 4294967295 /' \
	"$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-corrupt.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-corrupt.apo" \
	>"$tmp_dir/corrupt.out" 2>"$tmp_dir/corrupt.err"
then
	echo 'out-of-range function graph association unexpectedly read' >&2
	exit 1
fi

prototype_test_phase cbpv_negative
./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_computation_index.p \
	>"$tmp_dir/pure-index.out"
grep -q '^term bad :=' "$tmp_dir/pure-index.out"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_unknown_owner.p \
	>"$tmp_dir/unknown-owner.out" 2>"$tmp_dir/unknown-owner.err"
then
	echo 'function_graph_unknown_owner unexpectedly compiled' >&2
	exit 1
fi

prototype_test_phase static_boundary
if rg -n \
	'PROTOTYPE_(TERM|JUDGEMENT_PROOF)_FUNCTION_GRAPH|FUNCTION_GRAPH_(TYPE|WITNESS)_FORMER' \
	src/prototype/include/a_program/core \
	src/prototype/include/a_program/kernel \
	src/prototype/src/core \
	src/prototype/src/kernel
then
	echo 'function graph leaked into Core or kernel proof tags' >&2
	exit 1
fi

prototype_test_phase_finish
echo 'function graph certified execution tests passed'
