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
TYPING_PIPELINE=src/prototype/include/a_program/frontend/typing_pipeline.h
CONSTRAINT_TYPES=src/prototype/include/a_program/frontend/typing_constraint_state.h
CONSTRAINT_STATE=src/prototype/src/frontend/typing_constraint_state.c
CLASSIFIER_TYPES=src/prototype/include/a_program/frontend/typing_classifier_state.h
BINDING_TYPES=src/prototype/include/a_program/frontend/typing_binding_state.h
CONTEXT_TYPES=src/prototype/include/a_program/kernel/context.h
SOLVER_ROOT=$LOWERING_DIR/constraint_solver.inc
SOLVER_MODEL=$LOWERING_DIR/constraint/model_generation_and_index.inc
SOLVER_EFFECT=$LOWERING_DIR/constraint/effect_propagation_and_residuals.inc
SOLVER_COMPUTATION=$LOWERING_DIR/constraint/context_computation_and_fixed_point.inc
SOLVER_SOURCE=$LOWERING_DIR/constraint
JUDGEMENT_TYPES=src/prototype/include/a_program/kernel/judgement/types.h
USAGE_TYPES=src/prototype/include/a_program/frontend/typing_usage_state.h
READ_FILE=src/prototype/src/driver/read_file.c

grep -q 'struct prototype_typing_constraint_db' "$CONSTRAINT_TYPES"
grep -q 'struct prototype_typing_constraint_db constraints;' "$TYPING_PIPELINE"
if grep -q 'struct prototype_typing_constraint_db constraints;' \
	"$CONTEXT_SOURCE"; then
	echo 'transitional compile context still owns ConstraintDB' >&2
	exit 1
fi
grep -q 'OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER' "$CONSTRAINT_TYPES"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_COMPUTATION' "$CONSTRAINT_TYPES"
grep -q 'OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW' "$CONSTRAINT_TYPES"
grep -q 'struct prototype_typing_constraint_db_mark' "$CONSTRAINT_TYPES"
grep -q 'source_occurrence;' "$CONSTRAINT_TYPES"
grep -q 'owner_typed_projection;' "$CONSTRAINT_TYPES"
grep -q 'classifier_constraint_for_projection' "$CONSTRAINT_TYPES"
if grep -q 'classifier_meta_id\|base_classifier_constraint_for_occurrence' \
	"$CONSTRAINT_TYPES" "$SOLVER_SOURCE"/*.inc "$LOWERING_DIR"/*.inc; then
	echo 'classifier answer identity returned to occurrence-keyed storage' >&2
	exit 1
fi
if grep -R -q \
	'operation_solver_refresh_classifier_constraints\|operation_solver_classifier_topology_matches' \
	"$SOLVER_SOURCE"; then
	echo 'branch mutation returned a second classifier refresh route' >&2
	exit 1
fi
grep -q 'prototype_typing_constraint_transition(' "$CONSTRAINT_STATE"
if grep -R -q 'prototype_typing_constraint_reopen(' \
	"$CONSTRAINT_TYPES" "$CONSTRAINT_STATE" "$SOLVER_SOURCE"; then
	echo 'constraint lifecycle returned to a second reopen mutation API' >&2
	exit 1
fi
if grep -q 'operation_constraint_transition(\|operation_constraint_reopen(' \
	"$SOLVER_MODEL"; then
	echo 'constraint lifecycle implementation returned to the mixed solver' >&2
	exit 1
fi
grep -q 'operation_solver_generate_computation_constraints_impl(' \
	"$SOLVER_COMPUTATION"
grep -q 'operation_solver_bind_computation_constraint_solutions(' \
	"$SOLVER_COMPUTATION"
grep -q 'operation_solver_prepare_computation_constraint_inputs(' \
	"$SOLVER_COMPUTATION"
grep -q 'computation_constraint_for_occurrence' "$CONSTRAINT_TYPES"
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

if grep -R -q 'struct operation_solver_solution\|classifier_hints\[' \
	"$LOWERING_DIR"; then
	echo 'generic solver solution or classifier hint authority returned' >&2
	exit 1
fi

classifier_seed=$(sed -n \
	'/struct prototype_typing_classifier_seed {/,/^};/p' \
	"$CLASSIFIER_TYPES")
printf '%s\n' "$classifier_seed" | grep -q 'owner_occurrence;'
printf '%s\n' "$classifier_seed" | grep -q 'classifier;'
printf '%s\n' "$classifier_seed" | grep -q 'binder_classifier;'
printf '%s\n' "$classifier_seed" | grep -q 'reason;'
if printf '%s\n' "$classifier_seed" | grep -q \
	'effect_row\|first_usage\|usage_entry\|binder_usage\|stale_classifier'; then
	echo 'classifier seed owns another solver domain' >&2
	exit 1
fi
if grep -q 'struct prototype_typing_binder_solution' "$CLASSIFIER_TYPES"; then
	echo 'removed binder solution authority returned' >&2
	exit 1
fi

binder_owner=$(sed -n \
	'/struct prototype_typing_binder_owner {/,/^};/p' \
	"$BINDING_TYPES")
printf '%s\n' "$binder_owner" | grep -q 'binding_id;'
printf '%s\n' "$binder_owner" | grep -q 'binding_context;'
printf '%s\n' "$binder_owner" | grep -q 'first_classifier_evidence;'
if printf '%s\n' "$binder_owner" | grep -q 'classifier;'; then
	echo 'binding ownership index stores a classifier answer' >&2
	exit 1
fi
classifier_equation=$(sed -n \
	'/struct prototype_context_classifier_equation {/,/^};/p' \
	"$CONTEXT_TYPES")
printf '%s\n' "$classifier_equation" | grep -q 'binding_id;'
printf '%s\n' "$classifier_equation" | grep -q 'parent_context;'
printf '%s\n' "$classifier_equation" | grep -q 'answer;'

grep -q 'PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION' "$CONSTRAINT_TYPES"
grep -q 'effect_solution_constraint_for_projection' "$CONSTRAINT_TYPES"
if grep -R -q 'prototype_typing_effect_meta\|effect_solver\.metas\|effect_meta_id' \
	"$CONSTRAINT_TYPES" "$CONSTRAINT_STATE" "$SOLVER_SOURCE"; then
	echo 'effect answer returned to a second mutable meta authority' >&2
	exit 1
fi

usage_solution=$(sed -n \
	'/struct prototype_occurrence_usage_solution {/,/^};/p' \
	"$USAGE_TYPES")
printf '%s\n' "$usage_solution" | grep -q 'binder_usage;'
if grep -R -q 'classifier_metas\[[^]]*\]\.\(effect_row\|binder_usage\|usage_state\)' \
	"$LOWERING_DIR"; then
	echo 'classifier meta still mirrors effect or usage solutions' >&2
	exit 1
fi

constraint_record=$(sed -n \
	'/struct prototype_typing_constraint {/,/^};/p' \
	"$CONSTRAINT_TYPES")
if printf '%s\n' "$constraint_record" | grep -q 'context_id;'; then
	echo 'constraint edge stores a copied occurrence Context' >&2
	exit 1
fi
if printf '%s\n' "$constraint_record" | grep -E -q \
	'state;|reason;|evidence_id;|conversion_goal;|validated_motive;|refined_context;|substitution;|constructor_term;|residual_pattern;|residual_value;|refinement_status;'; then
	echo 'immutable constraint edge still owns mutable solution state' >&2
	exit 1
fi
constraint_solution=$(sed -n \
	'/struct prototype_typing_constraint_solution {/,/^};/p' \
	"$CONSTRAINT_TYPES")
printf '%s\n' "$constraint_solution" | grep -q 'state;'
printf '%s\n' "$constraint_solution" | grep -q 'result_term;'
printf '%s\n' "$constraint_solution" | grep -q 'producer_constraint_id;'
printf '%s\n' "$constraint_solution" | grep -q 'input_revision;'
printf '%s\n' "$constraint_solution" | grep -q 'answer_producer_input_revision;'
printf '%s\n' "$constraint_solution" | grep -q 'answer_context_revision;'
printf '%s\n' "$constraint_solution" | grep -q 'evidence_id;'
printf '%s\n' "$constraint_solution" | grep -q 'conversion_goal;'
printf '%s\n' "$constraint_solution" | grep -q 'validated_motive;'
printf '%s\n' "$constraint_solution" | grep -q 'refined_context;'
printf '%s\n' "$constraint_solution" | grep -q 'refinement_status;'
grep -q 'PROTOTYPE_TYPING_CONSTRAINT_CAPACITY' "$CONSTRAINT_TYPES"
grep -q 'constraint_hash_slots' "$CONSTRAINT_TYPES"
grep -q 'typing_constraint_hash(' "$CONSTRAINT_STATE"
grep -q 'prototype_typing_constraint_mark_input_changed(' "$CONSTRAINT_STATE"
grep -q 'operation_solver_answer_snapshot_matches(' \
	"$LOWERING_DIR/graph_construction.inc"
if rg -n -P \
	'answer_(pending_)?operand_fingerprint\s*==|==\s*[^;]*answer_(pending_)?operand_fingerprint' \
	"$LOWERING_DIR/graph_construction.inc"
then
	echo 'diagnostic answer fingerprint returned as reuse authority' >&2
	exit 1
fi
effect_add_body=$(sed -n \
	'/static int operation_effect_constraint_add(/,/^}/p' \
	"$SOLVER_EFFECT")
printf '%s\n' "$effect_add_body" | grep -q \
	'operation_solver_intern_constraint('
if grep -R -q 'prototype_typing_constraint_append\|operation_solver_append_.*constraint' \
	"$CONSTRAINT_TYPES" "$CONSTRAINT_STATE" "$SOLVER_SOURCE"; then
	echo 'append-named equation creation returned beside lookup-or-intern' >&2
	exit 1
fi
if printf '%s\n' "$effect_add_body" | grep -q 'for ('; then
	echo 'effect equation generation returned to a linear duplicate scan' >&2
	exit 1
fi

residual_effect_body=$(sed -n \
	'/static int compile_phase_record_residual_effect_constraints(/,/^}/p' \
	"$SOLVER_EFFECT")
if printf '%s\n' "$residual_effect_body" | grep -q \
	'metadata->effect_constraints\|effect_constraint_summary'; then
	echo 'verification obligations still read the diagnostic effect snapshot' >&2
	exit 1
fi
artifact_export_check=$(sed -n \
	'/static int artifact_exports_have_accepted_claims(/,/^}/p' \
	"$READ_FILE")
if printf '%s\n' "$artifact_export_check" | grep -q \
	'effect_constraints\|effect_constraint_summary'; then
	echo 'artifact acceptance still falls back to a diagnostic effect snapshot' >&2
	exit 1
fi

if grep -q 'effect_constraints\|effect_constraint_count\|effect_constraint_capacity' \
	src/prototype/include/a_program/graph/compile_metadata.h; then
	echo 'compile metadata still stores effect equations' >&2
	exit 1
fi

freeze_body=$(sed -n \
	'/static int operation_solver_freeze_occurrences(/,/^}/p' \
	"$SOLVER_SOURCE/evidence_and_freeze.inc")
printf '%s\n' "$freeze_body" | grep -q 'frozen->classifier = classifier;'
printf '%s\n' "$freeze_body" | grep -q \
	'frozen->binder_classifier = operation_solver_binder_classifier'
direct_frozen_writes=$(grep -R -h \
	'frozen->\(classifier\|binder_classifier\) =' "$LOWERING_DIR" | wc -l)
test "$direct_frozen_writes" -eq 3

validator_calls=$(grep -R -h 'operation_constraint_db_validate(ctx)' \
	"$SOLVER_SOURCE" | wc -l)
test "$validator_calls" -ge 3
rollback_calls=$(grep -R -h 'prototype_typing_constraint_db_rollback(' \
	"$SOLVER_SOURCE" | wc -l)
test "$rollback_calls" -ge 2
grep -q 'prototype_typing_constraint_db_rollback(' "$CONSTRAINT_STATE"

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

if rg -n -P \
	'motive_solutions\[[^]]*\]\.(motive|status|phase|constant_candidate)\s*=(?!=)' \
	"$SOLVER_SOURCE/branch_refinement_and_motives.inc" \
	"$LOWERING_DIR/finalization_and_entrypoints.inc"
then
	echo 'motive solution is written outside motive_solver.inc' >&2
	exit 1
fi
grep -q 'enum operation_motive_solution_status' "$CONSTRAINT_TYPES"
for status in UNSOLVED WAITING SOLVED RESIDUAL CONTRADICTION; do
	grep -q "OPERATION_MOTIVE_STATUS_$status" "$CONSTRAINT_TYPES"
done
grep -q 'enum operation_motive_solution_phase' "$CONSTRAINT_TYPES"
grep -q 'operation_motive_solution_transition_for_projection(' \
	"$SOLVER_SOURCE/motive_solver.inc"
test "$(grep -c '^[[:space:]]*\*current = \*next;' \
	"$SOLVER_SOURCE/motive_solver.inc")" -eq 1
if rg -n -P \
	'(solution|motive_solution)->(motive|status|phase|constant_candidate|'\
'expected_equation_[a-z_]+|dependency_mask|unavailable_mask|conflict_mask|'\
'source_case_index|source_classifier|recursive_equation_revision)\s*=(?!=)' \
	"$SOLVER_SOURCE/motive_solver.inc"
then
	echo 'motive cell bypasses the single transition API' >&2
	exit 1
fi
grep -q 'operation_solver_validate_accepted_motive_evidence(' \
	"$SOLVER_SOURCE/evidence_and_freeze.inc"
if grep -q 'recovered motive is not lambda' \
	"$SOLVER_SOURCE/motive_solver.inc"; then
	echo 'IH materialization still reconstructs a motive from a classifier' >&2
	exit 1
fi
close_line=$(grep -n 'operation_solver_close_match_motive_projections(ctx)' \
	"$LOWERING_DIR/finalization_and_entrypoints.inc" | head -1 | cut -d: -f1)
proof_line=$(grep -n 'operation_solver_materialize_judgements(ctx)' \
	"$LOWERING_DIR/finalization_and_entrypoints.inc" | head -1 | cut -d: -f1)
test "$close_line" -lt "$proof_line"
close_body=$(sed -n \
	'/static int operation_solver_close_match_motive_projections(/,/^}/p' \
	"$SOLVER_SOURCE/motive_solver.inc")
printf '%s\n' "$close_body" | grep -q \
	'solution->status != OPERATION_MOTIVE_STATUS_SOLVED'
grep -q 'expected_equation_classifier;' "$CLASSIFIER_TYPES"
grep -q 'expected_equation_constraint_id;' "$CLASSIFIER_TYPES"
grep -q 'expected_equation_context_id;' "$CLASSIFIER_TYPES"
grep -q 'operation_motive_offer_expected_equation(' \
	"$SOLVER_SOURCE/motive_solver.inc"
if grep -R -q 'operation_motive_accept_expected_materialization' \
	"$LOWERING_DIR"; then
	echo 'late expected-classifier motive materialization returned' >&2
	exit 1
fi
if rg -n -P 'expected_equation_(classifier|constraint_id|context_id)\s*=(?!=)' \
	"$LOWERING_DIR" -g '*.inc' | grep -v -E \
	'constraint/(motive_solver|model_generation_and_index)\.inc:'
then
	echo 'expected motive equation is written outside its Layer T authority' >&2
	exit 1
fi
if grep -q 'all_branches_impossible' "$SOLVER_SOURCE/motive_solver.inc"; then
	echo 'motive solver still reconstructs an impossible Match motive from its classifier' >&2
	exit 1
fi

A_PROGRAM_PERFORMANCE_COUNTERS=1 ./read_file.out \
	src/prototype/tests/fixtures/typing/function_graph_dependent_output_ih_check.p \
	>"$TMP_DIR/motive.out" 2>"$TMP_DIR/motive.err"
topology_line=$(grep '^A_PROGRAM_LAYER_T_TOPOLOGY_COUNTERS 1 ' \
	"$TMP_DIR/motive.err")
printf '%s\n' "$topology_line" | grep -q ' sealed=1 '
printf '%s\n' "$topology_line" | grep -q ' failures=0 '
printf '%s\n' "$topology_line" | grep -q ' external_motive_writes=0 '
printf '%s\n' "$topology_line" | grep -q ' post_seal_motive_writes=0$'
solution_lines=$(grep '^A_PROGRAM_LAYER_T_SEAL_GUARD_COUNTERS 1 ' \
	"$TMP_DIR/motive.err")
test "$(printf '%s\n' "$solution_lines" | wc -l)" -eq 6
if printf '%s\n' "$solution_lines" | grep -v \
	'proof_solver_calls=0$'
then
	echo 'sealed Layer T solutions changed downstream of solving' >&2
	exit 1
fi

./read_file.out --write-artifact "$TMP_DIR/indexed-map.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_map_check.p \
	>"$TMP_DIR/indexed-map.out" 2>"$TMP_DIR/indexed-map.err"
./read_file.out --read-graph "$TMP_DIR/indexed-map.apo" \
	>"$TMP_DIR/indexed-map-read.out" 2>"$TMP_DIR/indexed-map-read.err"

echo 'constraint authority checks passed'
