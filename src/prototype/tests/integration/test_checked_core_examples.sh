#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-checked-core-examples.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"

prototype_test_phase compile
prototype_compile c11 werror compiler \
	"$tmp_dir/checked-core-examples-check" \
	src/prototype/tests/checks/checked_core_examples_check.c

prototype_test_phase execute
"$tmp_dir/checked-core-examples-check" \
	examples/01_bool.p \
	examples/02_nat.p \
	examples/03_main.p \
	examples/04_match.p \
	examples/05_bool_to_nat.p \
	examples/06_pred.p \
	examples/07_add.p \
	examples/09_list_induction.p \
	src/prototype/tests/fixtures/typing/explicit_index_family_vec_check.p \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_check.p \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_concrete_check.p \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_eliminator_check.p \
	src/prototype/tests/fixtures/typing/function_graph_generated_length_check.p \
	src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p \
	src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p \
	src/prototype/tests/fixtures/typing/totality_evidence_surface_check.p \
	src/prototype/tests/fixtures/effects/higher_order_operation_handler_check.p

prototype_test_phase_finish
echo 'checked Core example tests passed'
