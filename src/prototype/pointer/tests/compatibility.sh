#!/usr/bin/env bash
set -eu
checker=$1
runtime=$2
fixtures=$3
failed=0
total=0

while read -r expectation name left right; do
	[ -n "$expectation" ] || continue
	total=$((total + 1))
	code=0
	output=$("$checker" --steps 1000000 "$fixtures/$name.p" 2>&1) || code=$?
	printf '%s\texit=%s\t%s\n' "$name" "$code" "$output"
	if [ "$code" -ne "$expectation" ]; then
		failed=$((failed + 1))
		continue
	fi
	if [ "$expectation" -eq 1 ]; then
		if ! "$runtime" --reject "$fixtures/$name.p"; then
			failed=$((failed + 1))
		fi
	fi
	if [ -n "$left" ]; then
		if ! "$runtime" --equal "$fixtures/$name.p" "$left" "$right"; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_generated_length_check ]; then
		if ! "$runtime" --equal "$fixtures/$name.p" certifiedMain expected; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_dependent_spine_check ]; then
		if ! "$runtime" --equal "$fixtures/$name.p" certified expected; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_two_recursive_calls_check ]; then
		if ! "$runtime" --equal "$fixtures/$name.p" certified expected; then
			failed=$((failed + 1))
		fi
	fi
done <<'CASES'
0 typing/explicit_index_family_vec_check
0 typing/explicit_index_family_acc_eliminator_check
1 typing/if8_order_check
0 typing/function_graph_generated_length_check main expected
0 typing/function_graph_dependent_output_ih_check main expected
0 typing/function_graph_two_recursive_calls_check main expected
0 typing/function_graph_dependent_spine_check main expected
0 typing/function_graph_nominal_index_constant_motive_check main expected
0 typing/function_graph_block_binding_check main expected
0 typing/function_graph_named_case_check
0 typing/if8_fuel_free_quicksort_check main expected
0 typing/if8_fuel_free_quicksort_check emptyMain emptyExpected
0 typing/if8_fuel_free_quicksort_check singletonMain singletonExpected
0 typing/if8_fuel_free_quicksort_check ascendingMain ascendingExpected
0 typing/if8_fuel_free_quicksort_check descendingMain descendingExpected
0 typing/if8_fuel_free_quicksort_check duplicateMain duplicateExpected
1 negative/function_graph_incompatible_recursive_property
0 negative/function_graph_coarse_forgery
1 ../../pointer/tests/acceptance/generated-function-graph-direct-forgery
1 ../../pointer/tests/acceptance/indexed-rigid-induction-invalid
CASES
# The historical coarse-fallback fixture only requests *first; it makes no
# false claim. Exact direct-body graph generation admits it. The final case
# instead demands the wrong result index from that graph and must reject.
# The original order fixture recursively folds LT at a fixed right index.
# Its step branch reuses Acc k even when the recursive LT input has another
# right index. The final counterexample demonstrates that the old compiler
# accepted Acc zero whose constructor exposes index one. Rejecting both files
# is intentional, not restoration of that historical acceptance bug.
printf 'source compatibility and selected results: %s/%s passed\n' "$((total - failed))" "$total"
test "$failed" -eq 0
