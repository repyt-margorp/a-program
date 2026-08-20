#!/bin/sh
set -eu

# Boundary audit: ISSUE-18-MUTABLE-SOLVER-AUTHORITY

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-constraint-authority.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

LOWERING_DIR=src/prototype/src/frontend/lowering
CONTEXT_SOURCE=$LOWERING_DIR/context_and_type_lowering.inc
SOLVER_ROOT=$LOWERING_DIR/constraint_solver.inc
SOLVER_MODEL=$LOWERING_DIR/constraint/model_generation_and_index.inc
SOLVER_EFFECT=$LOWERING_DIR/constraint/effect_propagation_and_residuals.inc
SOLVER_SOURCE=$LOWERING_DIR/constraint
JUDGEMENT_TYPES=src/prototype/include/a_program/kernel/judgement/types.h
READ_FILE=src/prototype/src/driver/read_file.c

grep -q 'struct operation_constraint_db' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_COMPUTATION' "$CONTEXT_SOURCE"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW' "$CONTEXT_SOURCE"
grep -q 'struct operation_constraint_db_mark' "$CONTEXT_SOURCE"
grep -q 'source_occurrence;' "$CONTEXT_SOURCE"
grep -q 'operation_constraint_transition(' "$SOLVER_MODEL"
grep -q 'operation_constraint_reopen(' "$SOLVER_MODEL"
grep -R -q 'operation_solver_refresh_computation_constraint_operands(' \
	"$SOLVER_SOURCE"
grep -q 'computation_constraint_for_occurrence' "$CONTEXT_SOURCE"
grep -q 'computation_constraint_by_occurrence' \
	src/prototype/include/a_program/kernel/judgement/db.h
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

solution_cell=$(sed -n \
	'/struct operation_solver_solution {/,/^};/p' \
	"$CONTEXT_SOURCE")
if printf '%s\n' "$solution_cell" | grep -q \
	'classifier_state\|effect_state\|reason'; then
	echo 'solution cell still duplicates ConstraintDB lifecycle state' >&2
	exit 1
fi

constraint_record=$(sed -n \
	'/struct operation_constraint {/,/^};/p' \
	"$CONTEXT_SOURCE")
if printf '%s\n' "$constraint_record" | grep -q 'result_term'; then
	echo 'ConstraintDB still copies a domain result Term' >&2
	exit 1
fi

residual_effect_body=$(sed -n \
	'/static int compile_phase_record_residual_effect_constraints(/,/^}/p' \
	"$SOLVER_EFFECT")
if printf '%s\n' "$residual_effect_body" | grep -q \
	'metadata->effect_constraints'; then
	echo 'verification obligations still read the diagnostic effect snapshot' >&2
	exit 1
fi
artifact_export_check=$(sed -n \
	'/static int artifact_exports_have_accepted_claims(/,/^}/p' \
	"$READ_FILE")
if printf '%s\n' "$artifact_export_check" | grep -q \
	'effect_constraints'; then
	echo 'artifact acceptance still falls back to a diagnostic effect snapshot' >&2
	exit 1
fi

validator_calls=$(grep -R -h 'operation_constraint_db_validate(ctx)' \
	"$SOLVER_SOURCE" | wc -l)
test "$validator_calls" -ge 3
rollback_calls=$(grep -R -h 'operation_constraint_db_rollback(' \
	"$SOLVER_SOURCE" | wc -l)
test "$rollback_calls" -ge 3

test "$(wc -l < "$SOLVER_ROOT")" -lt 40
grep -q 'constraint/model_generation_and_index.inc' "$SOLVER_ROOT"
grep -q 'constraint/effect_propagation_and_residuals.inc' "$SOLVER_ROOT"
grep -q 'constraint/evidence_and_freeze.inc' "$SOLVER_ROOT"

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
