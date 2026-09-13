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
0 typing/explicit_index_family_append_check
0 typing/explicit_index_family_tail_check
0 typing/explicit_index_family_tail_infer
0 typing/explicit_index_family_acc_eliminator_check
0 typing/indexed_branch_rebuild_check
0 typing/residual_index_equation_negative
0 ../../../../examples/type-infer-and-check/level2/02_tree
0 typing/dependent_recursive_comparison_check
0 typing/insertion_sort_check main expected
0 typing/eager_insertion_check main expected
0 typing/eager_insertion_check earlyMain earlyExpected
0 typing/eager_insertion_check traceEarly traceEarlyExpected
0 typing/eager_insertion_check traceRecursive traceRecursiveExpected
1 typing/if8_order_check
0 typing/function_graph_generated_length_check main expected
0 typing/function_graph_dependent_output_ih_check main expected
0 typing/function_graph_two_recursive_calls_check main expected
0 typing/function_graph_dependent_spine_check main expected
0 typing/function_graph_nominal_index_constant_motive_check main expected
0 typing/function_graph_block_binding_check main expected
1 typing/function_graph_named_case_check
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
# The named-case fixture omits a reachable nil case from selectGraph. The
# old integration test checks only that the term was printed; the dedicated
# function-graph-missing-case fixture demonstrates a stuck closed Match on
# nil in that compiler. Omission is accepted only with checked refutation;
# function-graph-refined-case tests a valid nonempty input in the main suite.
# Despite its historical filename, residual_index_equation_negative contains
# no false equality assertion: the result is Nat independently of the index.
# Retaining an unresolved index equation must not reject that constant motive.
printf 'source compatibility and selected results: %s/%s passed\n' "$((total - failed))" "$total"
test "$failed" -eq 0

# Check both base cases and recursive directions, including the returned LE
# witnesses, using the original module through ordinary imports and Solve.
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-vec-append-results.p"
for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --imports "$fixtures/typing/explicit_index_family_append_check.p" \
		--save "$directory/append.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for pair in original:expected empty:nil leftEmpty:right rightEmpty:right ordered:pair recursive:triple; do
		"$runtime" --equal-image "$directory/append.a" "${pair%:*}" "${pair#*:}"
	done
done
acceptance="$(dirname "${BASH_SOURCE[0]}")/acceptance"
"$checker" --steps 100000 --imports "$acceptance/computed-index-arithmetic.p" \
	"$acceptance/computed-constructor-index.p"
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-rebuild-results.p"
for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --imports "$fixtures/typing/indexed_branch_rebuild_check.p" \
		--save "$directory/rebuild.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in emptyMain recursiveMain; do
		"$runtime" --equal-image "$directory/rebuild.a" "$name" "${name}Expected"
	done
done
"$runtime" "$fixtures/../../../../examples/type-infer-and-check/level2/02_tree.p" Nat zero succ 3
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-comparison-results.p"
for steps in 0 100000; do
	code=0
	"$checker" --steps "$steps" --imports "$fixtures/typing/dependent_recursive_comparison_check.p" \
		--save "$directory/comparison.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in baseLeft baseRight less greater same; do
		"$runtime" --equal-image "$directory/comparison.a" "$name" "${name}Expected"
	done
done

# The original Acc-based function must also produce its own result witness;
# the output checks are not a proof of general sortedness or preservation.
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-quicksort-witness.p"
for steps in 0 1000000; do
	code=0
	"$checker" --steps "$steps" --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
		--save "$directory/quicksort.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"$runtime" --equal-image "$directory/quicksort.a" main expected
	"$runtime" --equal-image "$directory/quicksort.a" emptyMain emptyExpected
done

# A separate client proves element-preserving measurement for every input,
# using the unchanged QuickSort provider's graph, not a copied implementation.
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-measure-property.p"
for steps in 0 1000000; do
	code=0
	"$checker" --steps "$steps" --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
		--save "$directory/measure.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"$runtime" --equal-image "$directory/measure.a" main input
	"$runtime" --equal-image "$directory/measure.a" emptyMain empty
done
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-measure-property-wrong.p"
code=0
"$checker" --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
	--save "$directory/wrong-measure.a" "$client" > "$directory/status" || code=$?
test "$code" -eq 1
code=0
"$checker" --load "$directory/wrong-measure.a" > "$directory/status" || code=$?
test "$code" -eq 1
