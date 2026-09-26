#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-classifier-rhs.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

${CC:-cc} -I src/prototype/include -I src/prototype \
	-std=c11 -Wall -Wextra -Werror \
	src/prototype/tests/checks/typing_classifier_rhs_check.c \
	src/prototype/src/frontend/typing_constraint_state.c \
	-o "$TMP_DIR/typing-classifier-rhs-check"
"$TMP_DIR/typing-classifier-rhs-check"

CONSTRAINT_TYPES=src/prototype/include/a_program/frontend/typing_constraint_state.h
LOWERING_DIR=src/prototype/src/frontend/lowering

grep -q 'classifier_rhs_root;' "$CONSTRAINT_TYPES"
grep -q 'prototype_typing_classifier_rhs_intern(' "$CONSTRAINT_TYPES"
if grep -R -q \
	'binder_classifier_expression_for_occurrence\|classifier_expressions' \
	src/prototype/include/a_program/frontend "$LOWERING_DIR"; then
	echo 'parallel classifier-expression authority returned' >&2
	exit 1
fi
if [ -e src/prototype/include/a_program/frontend/typing_classifier_expression.h ] ||
	[ -e src/prototype/src/frontend/typing_classifier_expression.c ]; then
	echo 'removed classifier-expression store returned' >&2
	exit 1
fi
if grep -R -q \
	'operation_solver_bind_projection\|operation_solver_replace_projection' \
	"$LOWERING_DIR"; then
	echo 'parallel classifier answer transition returned' >&2
	exit 1
fi
grep -q 'OPERATION_SOLVER_ANSWER_PUBLISH' \
	"$LOWERING_DIR/graph_construction.inc"

make -f src/prototype/Makefile reader >/dev/null
./read_file.out \
	src/prototype/tests/fixtures/typing/function_graph_dependent_output_ih_check.p \
	>"$TMP_DIR/dependent.out" 2>"$TMP_DIR/dependent.err"
grep -q '^typed-occurrences=' "$TMP_DIR/dependent.out"

echo 'classifier RHS authority checks passed'
