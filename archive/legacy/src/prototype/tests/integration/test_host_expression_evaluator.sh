#!/bin/sh
set -eu

# Boundary audit: ISSUE-10-INTRINSIC-INT-PRINCIPAL

# Boundary audit: ISSUE-4-HOST-LITERAL-RECURSIVE-MOTIVE

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-host-expression.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

EXPR_SOURCE=src/prototype/tests/fixtures/typing/host_expression_evaluator_check.p
TEXT_SOURCE=src/prototype/tests/fixtures/typing/host_text_recursive_motive_check.p

./read_file.out "$EXPR_SOURCE" >"$TMP_DIR/expression.out"
grep -q 'induction-hypothesis .* case=1 field=0 ' "$TMP_DIR/expression.out"
grep -q 'induction-hypothesis .* case=1 field=1 ' "$TMP_DIR/expression.out"
grep -q 'induction-hypothesis .* case=2 field=0 ' "$TMP_DIR/expression.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/expression.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/expression.out"
grep -q '\[pure-primitive-type-intro proof#' "$TMP_DIR/expression.out"
grep -q 'has-type INT_LITERAL(42) PRIMITIVE(Int) \[int-literal-intro proof#' \
	"$TMP_DIR/expression.out"
grep -q '^term int32Box :=' "$TMP_DIR/expression.out"
grep -q '^term boxInt64 :=' "$TMP_DIR/expression.out"

printf 'main\n:q\n' | ./a.out "$EXPR_SOURCE" \
	>"$TMP_DIR/expression-runtime.out"
grep -q '^value main := RETURN(INT_LITERAL(42))$' \
	"$TMP_DIR/expression-runtime.out"

./read_file.out --write-artifact "$TMP_DIR/expression.apo" "$EXPR_SOURCE" \
	>"$TMP_DIR/expression-write.out"
./read_file.out --read-graph "$TMP_DIR/expression.apo" \
	>"$TMP_DIR/expression-read.out"
grep -Eq '^derivations [1-9][0-9]*$' "$TMP_DIR/expression.apo"
./read_file.out --aggregate-artifact "$TMP_DIR/expression-linked.apo" \
	"$TMP_DIR/expression.apo" >"$TMP_DIR/expression-link.out"
./read_file.out --read-graph "$TMP_DIR/expression-linked.apo" \
	>"$TMP_DIR/expression-linked-read.out"

./read_file.out "$TEXT_SOURCE" >"$TMP_DIR/text.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/text.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/text.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected --reduction-mode default "$TEXT_SOURCE" \
	>"$TMP_DIR/text-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/text-equal.out"

OUT_OF_RANGE_SOURCE=src/prototype/tests/fixtures/negative/int_literal_above_default_range.p
if ./read_file.out "$OUT_OF_RANGE_SOURCE" \
	>"$TMP_DIR/out-of-range.out" 2>"$TMP_DIR/out-of-range.err"; then
	echo 'out-of-range configured Int literal was accepted' >&2
	exit 1
fi
grep -q 'diagnostic-code=integer-literal-range' "$TMP_DIR/out-of-range.err"
grep -Eq 'span=1:[1-9][0-9]*' "$TMP_DIR/out-of-range.err"

BELOW_RANGE_SOURCE=src/prototype/tests/fixtures/negative/int_literal_below_default_range.p
if ./read_file.out "$BELOW_RANGE_SOURCE" \
	>"$TMP_DIR/below-range.out" 2>"$TMP_DIR/below-range.err"; then
	echo 'literal below the configured Int range was accepted' >&2
	exit 1
fi
grep -q 'diagnostic-code=integer-literal-range' "$TMP_DIR/below-range.err"
grep -Eq 'span=1:[1-9][0-9]*' "$TMP_DIR/below-range.err"

INT64_EXPECTED_SOURCE=src/prototype/tests/fixtures/negative/int64_constructor_unsuffixed_literal.p
if ./read_file.out "$INT64_EXPECTED_SOURCE" \
	>"$TMP_DIR/int64-expected.out" 2>"$TMP_DIR/int64-expected.err"; then
	echo 'an Int64 field contextually changed an unsuffixed literal principal' >&2
	exit 1
fi
grep -Eq 'diagnostic-code=(constructor-domain-mismatch|unsolved-classifier)' \
	"$TMP_DIR/int64-expected.err"
grep -Eq 'span=5:[1-9][0-9]*' "$TMP_DIR/int64-expected.err"

INT64_ASCRIPTION_SOURCE=src/prototype/tests/fixtures/negative/int64_ascription_unsuffixed_literal.p
if ./read_file.out "$INT64_ASCRIPTION_SOURCE" \
	>"$TMP_DIR/int64-ascription.out" 2>"$TMP_DIR/int64-ascription.err"; then
	echo 'an Int64 ascription changed an unsuffixed literal principal' >&2
	exit 1
fi
grep -q 'diagnostic-code=ascription-mismatch' "$TMP_DIR/int64-ascription.err"
grep -Eq 'span=1:[1-9][0-9]*' "$TMP_DIR/int64-ascription.err"

echo 'host expression evaluator tests passed'
