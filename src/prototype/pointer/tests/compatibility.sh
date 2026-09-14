#!/usr/bin/env bash
set -eu
# Frozen source fixtures keep their old spelling. Compatibility is explicit;
# current source/default rejection is checked independently by cli.sh.
checker=("$1" --legacy-intrinsic-dot)
runtime=("$2" --legacy-intrinsic-dot)
fixtures=$3
failed=0
total=0

while read -r expectation name left right; do
	[ -n "$expectation" ] || continue
	total=$((total + 1))
	code=0
	output=$("${checker[@]}" --steps 1000000 "$fixtures/$name.p" 2>&1) || code=$?
	printf '%s\texit=%s\t%s\n' "$name" "$code" "$output"
	if [ "$code" -ne "$expectation" ]; then
		failed=$((failed + 1))
		continue
	fi
	if [ "$expectation" -eq 1 ]; then
		if ! "${runtime[@]}" --reject "$fixtures/$name.p"; then
			failed=$((failed + 1))
		fi
	fi
	if [ -n "$left" ]; then
		if ! "${runtime[@]}" --equal "$fixtures/$name.p" "$left" "$right"; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_generated_length_check ]; then
		if ! "${runtime[@]}" --equal "$fixtures/$name.p" certifiedMain expected; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_dependent_spine_check ]; then
		if ! "${runtime[@]}" --equal "$fixtures/$name.p" certified expected; then
			failed=$((failed + 1))
		fi
	fi
	if [ "$name" = typing/function_graph_two_recursive_calls_check ]; then
		if ! "${runtime[@]}" --equal "$fixtures/$name.p" certified expected; then
			failed=$((failed + 1))
		fi
	fi
done <<'CASES'
0 typing/explicit_index_family_vec_check
0 typing/explicit_index_family_append_check
0 typing/explicit_index_family_tail_check
0 typing/explicit_index_family_tail_infer
0 typing/explicit_index_family_acc_eliminator_check
0 typing/explicit_index_family_fin_check
0 typing/explicit_index_family_head_check
0 typing/explicit_index_family_map_check
0 typing/explicit_index_family_acc_check
0 typing/explicit_index_family_acc_parameter_specialization_check
0 typing/explicit_index_family_acc_full_specialization_check
0 typing/explicit_index_family_acc_concrete_check
0 typing/iadts_box_perfect_construction_check
0 typing/outer_ih_nested_match_check tailResult tailExpected
0 typing/outer_ih_nested_match_check functionResult functionExpected
0 typing/recursive_ih_field_identity_check leftResult leftExpected
0 typing/recursive_ih_field_identity_check rightResult rightExpected
0 typing/recursive_ih_field_identity_check bothResult leftExpected
0 typing/recursive_ih_field_identity_check sizeResult sizeExpected
0 typing/recursive_ih_field_identity_check choiceLeftResult leftExpected
0 typing/recursive_ih_field_identity_check choiceRightResult rightExpected
0 typing/list_map_induction_check directMain expectedMulti
0 typing/list_map_induction_check helperMain expectedMulti
0 typing/list_map_induction_check monomorphicEmpty expectedEmpty
0 typing/list_map_induction_check monomorphicSingle expectedSingle
0 typing/list_map_induction_check monomorphicMulti expectedMulti
0 typing/list_map_induction_check polymorphicMain expectedMulti
0 typing/list_map_induction_check sequencedMain expectedMulti
0 typing/dependent_pi_surface_check
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
0 typing/function_graph_certified_length_model
0 typing/host_text_recursive_motive_check main expected
0 typing/host_expression_evaluator_check main expected
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
acceptance="$(dirname "${BASH_SOURCE[0]}")/acceptance"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/print.a" "$acceptance/host-print.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in main alias discard twice forward reemitted arithmetic; do
		"${runtime[@]}" --equal-image "$directory/print.a" "$name" expected
	done
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/arithmetic.a" "$acceptance/host-arithmetic.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for pair in main:expected overflow:minimum underflow:maximum product:negativeTwo negatedMinimum:minimum partialResult:expected higherResult:expected unboxed:expected; do
		"${runtime[@]}" --equal-image "$directory/arithmetic.a" "${pair%:*}" "${pair#*:}"
	done
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/host-expression.a" \
		"$fixtures/typing/host_expression_evaluator_check.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/host-expression.a" main expected
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/host.a" "$acceptance/host-literals.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/host.a" main expected
	"${runtime[@]}" --equal-image "$directory/host.a" minimum minimumExpected
	"${runtime[@]}" --equal-image "$directory/host.a" maximum maximumExpected
	"${runtime[@]}" --equal-image "$directory/host.a" textMain textExpected
	"${runtime[@]}" --equal-image "$directory/host.a" emptyMain emptyExpected
	"${runtime[@]}" --equal-image "$directory/host.a" same expected
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/host-recursion.a" \
		"$fixtures/typing/host_text_recursive_motive_check.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/host-recursion.a" main expected
done
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/function_graph_certified_length_model.p" \
		--save "$directory/length.a" "$acceptance/legacy-certified-length-results.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/length.a" original one
	"${runtime[@]}" --equal-image "$directory/length.a" empty zero
	"${runtime[@]}" --equal-image "$directory/length.a" many three
	code=0
	"${checker[@]}" --steps "$steps" --save "$directory/candidate.a" \
		"$acceptance/certified-length-candidate.p" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/candidate.a" main two
	"${runtime[@]}" --equal-image "$directory/candidate.a" base zero
	"${runtime[@]}" --equal-image "$directory/candidate.a" singleton one
	"${runtime[@]}" --equal-image "$directory/candidate.a" erased emptyResult
	"${runtime[@]}" --equal-image "$directory/candidate.a" tagged expectedTag
	"${runtime[@]}" --equal-image "$directory/candidate.a" skipped expectedSkip
done
# Removing post-checks must not remove the ability to synthesize this motive.
sed -e 's/(graph :: LengthGraph tail tailLength)/graph/' -e '/length :: /d' \
	"$fixtures/typing/function_graph_certified_length_model.p" > "$directory/length-no-expect.p"
"${checker[@]}" --steps 100000 "$directory/length-no-expect.p"
# A constant nil certificate cannot acquire a dependent input classifier.
sed 's/eraseCertified :: NatList->LengthResult NatList.nil;/eraseCertified :: (xs:NatList)->LengthResult xs;/' \
	"$acceptance/certified-length-candidate.p" > "$directory/wrong-candidate.p"
"${runtime[@]}" --reject "$directory/wrong-candidate.p"
code=0
"${checker[@]}" --steps 0 --save "$directory/wrong-candidate.a" "$directory/wrong-candidate.p" > "$directory/status" || code=$?
test "$code" -eq 3
code=0
"${checker[@]}" --load "$directory/wrong-candidate.a" > "$directory/status" || code=$?
test "$code" -eq 1
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-acc-concrete-results.p"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/explicit_index_family_acc_concrete_check.p" \
		--save "$directory/acc.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/acc.a" falseMain falseExpected
	"${runtime[@]}" --equal-image "$directory/acc.a" main expected
done
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/acc-concrete-successor.p"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/explicit_index_family_acc_concrete_check.p" \
		--save "$directory/successor.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for pair in main:expected base:zero childIndex:falseValue; do
		"${runtime[@]}" --equal-image "$directory/successor.a" "${pair%:*}" "${pair#*:}"
	done
done
code=0
"${checker[@]}" --imports "$fixtures/typing/explicit_index_family_acc_concrete_check.p" \
	--save "$directory/wrong-successor.a" "${client%.p}-wrong.p" > "$directory/status" || code=$?
test "$code" -eq 1
code=0
"${checker[@]}" --load "$directory/wrong-successor.a" > "$directory/status" || code=$?
test "$code" -eq 1
sed 's/down Bool.false Precedes.falseBeforeTrue/down Bool.true Precedes.falseBeforeTrue/' \
	"$client" > "$directory/wrong-child.p"
code=0
"${checker[@]}" --imports "$fixtures/typing/explicit_index_family_acc_concrete_check.p" \
	--save "$directory/wrong-child.a" "$directory/wrong-child.p" > "$directory/status" || code=$?
test "$code" -eq 1
code=0
"${checker[@]}" --load "$directory/wrong-child.a" > "$directory/status" || code=$?
test "$code" -eq 1
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-vec-append-results.p"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/explicit_index_family_append_check.p" \
		--save "$directory/append.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for pair in original:expected empty:nil leftEmpty:right rightEmpty:right ordered:pair recursive:triple; do
		"${runtime[@]}" --equal-image "$directory/append.a" "${pair%:*}" "${pair#*:}"
	done
done
acceptance="$(dirname "${BASH_SOURCE[0]}")/acceptance"
"${checker[@]}" --steps 100000 --imports "$acceptance/computed-index-arithmetic.p" \
	"$acceptance/computed-constructor-index.p"
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-rebuild-results.p"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/indexed_branch_rebuild_check.p" \
		--save "$directory/rebuild.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in emptyMain recursiveMain; do
		"${runtime[@]}" --equal-image "$directory/rebuild.a" "$name" "${name}Expected"
	done
done
"${runtime[@]}" "$fixtures/../../../../examples/type-infer-and-check/level2/02_tree.p" Nat zero succ 3
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-comparison-results.p"
for steps in 0 100000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/dependent_recursive_comparison_check.p" \
		--save "$directory/comparison.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	for name in baseLeft baseRight less greater same; do
		"${runtime[@]}" --equal-image "$directory/comparison.a" "$name" "${name}Expected"
	done
done

# The original Acc-based function must also produce its own result witness;
# the output checks are not a proof of general sortedness or preservation.
client="$(dirname "${BASH_SOURCE[0]}")/acceptance/legacy-quicksort-witness.p"
for steps in 0 1000000; do
	code=0
	"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
		--save "$directory/quicksort.a" "$client" > "$directory/status" || code=$?
	if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
	"${runtime[@]}" --equal-image "$directory/quicksort.a" main expected
	"${runtime[@]}" --equal-image "$directory/quicksort.a" emptyMain emptyExpected
done

# Clients consume the unchanged provider's graph and prove separate specifications.
check_property() {
	local client=$1 steps code pair
	shift
	for steps in 0 1000000; do
		code=0
		"${checker[@]}" --steps "$steps" --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
			--save "$directory/property.a" "$client" > "$directory/status" || code=$?
		if [ "$steps" -eq 0 ]; then test "$code" -eq 3; else test "$code" -eq 0; fi
		for pair in "$@"; do
			"${runtime[@]}" --equal-image "$directory/property.a" "${pair%:*}" "${pair#*:}"
		done
	done
}

check_wrong_property() {
	local client=$1 code
	code=0
	"${checker[@]}" --steps 1000000 --imports "$fixtures/typing/if8_fuel_free_quicksort_check.p" \
		--save "$directory/wrong-property.a" "$client" > "$directory/status" || code=$?
	test "$code" -eq 1
	code=0
	"${checker[@]}" --steps 1000000 --load "$directory/wrong-property.a" > "$directory/status" || code=$?
	test "$code" -eq 1
}

while read -r name pairs; do
	check_property "$acceptance/$name.p" $pairs
	check_wrong_property "$acceptance/$name-wrong.p"
done <<'CLIENTS'
legacy-measure-property main:input emptyMain:empty
legacy-partition-property main:expected lowerMain:expected upperMain:upperExpected duplicatesMain:duplicatesExpected emptyMain:empty
legacy-quicksort-graph main:expected emptyMain:empty graphMain:expected
CLIENTS

client="$acceptance/legacy-quicksort-property.p"
check_property "$client" main:ascending emptyMain:empty singletonMain:singleton \
	ascendingMain:ascending descendingMain:ascending duplicatesMain:duplicatesExpected unorderedMain:mixed
# Reuse the full specification, but substitute the right recursive proof where
# the left is required. No second, drifting copy of the specification is needed.
sed 's/left right result \*leftGraph \*rightGraph/left right result *rightGraph *rightGraph/' \
	"$client" > "$directory/wrong-quicksort-property.p"
check_wrong_property "$directory/wrong-quicksort-property.p"
