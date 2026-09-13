#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
fixture=$1
binary=$2
compare=$3
"$fixture" nominal-write "$directory/multiple.a"
code=0
"$binary" --load --steps 0 --save "$directory/unsolved.a" "$directory/multiple.a" > "$directory/status" || code=$?
test "$code" = 3
cmp "$directory/multiple.a" "$directory/unsolved.a"
# Solve can discover additional module inputs; only an unsolved resave is byte-identical.
"$binary" --load --root 4 --save "$directory/saved.a" "$directory/multiple.a" > "$directory/status"
# The selected root does not erase the separately rejected root or its inputs.
code=0
"$binary" --load --root 2 "$directory/saved.a" > "$directory/status" || code=$?
test "$code" = 1
grep -q '^rejected steps=' "$directory/status"
"$fixture" nominal-read "$directory/saved.a"
code=0
"$binary" --load --root 9 "$directory/multiple.a" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
grep -q 'root index out of range: 9 (count 8)' "$directory/status"
example="$(dirname "${BASH_SOURCE[0]}")/../../../../examples/09_list_induction.p"
"$binary" --nf main --save "$directory/list.a" "$example" > "$directory/source-nf"
"$binary" --load --nf main "$directory/list.a" > "$directory/image-nf"
# Solve scheduling may differ; the fully reduced computation graph must not.
sed '1d' "$directory/source-nf" > "$directory/source-value"
sed '1d' "$directory/image-nf" > "$directory/image-value"
cmp "$directory/source-value" "$directory/image-value"

# Retention selects raw reduction data, never accepted flags or host effects.
"$binary" --nf main --retain-reductions --save "$directory/retained.a" "$example" > "$directory/retained-nf"
sed '1d' "$directory/retained-nf" > "$directory/retained-value"
cmp "$directory/source-value" "$directory/retained-value"
"$fixture" retention-check "$directory/retained.a" > "$directory/retained-summary"
grep -Eq '^retained=1 reductions=[1-9][0-9]* phases=[0-9]+ steps=0$' "$directory/retained-summary"
code=0
"$binary" --load --steps 0 --retain-reductions --save "$directory/retained-resaved.a" "$directory/retained.a" > "$directory/status" || code=$?
test "$code" = 3
"$fixture" retention-summary "$directory/retained-resaved.a" > "$directory/resaved-summary"
cmp "$directory/retained-summary" "$directory/resaved-summary"
"$binary" --load --nf main "$directory/retained-resaved.a" > "$directory/retained-loaded-nf"
sed '1d' "$directory/retained-loaded-nf" > "$directory/retained-loaded-value"
cmp "$directory/source-value" "$directory/retained-loaded-value"
code=0
"$binary" --load --steps 0 --save "$directory/discarded.a" "$directory/retained.a" > "$directory/status" || code=$?
test "$code" = 3
"$fixture" retention-summary "$directory/discarded.a" > "$directory/discarded-summary"
grep -q '^retained=0 reductions=0 phases=0 steps=0$' "$directory/discarded-summary"
code=0
"$binary" --steps 0 --retain-reductions --save "$directory/empty-retained.a" "$example" > "$directory/status" || code=$?
test "$code" = 3
"$fixture" retention-summary "$directory/empty-retained.a" > "$directory/empty-summary"
grep -q '^retained=1 reductions=0 phases=0 steps=0$' "$directory/empty-summary"
printf ':save %s\n:quit\n' "$directory/repl-retained.a" |
	"$binary" --load --steps 0 --retain-reductions --repl "$directory/retained.a" > "$directory/status"
"$fixture" retention-summary "$directory/repl-retained.a" > "$directory/resaved-summary"
cmp "$directory/retained-summary" "$directory/resaved-summary"
code=0
"$binary" --retain-reductions "$example" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
printf '%s\n' 'image cli: explicit reduction retention, inert resave, REPL policy and default discard passed'

# A failed write must not truncate a previously usable image, even in place.
cp "$directory/list.a" "$directory/list-before.a"
code=0
(
	trap '' XFSZ
	ulimit -f 1
	"$binary" --load --save "$directory/list.a" "$directory/list.a"
) > "$directory/status" 2>&1 || code=$?
test "$code" = 2
grep -q 'cannot save input image' "$directory/status"
cmp "$directory/list-before.a" "$directory/list.a"
if compgen -G "$directory/list.a.tmp.*" > /dev/null; then exit 1; fi
"$binary" --load --save "$directory/list.a" "$directory/list.a" > "$directory/status"
"$binary" --load --nf main "$directory/list.a" > "$directory/image-nf"
sed '1d' "$directory/image-nf" > "$directory/image-value"
cmp "$directory/source-value" "$directory/image-value"
# Failure to publish (a directory target) also removes the temporary file.
mkdir "$directory/target"
code=0
"$binary" --load --save "$directory/target" "$directory/list.a" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
test -d "$directory/target"
if compgen -G "$directory/target.tmp.*" > /dev/null; then exit 1; fi
printf '%s\n' 'image cli: failed writes preserve existing images; in-place publication and cleanup passed'
printf '%s\n' 'image cli: multi-root selection, retained obligations and range rejection passed'
printf '%s\n' 'image cli: parameterized List source/image NF agreement passed'

# Retaining syntax does not require its type-level computations to have finished.
# Compare admission with direct Solve without treating unsupported as accepted.
for family in closed-family open-family; do
	input="$(dirname "${BASH_SOURCE[0]}")/acceptance/$family.p"
	direct=0
	"$binary" "$input" > "$directory/direct" || direct=$?
	case "$direct" in 0|4) ;; *) exit 1 ;; esac
	for steps in 0 100; do
		code=0
		"$binary" --steps "$steps" --save "$directory/family.a" "$input" > "$directory/status" || code=$?
		test "$code" = 3
		grep -q '^pending steps=' "$directory/status"
		code=0
		"$binary" --load --steps 0 --save "$directory/family-resaved.a" "$directory/family.a" > "$directory/status" || code=$?
		test "$code" = 3
		grep -q '^pending steps=0$' "$directory/status"
		code=0
		"$binary" --load "$directory/family-resaved.a" > "$directory/restored" || code=$?
		test "$code" = "$direct"
		cut -d ' ' -f 1 "$directory/direct" > "$directory/direct-status"
		cut -d ' ' -f 1 "$directory/restored" > "$directory/restored-status"
		cmp "$directory/direct-status" "$directory/restored-status"
	done
	if test "$family" = closed-family; then test "$direct" = 0; fi
done
printf '%s\n' 'image cli: unfinished family inputs preserve direct Solve admission after unsolved resave'

# Generated graph families are rebuilt by the same Solve, including aliases
# appearing in independently synthesized annotations. No graph proof flag loads.
while read -r fixture names; do
	input="$(dirname "${BASH_SOURCE[0]}")/$fixture"
	for steps in 0 100 100000; do
		code=0
		"$binary" --steps "$steps" --save "$directory/generated.a" "$input" > "$directory/status" || code=$?
		if test "$steps" = 100000; then test "$code" = 0; else test "$code" = 3; fi
		code=0
		"$binary" --load --steps 0 --save "$directory/generated-resaved.a" "$directory/generated.a" > "$directory/status" || code=$?
		test "$code" = 3
		# A bare fixture checks source admission, not NF of an unapplied function.
		if test -z "$names"; then
			"$binary" --load "$directory/generated-resaved.a" > "$directory/restored"
		fi
		for name in $names; do
			expected=expected
			if [[ "$name" == *:* ]]; then expected=${name#*:}; name=${name%%:*}; fi
			"$compare" --equal-image "$directory/generated-resaved.a" "$name" "$expected"
		done
		if test "$fixture" = acceptance/generated-function-graph-direct.p; then
			if "$compare" --equal-image "$directory/generated-resaved.a" certified other; then exit 1; fi
		fi
	done
done <<'GRAPHS'
acceptance/length-output-proof.p main emptyMain:emptyExpected
acceptance/generated-function-graph.p main certifiedMain aliasMain proofMain directMain directProof shadowMain:baseExpected nestedProof:baseExpected
acceptance/generated-function-graph-direct.p main certified
acceptance/function-graph-named-fields.p main aliasMain graphMain valueMain
acceptance/order-reflexivity.p main certified observed:two zeroCertified:zeroExpected
acceptance/dependent-graph-motive.p main reorderedMain certifiedMain
acceptance/function-graph-call-sites.p main unusedMain:unusedExpected orderedMain:orderedExpected propertyMain:propertyExpected cutMain:cutExpected
acceptance/motive-computed-callee.p main
acceptance/recursive-dependent-package.p main
acceptance/dependent-order-reflexivity.p main
acceptance/dependent-function-motive.p main dependentMain
acceptance/indexed-payload.p main otherMain dependentMain mixedMain mixedOther:boolExpected functionMain
acceptance/indexed-dependent-environment.p main originalMain dependentMain zeroMain:zeroExpected pointMain:zeroExpected
acceptance/indexed-rigid-refutation.p main boolMain:boolExpected emptyMain:emptyExpected functionMain secondIndexMain
acceptance/indexed-field-transport.p main pairMain
acceptance/indexed-result-transport.p main otherMain bothMain functionMain functionOtherMain
acceptance/induction-index-environment.p main readMain:two
acceptance/acc-accessible-successor.p main
acceptance/indexed-block-demands.p main nestedMain selectedMain readMain:two
acceptance/indexed-ih-environment.p
acceptance/computed-family-member.p main nested:nestedExpected
../../tests/fixtures/typing/function_graph_dependent_spine_check.p main certified
../../tests/fixtures/typing/function_graph_two_recursive_calls_check.p main certified
GRAPHS
printf '%s\n' 'image cli: generated graph source aliases and normal forms survive unfinished/completed resaves'
