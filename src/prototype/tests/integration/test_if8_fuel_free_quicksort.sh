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
grep -q 'diagnostic-code=unsolved-classifier' "$tmp_dir/missing-bound.err"

if sed \
	's/\*down lowerSize lowerBound lower/\*down lowerSize (LT.step (Nat.succ size)) lower/' \
	"$fuel_free" >"$tmp_dir/forged-bound.p" && \
	./read_file.out "$tmp_dir/forged-bound.p" \
		>"$tmp_dir/forged-bound.out" 2>"$tmp_dir/forged-bound.err"
then
	echo 'QuickSort with an unrelated constructed LT witness unexpectedly compiled' >&2
	exit 1
fi
grep -q 'diagnostic-code=unsolved-classifier' "$tmp_dir/forged-bound.err"

awk '
	/^Acc :=/ {
		print "unresolvedDecrease : (left : Nat) -> (right : Nat) -> LT left right;"
	}
	{ print }
' "$fuel_free" | sed \
	's/\*down lowerSize lowerBound lower/\*down lowerSize (unresolvedDecrease lowerSize (Nat.succ size)) lower/' \
	>"$tmp_dir/residual-bound.p"
if ./read_file.out "$tmp_dir/residual-bound.p" \
	>"$tmp_dir/residual-bound.out" 2>"$tmp_dir/residual-bound.err"
then
	echo 'QuickSort with unresolved external decrease evidence unexpectedly compiled' >&2
	exit 1
fi
grep -q 'diagnostic-code=unsolved-classifier' "$tmp_dir/residual-bound.err"

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

prototype_test_phase_finish
echo 'IF8 fuel-free QuickSort source and artifact tests passed'
