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
checked_one=src/prototype/tests/fixtures/typing/function_graph_dependent_output_ih_checked_core.p
checked_two=src/prototype/tests/fixtures/typing/function_graph_two_recursive_output_ih_checked_core.p

prototype_test_phase checked_core
prototype_compile c11 werror compiler \
	"$tmp_dir/checked-core" \
	src/prototype/tests/checks/checked_core_examples_check.c
"$tmp_dir/checked-core" "$checked_one" "$checked_two"

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

prototype_test_phase quicksort_two_recursive_ih
{
	sed -n '1,$p' \
		src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
	printf '%s\n' \
		'' \
		'emptyNatList := (List Nat).nil;' \
		'' \
		'QuickSortOutputTree := @\output : List Nat => {' \
		'nil : * emptyNatList;' \
		'split : (lowerResult : List Nat) -> (upperResult : List Nat) ->' \
		'(result : List Nat) ->' \
		'* lowerResult -> * upperResult -> * result;' \
		'};' \
		'' \
		'quickSortAccOutputTree := \le : Nat -> Nat -> Bool =>' \
		'\@le : (left : Nat) -> (right : Nat) -> Bool -> @ =>' \
		'\*le =>' \
		'\size : Nat => \access : Acc Nat LT size =>' \
		'\input : SizedList Nat size => \output : List Nat =>' \
		'\graph : @quickSortAcc Nat @le size access input output =>' \
		'graph' \
		'@nil current down => QuickSortOutputTree.nil' \
		'@cons tailSize pivot tail current down' \
		'lowerSize lower upperSize upper lowerBound upperBound' \
		'lowerResult lowerGraph upperResult upperGraph result appendGraph =>' \
		'QuickSortOutputTree.split lowerResult upperResult result' \
		'*lowerGraph *upperGraph;'
} >"$tmp_dir/quicksort-output-tree.p"
./read_file.out --write-artifact "$tmp_dir/quicksort.apo" \
	"$tmp_dir/quicksort-output-tree.p" \
	>"$tmp_dir/quicksort.out"
grep -q '^type QuickSortOutputTree constructors=2$' "$tmp_dir/quicksort.out"
grep -q '^term quickSortAccOutputTree :=' "$tmp_dir/quicksort.out"
./read_file.out --read-graph "$tmp_dir/quicksort.apo" \
	>"$tmp_dir/quicksort-read.out"
grep -q '^interface term quickSortAccOutputTree ' \
	"$tmp_dir/quicksort-read.out"

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
if rg -n \
	'^static .*operation_solver_(seed_motive|seed_indexed_motive|build_quoted_motive|materialize_match_solution|rebuild_refined_match_motives)|^static .*build_operation_(motive|uniform_motive|guarded_recursive_motive)' \
	src/prototype/src/frontend/lowering/graph_construction.inc \
	src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc
then
	echo 'motive solver implementation escaped its owner module' >&2
	exit 1
fi
if rg -n 'recursive_equation_owners\[4096\]' \
	src/prototype/src/frontend/lowering
then
	echo 'parallel recursive-equation owner authority remains' >&2
	exit 1
fi
grep -q 'ih_constraint_for_occurrence\[4096\]' \
	src/prototype/src/frontend/lowering/context_and_type_lowering.inc
grep -q 'activation_revision' \
	src/prototype/src/frontend/lowering/context_and_type_lowering.inc
grep -q 'operation_motive_validate_recursive_equations' \
	src/prototype/src/frontend/lowering/constraint/motive_solver.inc
grep -q 'operation_solver_seed_motive' \
	src/prototype/src/frontend/lowering/constraint/motive_solver.inc
grep -q 'operation_solver_materialize_match_solution' \
	src/prototype/src/frontend/lowering/constraint/motive_solver.inc

prototype_test_phase_finish
echo 'Issue 23 dependent motive tests passed'
