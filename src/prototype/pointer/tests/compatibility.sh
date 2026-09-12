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
done <<'CASES'
0 typing/explicit_index_family_vec_check
0 typing/explicit_index_family_acc_eliminator_check
0 typing/if8_order_check
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
1 negative/function_graph_coarse_forgery
CASES
printf 'source compatibility and selected results: %s/%s passed\n' "$((total - failed))" "$total"
test "$failed" -eq 0
