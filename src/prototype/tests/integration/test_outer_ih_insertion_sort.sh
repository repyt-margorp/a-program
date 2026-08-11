#!/bin/sh
set -eu

# Boundary audit: ISSUE-7-OUTER-IH-FRAME

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-outer-ih.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

OUTER_SOURCE=src/prototype/tests/fixtures/typing/outer_ih_nested_match_check.p
COMPARATOR_SOURCE=src/prototype/tests/fixtures/typing/recursive_comparator_check.p
INSERT_SOURCE=src/prototype/tests/fixtures/typing/eager_insertion_check.p
SORT_SOURCE=src/prototype/tests/fixtures/typing/insertion_sort_check.p
GROUPING_SOURCE=src/prototype/tests/fixtures/negative/outer_ih_missing_parentheses.p
GROUPING_WITHOUT_IH_SOURCE=src/prototype/tests/fixtures/negative/outer_match_missing_parentheses_without_ih.p

./read_file.out "$OUTER_SOURCE" >"$TMP_DIR/outer.out"
grep -q 'induction-hypothesis .* case=1 field=1 ' "$TMP_DIR/outer.out"
test "$(grep -c 'induction-hypothesis .* case=1 field=1 ' "$TMP_DIR/outer.out")" -eq 2

for pair in \
	'tailResult tailExpected' \
	'functionResult functionExpected'
do
	set -- $pair
	./read_file.out --check-source-exports-normalization-equal \
		"$1" "$2" --reduction-mode default "$OUTER_SOURCE" \
		>"$TMP_DIR/$1.out"
	grep -q 'mode=default yes$' "$TMP_DIR/$1.out"
done

if ./read_file.out "$GROUPING_SOURCE" \
	>"$TMP_DIR/grouping.out" 2>"$TMP_DIR/grouping.err"; then
	echo 'unparenthesized nested Match unexpectedly compiled' >&2
	exit 1
fi
grep -q 'nested elimination body requires parentheses' "$TMP_DIR/grouping.err"
grep -Eq 'read-diagnostic diagnostic-code=nested-match-grouping span=[1-9][0-9]*:[1-9][0-9]*' \
	"$TMP_DIR/grouping.err"

if ./read_file.out "$GROUPING_WITHOUT_IH_SOURCE" \
	>"$TMP_DIR/grouping-without-ih.out" 2>"$TMP_DIR/grouping-without-ih.err"; then
	echo 'unparenthesized nested Match without IH unexpectedly compiled' >&2
	exit 1
fi
grep -Eq 'compile-diagnostic diagnostic-code=nested-match-grouping .*span=[1-9][0-9]*:[1-9][0-9]*' \
	"$TMP_DIR/grouping-without-ih.err"

./read_file.out "$COMPARATOR_SOURCE" >"$TMP_DIR/comparator.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/comparator.out"
for pair in \
	'lessResult lessExpected' \
	'greaterResult greaterExpected'
do
	set -- $pair
	./read_file.out --check-source-exports-normalization-equal \
		"$1" "$2" --reduction-mode default "$COMPARATOR_SOURCE" \
		>"$TMP_DIR/comparator-$1.out"
	grep -q 'mode=default yes$' "$TMP_DIR/comparator-$1.out"
done

./read_file.out "$INSERT_SOURCE" >"$TMP_DIR/insertion.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/insertion.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/insertion.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected --reduction-mode default "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/insertion-equal.out"
./read_file.out --check-source-exports-normalization-equal \
	earlyMain earlyExpected --reduction-mode default "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-early-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/insertion-early-equal.out"
./read_file.out --check-source-exports-normalization-equal \
	traceEarly traceEarlyExpected --reduction-mode default "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-trace-early-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/insertion-trace-early-equal.out"
./read_file.out --check-source-exports-normalization-equal \
	traceRecursive traceRecursiveExpected --reduction-mode default "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-trace-recursive-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/insertion-trace-recursive-equal.out"
./read_file.out --trace-source-export-evaluation traceEarly "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-early-trace.out"
grep -q '^evaluation-trace export=traceEarly induction-hypothesis-reductions=0$' \
	"$TMP_DIR/insertion-early-trace.out"
./read_file.out --trace-source-export-evaluation traceRecursive "$INSERT_SOURCE" \
	>"$TMP_DIR/insertion-recursive-trace.out"
grep -Eq '^evaluation-trace export=traceRecursive induction-hypothesis-reductions=[1-9][0-9]*$' \
	"$TMP_DIR/insertion-recursive-trace.out"

./read_file.out "$SORT_SOURCE" >"$TMP_DIR/sort.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/sort.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/sort.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected --reduction-mode default "$SORT_SOURCE" \
	>"$TMP_DIR/sort-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/sort-equal.out"

./read_file.out --write-artifact "$TMP_DIR/sort.apo" "$SORT_SOURCE" \
	>"$TMP_DIR/sort-write.out"
./read_file.out --read-graph "$TMP_DIR/sort.apo" >"$TMP_DIR/sort-read.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/sort.apo" \
	main expected --reduction-mode default >"$TMP_DIR/sort-artifact-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/sort-artifact-equal.out"
grep -Eq '^payload induction [0-9]+ [0-9]+ 1 1$' "$TMP_DIR/sort.apo"

echo 'outer IH and insertion sort tests passed'
