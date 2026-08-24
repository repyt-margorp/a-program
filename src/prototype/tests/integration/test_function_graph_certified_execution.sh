#!/bin/sh
set -eu

# Boundary audit: ISSUE-13-FUNCTION-GRAPH-RESULT-EVIDENCE

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader a.out >/dev/null

positive=src/prototype/tests/fixtures/typing/function_graph_generated_length_check.p
model=src/prototype/tests/fixtures/typing/function_graph_certified_length_model.p
two_recursive=src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p
dependent_spine=src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p
block_binding=src/prototype/tests/fixtures/typing/function_graph_block_binding_check.p
named_case=src/prototype/tests/fixtures/typing/function_graph_named_case_check.p
quick_sort=src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p

test -f "$model"

prototype_test_phase parser_boundary
./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_vec_check.p \
	>"$tmp_dir/explicit-index.out"
grep -q '^type (Vec A) constructors=2$' "$tmp_dir/explicit-index.out"

prototype_test_phase generated_graph
./read_file.out "$positive" >"$tmp_dir/positive.out"
grep -q '^term certifiedMain :=' "$tmp_dir/positive.out"
grep -q '^term main :=' "$tmp_dir/positive.out"

prototype_test_phase two_recursive_calls
./read_file.out "$two_recursive" >"$tmp_dir/two-recursive.out"
grep -q '^constructor \$graph\.mirror\.fork readback_fields=6 ' \
	"$tmp_dir/two-recursive.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected "$two_recursive" >"$tmp_dir/two-recursive-equality.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/two-recursive-equality.out"

prototype_test_phase dependent_argument_spine
./read_file.out "$dependent_spine" >"$tmp_dir/dependent-spine.out"
grep -q '^term certified :=' "$tmp_dir/dependent-spine.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected "$dependent_spine" >"$tmp_dir/dependent-spine-equality.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/dependent-spine-equality.out"

prototype_test_phase block_bound_recursive_result
./read_file.out "$block_binding" >"$tmp_dir/block-binding.out"
grep -q '^constructor \$graph\.length\.cons readback_fields=4 ' \
	"$tmp_dir/block-binding.out"
grep -q 'name=tailLength' "$tmp_dir/block-binding.out"
./read_file.out --check-source-exports-normalization-equal \
	main expected "$block_binding" >"$tmp_dir/block-binding-equality.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/block-binding-equality.out"

prototype_test_phase named_case_roles
./read_file.out "$named_case" >"$tmp_dir/named-case.out"
grep -q '^term inspect :=' "$tmp_dir/named-case.out"
grep -q '^term selectGraph :=' "$tmp_dir/named-case.out"
grep -q '^term package :=' "$tmp_dir/named-case.out"
grep -q '^term directValue :=' "$tmp_dir/named-case.out"
grep -q '^term proof :=' "$tmp_dir/named-case.out"
grep -q 'CASE(cons .*INDUCTION_HYPOTHESIS' "$tmp_dir/named-case.out"
if grep -q '@returned' "$named_case"
then
	echo 'ordinary named graph fixture still exposes generated returned syntax' >&2
	exit 1
fi

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_named_unknown_selector.p \
	>"$tmp_dir/named-unknown.out" 2>"$tmp_dir/named-unknown.err"
then
	echo 'unknown named graph selector unexpectedly compiled' >&2
	exit 1
fi
grep -q 'unknown named graph selector' "$tmp_dir/named-unknown.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_named_unavailable_role.p \
	>"$tmp_dir/named-role.out" 2>"$tmp_dir/named-role.err"
then
	echo 'unavailable named graph role unexpectedly compiled' >&2
	exit 1
fi
grep -q 'named graph selector has no graph role' "$tmp_dir/named-role.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_direct_result_ih.p \
	>"$tmp_dir/direct-result-ih.out" 2>"$tmp_dir/direct-result-ih.err"
then
	echo 'direct certified result unexpectedly exposed an induction hypothesis' >&2
	exit 1
fi
grep -q 'named graph selector has no induction role' \
	"$tmp_dir/direct-result-ih.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_companion_without_raw.p \
	>"$tmp_dir/companion-without-raw.out" \
	2>"$tmp_dir/companion-without-raw.err"
then
	echo 'graph companion without a raw binder unexpectedly compiled' >&2
	exit 1
fi
grep -q 'graph companion requires a preceding raw binder' \
	"$tmp_dir/companion-without-raw.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_certified_companion_without_graph.p \
	>"$tmp_dir/certified-without-graph.out" \
	2>"$tmp_dir/certified-without-graph.err"
then
	echo 'certified companion without a graph binder unexpectedly compiled' >&2
	exit 1
fi
grep -q 'certified companion requires preceding raw and graph binders' \
	"$tmp_dir/certified-without-graph.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_certified_companion_nonbinary.p \
	>"$tmp_dir/certified-nonbinary.out" \
	2>"$tmp_dir/certified-nonbinary.err"
then
	echo 'non-binary certified companion unexpectedly compiled' >&2
	exit 1
fi
grep -q 'certified companion requires a binary callback classifier' \
	"$tmp_dir/certified-nonbinary.err"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_certified_companion_nonbool.p \
	>"$tmp_dir/certified-nonbool.out" \
	2>"$tmp_dir/certified-nonbool.err"
then
	echo 'non-Bool certified companion unexpectedly compiled' >&2
	exit 1
fi
grep -q 'certified companion currently requires a binary Bool callback' \
	"$tmp_dir/certified-nonbool.err"

prototype_test_phase static_inspection
inspection_source=$block_binding
./read_file.out --quiet --show-function-graph length "$inspection_source" \
	>"$tmp_dir/function-graph-inspection.out"
grep -q '^function-graph owner=length association=0 source=local ' \
	"$tmp_dir/function-graph-inspection.out"
grep -q '^constructor ordinal=1 name=cons fields=4$' \
	"$tmp_dir/function-graph-inspection.out"
grep -q '^origin name=tailLength roles=value,graph,ih value-field=2 graph-field=3 recursive=yes$' \
	"$tmp_dir/function-graph-inspection.out"
./read_file.out --quiet --show-function-graph length "$inspection_source" \
	>"$tmp_dir/function-graph-inspection-repeat.out"
cmp "$tmp_dir/function-graph-inspection.out" \
	"$tmp_dir/function-graph-inspection-repeat.out"

printf ':graph length\n:q\n' | ./a.out \
	src/prototype/tests/fixtures/typing/function_graph_import_provider_without_association.p \
	>"$tmp_dir/function-graph-repl-absent.out"
grep -q 'function graph length: absent; recompile with --show-function-graph length' \
	"$tmp_dir/function-graph-repl-absent.out"
printf ':graph length\n:q\n' | ./a.out "$named_case" \
	>"$tmp_dir/function-graph-repl-accepted.out"
grep -q 'function-graph owner=length association=0 source=local ' \
	"$tmp_dir/function-graph-repl-accepted.out"

prototype_test_phase quicksort_dependency_closure
{
	sed -n '1,$p' "$quick_sort"
	printf '%s\n' \
		'leftIsZero := \left : Nat => \right : Nat =>' \
			'left @zero => Bool.true @succ predecessor => Bool.false;' \
			'' \
			'partitionGraphProbe := \pivot : Nat => \size : Nat =>' \
			'\input : SizedList Nat size =>' \
			'*partition Nat &leftIsZero pivot size input;' \
			'' \
			'quickSortGraphProbe := *quickSort Nat &leftIsZero sample;' \
			'' \
			'inspectQuickSortAcc := \le : Nat -> Nat -> Bool =>' \
			'\@le : (left : Nat) -> (right : Nat) -> Bool -> @ =>' \
			'\*le =>' \
			'\size : Nat => \access : Acc Nat LT size =>' \
			'\input : SizedList Nat size => \output : List Nat =>' \
			'\graph : @quickSortAcc Nat @le size access input output =>' \
			'graph' \
			'@nil current down => (List Nat).nil' \
			'@cons { lowerResult; upperResult; } =>' \
			'append Nat *lowerResult *upperResult;'
} >"$tmp_dir/quicksort-graph.p"
./read_file.out --write-artifact "$tmp_dir/quicksort-graph.apo" \
	"$tmp_dir/quicksort-graph.p" >"$tmp_dir/quicksort-graph.out"
grep -q '^type (\$graph.quickSortAcc A le) constructors=2$' \
	"$tmp_dir/quicksort-graph.out"
grep -q '^constructor (\$graph.quickSortAcc A le).cons readback_fields=17 ' \
	"$tmp_dir/quicksort-graph.out"
grep -q '^type (\$graph.quickSort A le xs) constructors=1$' \
	"$tmp_dir/quicksort-graph.out"
grep -q '^term \$certified.quickSortAcc :=' "$tmp_dir/quicksort-graph.out"
grep -q '^term inspectQuickSortAcc :=' "$tmp_dir/quicksort-graph.out"
grep -q '^term partitionGraphProbe :=' "$tmp_dir/quicksort-graph.out"
grep -q '^term quickSortGraphProbe :=' "$tmp_dir/quicksort-graph.out"
./read_file.out --quiet --show-function-graph quickSortAcc \
	"$tmp_dir/quicksort-graph.p" >"$tmp_dir/quicksort-inspection.out"
grep -q 'name=lowerResult roles=value,graph,ih' \
	"$tmp_dir/quicksort-inspection.out"
grep -q 'name=upperResult roles=value,graph,ih' \
	"$tmp_dir/quicksort-inspection.out"
./read_file.out --read-graph "$tmp_dir/quicksort-graph.apo" \
	>"$tmp_dir/quicksort-graph-read.out"
grep -q '^interface term \$certified\.quickSort ' \
	"$tmp_dir/quicksort-graph-read.out"

expect_quicksort_association_rejection() {
	artifact=$1
	message=$2
	if ./read_file.out --read-graph "$artifact" \
		>"$artifact.out" 2>"$artifact.err"
	then
		echo "$message" >&2
		exit 1
	fi
}

awk '
	$1 == "function_graph_association" && $9 != 4294967295 && !done {
		$9 = 4294967295
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/quicksort-graph.apo" >"$tmp_dir/quicksort-higher-as-first.apo"
expect_quicksort_association_rejection \
	"$tmp_dir/quicksort-higher-as-first.apo" \
	'higher-order function graph accepted the first-order sentinel'

awk '
	$1 == "function_graph_association" && $9 != 4294967295 && !done {
		$9 = 999
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/quicksort-graph.apo" >"$tmp_dir/quicksort-higher-out-of-range.apo"
expect_quicksort_association_rejection \
	"$tmp_dir/quicksort-higher-out-of-range.apo" \
	'higher-order function graph accepted an out-of-range callback index'

awk '
	$1 == "function_graph_association" && $9 == 4294967295 && !done {
		$9 = 0
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/quicksort-graph.apo" >"$tmp_dir/quicksort-first-as-higher.apo"
expect_quicksort_association_rejection \
	"$tmp_dir/quicksort-first-as-higher.apo" \
	'first-order function graph accepted a fabricated callback index'

awk '
	$1 == "function_graph_association" && $9 != 4294967295 && !done {
		temporary = $4
		$4 = $5
		$5 = temporary
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/quicksort-graph.apo" >"$tmp_dir/quicksort-swapped-families.apo"
expect_quicksort_association_rejection \
	"$tmp_dir/quicksort-swapped-families.apo" \
	'function graph accepted swapped graph and result families'

prototype_test_phase executable_projection
./read_file.out --check-source-exports-normalization-equal \
	main expected "$positive" >"$tmp_dir/equality.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$tmp_dir/equality.out"

prototype_test_phase artifact_association
./read_file.out --write-artifact "$tmp_dir/function-graph.apo" "$positive" \
	>"$tmp_dir/artifact-write.out"
./read_file.out --write-artifact "$tmp_dir/function-graph-repeat.apo" "$positive" \
	>"$tmp_dir/artifact-repeat.out"
cmp "$tmp_dir/function-graph.apo" "$tmp_dir/function-graph-repeat.apo"
grep -q '^function_graph_associations 1$' "$tmp_dir/function-graph.apo"
grep -q '^function_graph_association 0 ' "$tmp_dir/function-graph.apo"
grep -q '^function_graph_selector_groups 2$' "$tmp_dir/function-graph.apo"
grep -q '^function_graph_selector_group 0 0 1 head 1 0 4294967295 0$' \
	"$tmp_dir/function-graph.apo"
./read_file.out --read-graph "$tmp_dir/function-graph.apo" \
	>"$tmp_dir/artifact-read.out"
grep -q '^interface term \$certified\.length ' "$tmp_dir/artifact-read.out"

prototype_test_phase imported_association
./read_file.out --write-artifact "$tmp_dir/function-graph-consumer.apo" \
	--import-interface "$tmp_dir/function-graph.apo" \
	src/prototype/tests/fixtures/typing/function_graph_import_consumer.p \
	>"$tmp_dir/function-graph-consumer.out"
grep -q '^term certified :=' "$tmp_dir/function-graph-consumer.out"
grep -q '^term main :=' "$tmp_dir/function-graph-consumer.out"
./read_file.out --read-graph "$tmp_dir/function-graph-consumer.apo" \
	>"$tmp_dir/function-graph-consumer-read.out"
grep -Fq 'dependency $certified.length namespace ' \
	"$tmp_dir/function-graph-consumer.apo"
./read_file.out --link-artifacts "$tmp_dir/function-graph-consumer.apo" \
	"$tmp_dir/function-graph.apo" \
	--link-reexport-providers \
	--link-output "$tmp_dir/function-graph-consumer-linked.apo" \
	>"$tmp_dir/function-graph-consumer-link.out"
./read_file.out --read-graph "$tmp_dir/function-graph-consumer-linked.apo" \
	>"$tmp_dir/function-graph-consumer-linked-read.out"
grep -q 'relocation_external_terms=0 relocation_resolved_external_terms=0' \
	"$tmp_dir/function-graph-consumer-linked-read.out"
grep -q '^interface term \$certified\.length ' \
	"$tmp_dir/function-graph-consumer-linked-read.out"

./read_file.out --quiet --show-function-graph length \
	--import-interface "$tmp_dir/function-graph.apo" \
	src/prototype/tests/fixtures/typing/function_graph_import_consumer.p \
	>"$tmp_dir/function-graph-imported-inspection.out"
grep -q '^function-graph owner=length association=0 source=imported ' \
	"$tmp_dir/function-graph-imported-inspection.out"
grep -q '^origin name=head roles=value value-field=0 graph-field=none recursive=no$' \
	"$tmp_dir/function-graph-imported-inspection.out"

./read_file.out --import-interface "$tmp_dir/function-graph.apo" \
	src/prototype/tests/fixtures/typing/function_graph_import_named_consumer.p \
	>"$tmp_dir/function-graph-imported-named.out"
grep -q '^term graphHead :=' "$tmp_dir/function-graph-imported-named.out"
grep -q '^term certified :=' "$tmp_dir/function-graph-imported-named.out"

./read_file.out --write-artifact "$tmp_dir/function-without-graph.apo" \
	src/prototype/tests/fixtures/typing/function_graph_import_provider_without_association.p \
	>"$tmp_dir/function-without-graph.out"
if ./read_file.out \
	--import-interface "$tmp_dir/function-without-graph.apo" \
	src/prototype/tests/fixtures/typing/function_graph_import_consumer.p \
	>"$tmp_dir/missing-import-graph.out" 2>"$tmp_dir/missing-import-graph.err"
then
	echo 'imported owner without graph association unexpectedly compiled' >&2
	exit 1
fi
grep -q 'imported function graph association missing' \
	"$tmp_dir/missing-import-graph.err"

sed -E \
	's/^(function_graph_association [0-9]+ [0-9]+) [0-9]+ /\1 4294967295 /' \
	"$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-corrupt.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-corrupt.apo" \
	>"$tmp_dir/corrupt.out" 2>"$tmp_dir/corrupt.err"
then
	echo 'out-of-range function graph association unexpectedly read' >&2
	exit 1
fi

sed -E \
	's/^(function_graph_association 0) [0-9]+ /\1 0 /' \
	"$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-wrong-owner.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-wrong-owner.apo" \
	>"$tmp_dir/wrong-owner.out" 2>"$tmp_dir/wrong-owner.err"
then
	echo 'wrong function graph owner unexpectedly read' >&2
	exit 1
fi

sed -E \
	's/^(function_graph_association 0 [0-9]+ [0-9]+) [0-9]+ /\1 2 /' \
	"$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-wrong-result.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-wrong-result.apo" \
	>"$tmp_dir/wrong-result.out" 2>"$tmp_dir/wrong-result.err"
then
	echo 'wrong function graph result family unexpectedly read' >&2
	exit 1
fi

awk '
	$1 == "function_graph_selector_group" && !done {
		$6 = 3
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-selector-wrong-role.apo"
if ./read_file.out --read-graph \
	"$tmp_dir/function-graph-selector-wrong-role.apo" \
	>"$tmp_dir/selector-wrong-role.out" 2>"$tmp_dir/selector-wrong-role.err"
then
	echo 'function graph selector accepted a graph role without a field' >&2
	exit 1
fi

awk '
	$1 == "function_graph_selector_group" && !done {
		$7 = 999
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-selector-wrong-field.apo"
if ./read_file.out --read-graph \
	"$tmp_dir/function-graph-selector-wrong-field.apo" \
	>"$tmp_dir/selector-wrong-field.out" 2>"$tmp_dir/selector-wrong-field.err"
then
	echo 'function graph selector accepted an out-of-telescope value field' >&2
	exit 1
fi

./read_file.out --write-artifact "$tmp_dir/function-graph-recursive-selector.apo" \
	"$block_binding" >"$tmp_dir/function-graph-recursive-selector.out"
grep -q '^function_graph_selector_group .* tailLength 7 2 3 1$' \
	"$tmp_dir/function-graph-recursive-selector.apo"
awk '
	$1 == "function_graph_selector_group" && $5 == "tailLength" && !done {
		$6 = 3
		$9 = 0
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/function-graph-recursive-selector.apo" \
	>"$tmp_dir/function-graph-selector-false-recursive.apo"
if ./read_file.out --read-graph \
	"$tmp_dir/function-graph-selector-false-recursive.apo" \
	>"$tmp_dir/selector-false-recursive.out" \
	2>"$tmp_dir/selector-false-recursive.err"
then
	echo 'function graph selector accepted a false recursion marker' >&2
	exit 1
fi

awk '
	$1 == "function_graph_selector_group" && $5 == "tailLength" && !done {
		$7 = 0
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$tmp_dir/function-graph-recursive-selector.apo" \
	>"$tmp_dir/function-graph-selector-swapped-value.apo"
if ./read_file.out --read-graph \
	"$tmp_dir/function-graph-selector-swapped-value.apo" \
	>"$tmp_dir/selector-swapped-value.out" \
	2>"$tmp_dir/selector-swapped-value.err"
then
	echo 'function graph selector accepted a same-typed field swap' >&2
	exit 1
fi

awk '
	BEGIN { in_graph_constructor = 0; premise = 0 }
	/^derivation 11 / { in_graph_constructor = 1 }
	in_graph_constructor && /^premise claim / {
		premise++
		if (premise == 4) {
			$3 = 0
			in_graph_constructor = 0
		}
	}
	{ print }
' "$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-wrong-premise.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-wrong-premise.apo" \
	>"$tmp_dir/wrong-premise.out" 2>"$tmp_dir/wrong-premise.err"
then
	echo 'wrong recursive graph premise unexpectedly read' >&2
	exit 1
fi

sed -E \
	's/^(proposition 7 1 1 81) 0 /\1 1 /' \
	"$tmp_dir/function-graph.apo" >"$tmp_dir/function-graph-wrong-context.apo"
if ./read_file.out --read-graph "$tmp_dir/function-graph-wrong-context.apo" \
	>"$tmp_dir/wrong-context.out" 2>"$tmp_dir/wrong-context.err"
then
	echo 'wrong function graph Claim Context unexpectedly read' >&2
	exit 1
fi

prototype_test_phase cbpv_negative
./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_computation_index.p \
	>"$tmp_dir/pure-index.out"
grep -q '^term bad :=' "$tmp_dir/pure-index.out"

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_unknown_owner.p \
	>"$tmp_dir/unknown-owner.out" 2>"$tmp_dir/unknown-owner.err"
then
	echo 'function_graph_unknown_owner unexpectedly compiled' >&2
	exit 1
fi

if ./read_file.out \
	src/prototype/tests/fixtures/negative/function_graph_coarse_forgery.p \
	>"$tmp_dir/coarse.out" 2>"$tmp_dir/coarse.err"
then
	echo 'forgeable coarse function graph unexpectedly compiled' >&2
	exit 1
fi
grep -q 'source shape is not structure-preserving' "$tmp_dir/coarse.err"

for negative in nonfunction_owner effectful_owner ambiguous_owner \
	higher_order_variable
do
	if ./read_file.out \
		"src/prototype/tests/fixtures/negative/function_graph_${negative}.p" \
		>"$tmp_dir/${negative}.out" 2>"$tmp_dir/${negative}.err"
	then
		echo "function_graph_${negative} unexpectedly compiled" >&2
		exit 1
	fi
done
grep -q 'accepted definition view unavailable' "$tmp_dir/nonfunction_owner.err"
grep -q 'accepted definition graph fragment rejected' "$tmp_dir/effectful_owner.err"
grep -q 'assignments=2' "$tmp_dir/ambiguous_owner.err"
grep -q 'induction hypothesis must refer to a match-case binder' \
	"$tmp_dir/higher_order_variable.err"

prototype_test_phase static_boundary
if rg -n \
	'PROTOTYPE_(TERM|JUDGEMENT_PROOF)_FUNCTION_GRAPH|FUNCTION_GRAPH_(TYPE|WITNESS)_FORMER' \
	src/prototype/include/a_program/core \
	src/prototype/include/a_program/kernel \
	src/prototype/src/core \
	src/prototype/src/kernel
then
	echo 'function graph leaked into Core or kernel proof tags' >&2
	exit 1
fi
grep -q 'PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS' \
	src/prototype/tests/checks/hott/universe_scaffold.inc
if rg -n 'FUNCTION_GRAPH' \
	src/prototype/src/identity \
	src/prototype/src/parametricity \
	src/prototype/src/dimension
then
	echo 'function graph introduced a special HOTT action' >&2
	exit 1
fi

prototype_test_phase_finish
echo 'function graph certified execution tests passed'
