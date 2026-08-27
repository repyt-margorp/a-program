#!/bin/sh
set -eu

# Boundary audit: ISSUE-23-DEPENDENT-MOTIVE

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader >/dev/null

dependent=src/prototype/tests/fixtures/typing/function_graph_dependent_output_ih_check.p
two_recursive=src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p
nominal=src/prototype/tests/fixtures/typing/function_graph_nominal_index_constant_motive_check.p
indexed=src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p
incompatible=src/prototype/tests/fixtures/negative/function_graph_incompatible_recursive_property.p
unguarded=src/prototype/tests/fixtures/negative/nonrecursive_induction_hypothesis.p
unknown_owner=src/prototype/tests/fixtures/negative/function_graph_unknown_owner.p

prototype_test_phase generated_dependent_motive
./read_file.out --write-artifact "$tmp_dir/dependent.apo" "$dependent" \
	>"$tmp_dir/dependent.out"
grep -q '^term lengthOutputUnary :=' "$tmp_dir/dependent.out"
./read_file.out --read-graph "$tmp_dir/dependent.apo" \
	>"$tmp_dir/dependent-read.out"
grep -q '^interface term lengthOutputUnary ' "$tmp_dir/dependent-read.out"
./read_file.out --write-artifact "$tmp_dir/two-recursive.apo" "$two_recursive" \
	>"$tmp_dir/two-recursive.out"
grep -q '^term mirrorOutputTree :=' "$tmp_dir/two-recursive.out"
grep -q '^term certifiedOutputTree :=' "$tmp_dir/two-recursive.out"
./read_file.out --read-graph "$tmp_dir/two-recursive.apo" \
	>"$tmp_dir/two-recursive-read.out"
grep -q '^interface term certifiedOutputTree ' \
	"$tmp_dir/two-recursive-read.out"

prototype_test_phase indexed_controls
./read_file.out "$nominal" >"$tmp_dir/nominal.out"
./read_file.out "$indexed" >"$tmp_dir/indexed.out"
grep -q '^term constantNatUnary :=' "$tmp_dir/nominal.out"
grep -q '^term certified :=' "$tmp_dir/indexed.out"

prototype_test_phase recursive_rejections
if ./read_file.out "$incompatible" >"$tmp_dir/incompatible.out" \
	2>"$tmp_dir/incompatible.err"
then
	echo 'incompatible recursive motive unexpectedly compiled' >&2
	exit 1
fi
grep -q 'diagnostic-code=motive-equation-mismatch' \
	"$tmp_dir/incompatible.err"
if grep -q 'P0 accepted proof graph validation failed' "$tmp_dir/incompatible.err"
then
	echo 'incompatible recursive motive reached accepted replay' >&2
	exit 1
fi

for fixture in "$unguarded" "$unknown_owner"
do
	if ./read_file.out "$fixture" >"$tmp_dir/rejected.out" \
		2>"$tmp_dir/rejected.err"
	then
		echo "recursive ownership control unexpectedly compiled: $fixture" >&2
		exit 1
	fi
done

prototype_test_phase authority_boundary
if rg -n 'recursive_equation_operation|A_PROGRAM_MOTIVE_TRACE' \
	src/prototype/src/frontend/lowering
then
	echo 'obsolete or debug motive authority remains' >&2
	exit 1
fi
grep -q 'recursive_equation_owners\[4096\]' \
	src/prototype/src/frontend/lowering/context_and_type_lowering.inc
grep -q 'operation_motive_validate_recursive_equations' \
	src/prototype/src/frontend/lowering/constraint/motive_solver.inc

prototype_test_phase_finish
echo 'Issue 23 dependent motive tests passed'
