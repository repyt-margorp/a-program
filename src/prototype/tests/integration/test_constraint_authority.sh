#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-constraint-authority.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

LOWERING_DIR=src/prototype/src/frontend/lowering
CONTEXT_SOURCE=$LOWERING_DIR/context_and_type_lowering.inc
SOLVER_SOURCE=$LOWERING_DIR/constraint_solver.inc
JUDGEMENT_TYPES=src/prototype/include/a_program/kernel/judgement/types.h

grep -q 'struct operation_constraint_db' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_COMPUTATION' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW' "$CONTEXT_SOURCE"
grep -q 'struct operation_constraint_db_mark' "$CONTEXT_SOURCE"
grep -q 'source_occurrence;' "$CONTEXT_SOURCE"
grep -q 'operation_constraint_transition(' "$SOLVER_SOURCE"
grep -q 'operation_constraint_reopen(' "$SOLVER_SOURCE"
grep -q 'operation_solver_relink_recorded_computation_constraints(' "$SOLVER_SOURCE"
grep -q 'computation_constraint_id;' "$JUDGEMENT_TYPES"

computation_payload=$(sed -n \
	'/struct prototype_judgement_computation_constraint {/,/^};/p' \
	"$JUDGEMENT_TYPES")
if printf '%s\n' "$computation_payload" | grep -q \
	'solved_classifier\|projected_classifier\|effect_residual'; then
	echo 'Judgement computation payload still owns mutable solver output' >&2
	exit 1
fi

if grep -R -q 'classifier_solver\.constraints\|classifier_solver\.constraint_results' \
	"$LOWERING_DIR"; then
	echo 'classifier constraints escaped the central ConstraintDB' >&2
	exit 1
fi
if grep -R -q 'effect_solver\.constraint_states' "$LOWERING_DIR"; then
	echo 'effect constraint lifecycle state is duplicated outside ConstraintDB' >&2
	exit 1
fi

validator_calls=$(grep -c 'operation_constraint_db_validate(ctx)' "$SOLVER_SOURCE")
test "$validator_calls" -ge 3
rollback_calls=$(grep -c 'operation_constraint_db_rollback(' "$SOLVER_SOURCE")
test "$rollback_calls" -ge 3

./read_file.out examples/type-infer-and-check/level0/01_function.p \
	>"$TMP_DIR/function.out" 2>"$TMP_DIR/function.err"
grep -q 'constraints=' "$TMP_DIR/function.out"

./read_file.out src/prototype/tests/fixtures/effects/effect_function_check.p \
	>"$TMP_DIR/effect.out" 2>"$TMP_DIR/effect.err"
grep -q 'constraints=' "$TMP_DIR/effect.out"

if grep -q 'constraint db .* invalid' "$TMP_DIR/function.err" ||
	grep -q 'constraint db .* invalid' "$TMP_DIR/effect.err"; then
	echo 'ConstraintDB validation failed for a permanent boundary fixture' >&2
	exit 1
fi

echo 'constraint authority checks passed'
