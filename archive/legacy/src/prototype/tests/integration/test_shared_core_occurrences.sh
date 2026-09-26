#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/shared-id.p" <<'EOF'
identityInt := \x : #.Int => x;
identityText := \y : #.Text => y;
intResult := identityInt #1;
textResult := identityText #"x";
EOF

./read_file.out --write-artifact "$tmp_dir/shared-id.apo" \
	"$tmp_dir/shared-id.p" >"$tmp_dir/output"

int_term=$(sed -n 's/^metadata label identityInt -> occurrence#[0-9][0-9]* -> term#\([0-9][0-9]*\)$/\1/p' "$tmp_dir/output")
text_term=$(sed -n 's/^metadata label identityText -> occurrence#[0-9][0-9]* -> term#\([0-9][0-9]*\)$/\1/p' "$tmp_dir/output")

test -n "$int_term"
test "$int_term" = "$text_term"
grep -q 'has-type LAMBDA(_#[0-9][0-9]*, RETURN(VAR(_#[0-9][0-9]*))) PI(PRIMITIVE(Int),' "$tmp_dir/output"
grep -q 'has-type LAMBDA(_#[0-9][0-9]*, RETURN(VAR(_#[0-9][0-9]*))) PI(PRIMITIVE(Text),' "$tmp_dir/output"
grep -q 'metadata label intResult ' "$tmp_dir/output"
grep -q 'metadata label textResult ' "$tmp_dir/output"

int_occurrence=$(sed -n 's/^metadata label identityInt -> occurrence#\([0-9][0-9]*\) -> term#[0-9][0-9]*$/\1/p' \
	"$tmp_dir/output")
text_occurrence=$(sed -n 's/^metadata label identityText -> occurrence#\([0-9][0-9]*\) -> term#[0-9][0-9]*$/\1/p' \
	"$tmp_dir/output")
test -n "$int_occurrence"
test -n "$text_occurrence"
test "$int_occurrence" != "$text_occurrence"

int_core=$(awk -v occurrence="$int_occurrence" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $6 }' \
	"$tmp_dir/shared-id.apo")
text_core=$(awk -v occurrence="$text_occurrence" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $6 }' \
	"$tmp_dir/shared-id.apo")
int_classifier=$(awk -v occurrence="$int_occurrence" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $8 }' \
	"$tmp_dir/shared-id.apo")
text_classifier=$(awk -v occurrence="$text_occurrence" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $8 }' \
	"$tmp_dir/shared-id.apo")
test "$int_core" = "$text_core"
test "$int_classifier" != "$text_classifier"

int_body=$(awk -v parent="$int_occurrence" \
	'$1 == "occurrence_edge" && $3 == parent { print $6; exit }' \
	"$tmp_dir/shared-id.apo")
text_body=$(awk -v parent="$text_occurrence" \
	'$1 == "occurrence_edge" && $3 == parent { print $6; exit }' \
	"$tmp_dir/shared-id.apo")
int_context=$(awk -v occurrence="$int_body" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $25 }' \
	"$tmp_dir/shared-id.apo")
text_context=$(awk -v occurrence="$text_body" \
	'$1 == "typed_occurrence" && $2 == occurrence { print $25 }' \
	"$tmp_dir/shared-id.apo")
test -n "$int_context"
test -n "$text_context"
test "$int_context" != "$text_context"

int_claim=$(awk '$1 == "term" && $2 == "identityInt" {
	for (i = 1; i <= NF; ++i) if ($i == "evidence" && $(i + 1) == 1) print $(i + 2)
}' "$tmp_dir/shared-id.apo")
text_claim=$(awk '$1 == "term" && $2 == "identityText" {
	for (i = 1; i <= NF; ++i) if ($i == "evidence" && $(i + 1) == 1) print $(i + 2)
}' "$tmp_dir/shared-id.apo")
test -n "$int_claim"
test -n "$text_claim"
test "$int_claim" != "$text_claim"
int_derivation=$(awk -v claim="$int_claim" \
	'$1 == "derivation" && $4 == "claim" && $5 == claim { print $2 }' \
	"$tmp_dir/shared-id.apo")
text_derivation=$(awk -v claim="$text_claim" \
	'$1 == "derivation" && $4 == "claim" && $5 == claim { print $2 }' \
	"$tmp_dir/shared-id.apo")
test -n "$int_derivation"
test -n "$text_derivation"
test "$int_derivation" != "$text_derivation"

if ./read_file.out \
		src/prototype/tests/fixtures/negative/source_intrinsic_nat_nominal_mismatch.p \
		>"$tmp_dir/source-intrinsic-nat-negative.out" \
		2>"$tmp_dir/source-intrinsic-nat-negative.err"; then
	echo 'intrinsic Nat was nominally converted to source Nat' >&2
	exit 1
fi
grep -q '^expectation mismatch ' \
	"$tmp_dir/source-intrinsic-nat-negative.err"

./read_file.out training/list_nat_match.p >"$tmp_dir/list-match-output"
grep -q 'metadata label main ' "$tmp_dir/list-match-output"
grep -q 'occurrence#[0-9][0-9]* match ' "$tmp_dir/list-match-output"

./read_file.out training/recursive_dependent_match.p \
	>"$tmp_dir/recursive-dependent-match-output"
grep -q 'metadata label fold ' "$tmp_dir/recursive-dependent-match-output"
grep -q 'occurrence#[0-9][0-9]* induction-hypothesis ' \
	"$tmp_dir/recursive-dependent-match-output"

if rg -n 'compile_prior_value_ref|collect_graph_classifiers' \
		src/prototype/src/frontend/lowering \
		>"$tmp_dir/core-to-occurrence-recovery"; then
	cat "$tmp_dir/core-to-occurrence-recovery" >&2
	echo 'lowering recovers Layer T identity from a Core Term' >&2
	exit 1
fi

grep -q 'view.root_occurrence = label->exposed_occurrence;' \
	src/prototype/src/frontend/function_graph.c
if rg -n 'compiled_(term|classifier|operation|type)|[[:space:]]compiled;|[[:space:]]compiling;|[[:space:]]published;' \
		src/prototype/include/a_program/frontend/ast.h; then
	echo 'source AST retains a compiled Layer C or Layer T result' >&2
	exit 1
fi
grep -q 'prototype_typed_occurrence_graph_child' \
	src/prototype/src/graph/typed_occurrence/graph_validation.inc
