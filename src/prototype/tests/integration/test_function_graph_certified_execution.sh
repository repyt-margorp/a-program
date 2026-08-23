#!/bin/sh
set -eu

# Boundary audit: ISSUE-13-FUNCTION-GRAPH-RESULT-EVIDENCE

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader >/dev/null

positive=src/prototype/tests/fixtures/typing/function_graph_generated_length_check.p
model=src/prototype/tests/fixtures/typing/function_graph_certified_length_model.p
two_recursive=src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p
dependent_spine=src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p
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
			'inspectQuickSortAcc := \LeGraph : (left : Nat) -> (right : Nat) -> Bool -> @ =>' \
			'\size : Nat => \access : Acc Nat LT size =>' \
			'\input : SizedList Nat size => \output : List Nat =>' \
			'\graph : @quickSortAcc Nat LeGraph size access input output =>' \
			'graph' \
			'@nil current down => output' \
			'@cons tailSize pivot tail current down lowerSize lower upperSize upper' \
			'lowerBound upperBound lowerOutput lowerGraph upperOutput upperGraph' \
			'result appendGraph => output;'
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
./read_file.out --read-graph "$tmp_dir/quicksort-graph.apo" \
	>"$tmp_dir/quicksort-graph-read.out"
grep -q '^interface term \$certified\.quickSort ' \
	"$tmp_dir/quicksort-graph-read.out"

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
