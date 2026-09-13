#!/usr/bin/env bash
set -eu
checker=$1
runtime=$2
fixtures="$(dirname "${BASH_SOURCE[0]}")/acceptance"
source="$fixtures/merge-function-graph-composition.p"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

# Independent declarations must not change the induction motive or admission.
awk '/^repeatWithFuel :=/ { skip=1 } !skip && !/^fuelMain :=/ { print } /^repeatWithFuel ::/ { skip=0 }' \
	"$source" > "$directory/alone.p"
awk '/^repeatWithFuel :=/ { take=1 } take { print } /^repeatWithFuel ::/ { take=0 }' \
	"$source" > "$directory/fuel.p"
awk -v fuel="$directory/fuel.p" '
	/^structuralMerge :=/ { while ((getline line < fuel) > 0) print line; close(fuel) }
	/^repeatWithFuel :=/ { skip=1 }
	!skip { print }
	/^repeatWithFuel ::/ { skip=0 }
' "$source" > "$directory/reordered.p"

for input in "$source" "$directory/alone.p" "$directory/reordered.p"; do
	"$checker" --steps 1000000 "$input"
	for name in main curriedMain; do
		"$runtime" --equal "$input" "$name" expected
	done
	"$runtime" --equal "$input" emptyMain emptyExpected
done
"$runtime" --equal "$source" fuelMain expected

# The posted error is invalid even without the independent fuel recursion.
wrong="$fixtures/merge-function-graph-wrong-application.p"
"$runtime" --reject "$wrong"
sed '/^repeatWithFuel :=/,$d' "$wrong" > "$directory/wrong-alone.p"
"$runtime" --reject "$directory/wrong-alone.p"

for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --save "$directory/merge.a" "$source" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in main curriedMain fuelMain; do
		"$runtime" --equal-image "$directory/merge.a" "$name" expected
	done
done
"$checker" --steps 100000 --imports "$source" --save "$directory/graph.a" \
	"$fixtures/merge-function-graph-request.p"
"$runtime" --equal-image "$directory/graph.a" main graphExpected
for input in "$source" "$directory/alone.p" "$directory/reordered.p"; do
	"$checker" --steps 100000 --imports "$input" --save "$directory/captured.a" \
		"$fixtures/merge-function-graph-captured-request.p"
	"$runtime" --equal-image "$directory/captured.a" main graphExpected
	"$runtime" --equal-image "$directory/captured.a" repeatMain repeatExpected
	"$runtime" --equal-image "$directory/captured.a" emptyMain emptyExpected
	"$runtime" --equal-image "$directory/captured.a" chosen one
	"$runtime" --equal-image "$directory/captured.a" laterMain one
	"$runtime" --equal-image "$directory/captured.a" dependentMain dependentExpected
	"$runtime" --equal-image "$directory/captured.a" mergedMain graphExpected
	"$runtime" --equal-image "$directory/captured.a" mergedZero emptyExpected
	"$runtime" --equal-image "$directory/captured.a" mergedTwice mergedTwiceExpected
done
# Indexed ambient generalization is checked independently of graph extraction.
indexed="$fixtures/indexed-captured-induction.p"
"$runtime" --equal "$indexed" main three
"$runtime" --equal "$indexed" emptyMain one
"$runtime" --equal "$indexed" selected one
for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --save "$directory/indexed.a" "$indexed" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"$runtime" --equal-image "$directory/indexed.a" main three
	"$runtime" --equal-image "$directory/indexed.a" emptyMain one
	"$runtime" --equal-image "$directory/indexed.a" selected one
done
for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --imports "$indexed" --save "$directory/indexed-graph.a" \
		"$fixtures/indexed-captured-graph-request.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"$runtime" --equal-image "$directory/indexed-graph.a" main expected
	"$runtime" --equal-image "$directory/indexed-graph.a" emptyMain emptyExpected
	"$runtime" --equal-image "$directory/indexed-graph.a" selected emptyExpected
	"$runtime" --equal-image "$directory/indexed-graph.a" copyMain copyExpected
	"$runtime" --equal-image "$directory/indexed-graph.a" copyEmpty empty
	"$runtime" --equal-image "$directory/indexed-graph.a" repeated five
	"$runtime" --equal-image "$directory/indexed-graph.a" pointMain emptyExpected
	"$runtime" --equal-image "$directory/indexed-graph.a" laterMain laterExpected
	"$runtime" --equal-image "$directory/indexed-graph.a" laterEmpty emptyExpected
done
code=0
"$checker" --steps 100000 --imports "$indexed" \
	"$fixtures/indexed-captured-graph-wrong-index.p" > "$directory/status" || code=$?
test "$code" -eq 1
code=0
"$checker" --steps 0 --imports "$indexed" --save "$directory/wrong-index.a" \
	"$fixtures/indexed-captured-graph-wrong-index.p" > "$directory/status" || code=$?
test "$code" -eq 3
code=0
"$checker" --load "$directory/wrong-index.a" --steps 100000 > "$directory/status" || code=$?
test "$code" -eq 1
printf '%s\n' 'merge composition: capture, currying, declaration order, result and image checks passed'
