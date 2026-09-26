#!/bin/sh
set -eu

# Boundary audit: ISSUE-11-INDEXED-FAMILY-QUICKSORT

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader >/dev/null

fuel_free=src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
fuel_reference=src/prototype/tests/fixtures/typing/if8_fuel_quicksort_comparison.p
order=src/prototype/tests/fixtures/typing/if8_order_check.p

prototype_test_phase order_prerequisite
./read_file.out "$order" >"$tmp_dir/order.out"

prototype_test_phase source_equality
./read_file.out --check-source-exports-normalization-equal \
	main expected "$fuel_free" >"$tmp_dir/fuel-free.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/fuel-free.out"

./read_file.out --check-source-exports-normalization-equal \
	main expected "$fuel_reference" >"$tmp_dir/fuel-reference.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/fuel-reference.out"

prototype_test_phase negative_proofs
if sed 's/\*down lowerSize lowerBound lower/\*down lowerSize upperBound lower/' \
	"$fuel_free" >"$tmp_dir/swapped-bound.p" && \
	./read_file.out "$tmp_dir/swapped-bound.p" \
		>"$tmp_dir/swapped-bound.out" 2>"$tmp_dir/swapped-bound.err"
then
	echo 'QuickSort with a swapped decrease witness unexpectedly compiled' >&2
	exit 1
fi

if sed 's/\*down lowerSize lowerBound lower/\*down lowerSize lower/' \
	"$fuel_free" >"$tmp_dir/missing-bound.p" && \
	./read_file.out "$tmp_dir/missing-bound.p" \
		>"$tmp_dir/missing-bound.out" 2>"$tmp_dir/missing-bound.err"
then
	echo 'QuickSort without a decrease witness unexpectedly compiled' >&2
	exit 1
fi
grep -q 'diagnostic-code=classifier-equation-contradiction' \
	"$tmp_dir/missing-bound.err"

if sed \
	's/\*down lowerSize lowerBound lower/\*down lowerSize (LT.step (Nat.succ size)) lower/' \
	"$fuel_free" >"$tmp_dir/forged-bound.p" && \
	./read_file.out "$tmp_dir/forged-bound.p" \
		>"$tmp_dir/forged-bound.out" 2>"$tmp_dir/forged-bound.err"
then
	echo 'QuickSort with an unrelated constructed LT witness unexpectedly compiled' >&2
	exit 1
fi
grep -q 'diagnostic-code=classifier-equation-contradiction' \
	"$tmp_dir/forged-bound.err"

awk '
	/^Acc :=/ {
		print "unresolvedDecrease : (left : Nat) -> (right : Nat) -> LT left right;"
	}
	{ print }
' "$fuel_free" | sed \
	's/\*down lowerSize lowerBound lower/\*down lowerSize (unresolvedDecrease lowerSize (Nat.succ size)) lower/' \
	>"$tmp_dir/incompatible-external-bound.p"
if ./read_file.out "$tmp_dir/incompatible-external-bound.p" \
	>"$tmp_dir/incompatible-external-bound.out" \
	2>"$tmp_dir/incompatible-external-bound.err"
then
	echo 'QuickSort with incompatible external decrease evidence unexpectedly compiled' >&2
	exit 1
fi
# The external term has the requested surface classifier, so APP-domain
# checking is not the rejecting rule. It fails when the recursive Match motive
# cannot derive the structural decrease relation from that opaque term.
grep -q 'diagnostic-code=motive-equation-mismatch' \
	"$tmp_dir/incompatible-external-bound.err"

if grep -nE 'quickSort(Acc)?[^:]*:.*fuel|\\fuel[[:space:]]*:' "$fuel_free"
then
	echo 'fuel-free QuickSort unexpectedly exposes an algorithmic fuel argument' >&2
	exit 1
fi

prototype_test_phase publication
./read_file.out --write-artifact "$tmp_dir/if8-order.apo" "$order" \
	>"$tmp_dir/order-artifact.out"
./read_file.out --write-artifact "$tmp_dir/if8-quicksort.apo" "$fuel_free" \
	>"$tmp_dir/quicksort-artifact.out"
grep -Eq '^derivation [0-9]+ 54 claim [0-9]+ premises 2$' \
	"$tmp_dir/if8-quicksort.apo"

prototype_test_phase readback
./read_file.out --read-graph "$tmp_dir/if8-order.apo" \
	>"$tmp_dir/order-read.out"
./read_file.out --read-graph "$tmp_dir/if8-quicksort.apo" \
	>"$tmp_dir/quicksort-read.out"

# One dense arena retains source/replay cases as well as accepted executable
# Core. Source-only cases remain readable, but promoting one into the
# executable closure must fail because its constructor selection is unresolved.
source_only_case=$(awk '
	$1 == "match_case" && $4 == 4294967295 && $5 == 4294967295 {
		print $2
		exit
	}
' "$tmp_dir/if8-quicksort.apo")
test -n "$source_only_case"
if awk -v target="$source_only_case" '
	$1 == "executable_cases" {
		$2++
		in_cases = 1
		print
		next
	}
	in_cases && $1 != "executable_case" {
		print "executable_case " target
		in_cases = 0
	}
	{ print }
' "$tmp_dir/if8-quicksort.apo" >"$tmp_dir/forged-executable-case.apo" &&
	./read_file.out --read-graph "$tmp_dir/forged-executable-case.apo" \
		>"$tmp_dir/forged-executable-case.out" \
		2>"$tmp_dir/forged-executable-case.err"
then
	echo 'artifact promoted unresolved source provenance into executable Core' >&2
	exit 1
fi

# Direct accepted roots and transitive Core children must both remain in the
# declared executable closure.
main_term=$(awk '$1 == "term" && $2 == "main" { print $3; exit }' \
	"$tmp_dir/if8-quicksort.apo")
test -n "$main_term"
awk -v target="$main_term" '
	$1 == "executable_terms" { $2--; print; next }
	$1 == "executable_term" && $2 == target { removed = 1; next }
	{ print }
	END { if (!removed) exit 1 }
' "$tmp_dir/if8-quicksort.apo" >"$tmp_dir/missing-executable-root.apo"
if ./read_file.out --read-graph "$tmp_dir/missing-executable-root.apo" \
	>"$tmp_dir/missing-executable-root.out" \
	2>"$tmp_dir/missing-executable-root.err"
then
	echo 'artifact accepted an executable interface root outside its closure' >&2
	exit 1
fi

app_child=$(awk '$1 == "term_node" && $3 == 3 { print $4; exit }' \
	"$tmp_dir/if8-quicksort.apo")
test -n "$app_child"
awk -v target="$app_child" '
	$1 == "executable_terms" { $2--; print; next }
	$1 == "executable_term" && $2 == target { removed = 1; next }
	{ print }
	END { if (!removed) exit 1 }
' "$tmp_dir/if8-quicksort.apo" >"$tmp_dir/missing-executable-child.apo"
if ./read_file.out --read-graph "$tmp_dir/missing-executable-child.apo" \
	>"$tmp_dir/missing-executable-child.out" \
	2>"$tmp_dir/missing-executable-child.err"
then
	echo 'artifact accepted a non-closed executable Term graph' >&2
	exit 1
fi

prototype_test_phase artifact_equality
for pair in \
	"main expected" \
	"emptyMain emptyExpected" \
	"singletonMain singletonExpected" \
	"ascendingMain ascendingExpected" \
	"descendingMain descendingExpected" \
	"duplicateMain duplicateExpected"
do
	set -- $pair
	./read_file.out --check-exports-normalization-equal \
		"$tmp_dir/if8-quicksort.apo" "$1" "$2" \
		>"$tmp_dir/$1-artifact.out"
	grep -q "^exports-normalization-equal $1 $2 mode=default yes$" \
		"$tmp_dir/$1-artifact.out"
done
grep -q '^interface term quickSortTerminates ' "$tmp_dir/quicksort-read.out"

prototype_test_phase determinism
./read_file.out --write-artifact "$tmp_dir/if8-quicksort-repeat.apo" "$fuel_free" \
	>"$tmp_dir/quicksort-artifact-repeat.out"
cmp "$tmp_dir/if8-quicksort.apo" "$tmp_dir/if8-quicksort-repeat.apo"

prototype_test_phase authority_boundary
grep -q 'sequence_fold_occurrence' \
	src/prototype/src/frontend/lowering/context_and_type_lowering.inc
grep -q 'owner->sequence_fold_occurrence' \
	src/prototype/src/frontend/lowering/graph_construction.inc
if rg -n 'for \(uint32_t fold_id = 0;.*occurrence_count' \
	src/prototype/src/frontend/lowering/graph_construction.inc
then
	echo 'sequence-result refinement still scans every occurrence for its producer' >&2
	exit 1
fi

prototype_test_phase_finish
echo 'IF8 fuel-free QuickSort source and artifact tests passed'
