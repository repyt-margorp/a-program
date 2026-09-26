#!/usr/bin/env bash
set -euo pipefail

plain=$1
linked=$2
compare=$3
root=$(dirname "${BASH_SOURCE[0]}")
provider="$root/fixtures/sorted-proof-provider.p"
theorem="$root/acceptance/generic-quick-sorted.p"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

nm "$plain" > "$directory/plain-symbols"
nm "$linked" > "$directory/linked-symbols"
if grep -Eq 'pg_function_graph_(witness|packet)' "$directory/plain-symbols"; then
	printf '%s\n' 'ordinary compiler still links the optional witness generator' >&2
	exit 1
fi
grep -q 'pg_function_graph_witness_advance' "$directory/linked-symbols"

check() {
	local expected=$1 label=$2
	shift 2
	local code=0
	"$binary" --steps 10000000 "$@" > "$directory/status" || code=$?
	if [[ $code != "$expected" ]]; then
		printf '%s: expected exit %s, got %s\n' "$label" "$expected" "$code" >&2
		cat "$directory/status" >&2
		exit 1
	fi
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
		4) grep -q '^unsupported steps=' "$directory/status" ;;
		*) exit 1 ;;
	esac
}

base='Nat:=@{zero:*;succ:*->*;}; f:=\n:Nat=>n @zero=>Nat.zero @succ k=>Nat.succ *k;'
for binary in "$plain" "$linked"; do
	# Semantic negatives must reach their typed boundary, not removed syntax.
	for fixture in generated-function-witness-mismatch generated-function-graph-direct-forgery \
		generated-packet-wrong-output generated-packet-shadow generated-packet-imitation \
		function-graph-callable-parameter-wrong generated-function-graph-parameter-mismatch; do
		sed '/^bad :=/,$d' "$root/acceptance/$fixture.p" | check 0 "$fixture-positive" -
		check 1 "$fixture-negative" "$root/acceptance/$fixture.p"
	done
	sed 's/@cons { tailLength; }/@nil => Nat.zero @cons { tailLength; }/' \
		"$root/acceptance/function-graph-missing-case.p" |
		check 0 complete-graph-cases -
	check 1 missing-graph-case "$root/acceptance/function-graph-missing-case.p"
	for suffix in 'bad:=*f;' 'alias:=f; bad:=*alias;' \
		'bad:=*(f);' 'bad:=\x:Nat->Nat=>*x;' \
		'alias:=&f; bad:=*alias;' 'bad:=*(Nat.succ);' \
		'bad:=\n:Nat=>n @zero=>Nat.zero @succ k=>(\k:Nat=>*k) k;'; do
		printf '%s\n%s\n' "$base" "$suffix" | check 1 global-star -
	done
	printf '%s\n' "$base" 'bad:=*Nat.succ;' | check 1 qualified-star -
	printf '%s\n' "$base" 'graph:=@f; main:=f (Nat.succ Nat.zero);' | check 0 local-ih -
	printf '%s\n' "$base" 'shadow:=\n:Nat=>n @zero=>Nat.zero @succ f=>Nat.succ *f;' | check 0 shadowed-ih -
	printf '%s\n' "$base" 'Vec:=\A:@=>@\n:Nat=>{nil:* Nat.zero;cons:A->* n->* (Nat.succ n);};' | check 0 indexed-self -
	printf '%s\n' 'import quickSort; bad:=*quickSort;' |
		check 1 imported-star --legacy-intrinsic-dot --imports "$provider" -

	printf '%s\n' "$base" 'bad:=*f;' |
		check 3 pending-star --steps 0 --save "$directory/rejected.a" -
	check 1 restored-star --load "$directory/rejected.a"
	check 1 stored-star --load "$directory/rejected.a" --save "$directory/rejected-complete.a"
	check 1 reloaded-star --load "$directory/rejected-complete.a"

	check 0 general-sorted --legacy-intrinsic-dot --imports "$provider" "$theorem"
	check 3 pending-sorted --legacy-intrinsic-dot --imports "$provider" --steps 100 \
		--save "$directory/pending.a" "$theorem"
	check 0 resumed-sorted --load "$directory/pending.a"
	check 0 retained-sorted --legacy-intrinsic-dot --imports "$provider" \
		--retain-reductions --save "$directory/retained.a" "$theorem"
	check 0 reloaded-sorted --load "$directory/retained.a"
	check 3 inert-sorted --load --steps 0 --retain-reductions \
		--save "$directory/resaved.a" "$directory/retained.a"
	cmp "$directory/retained.a" "$directory/resaved.a"

	# The same general theorem must not be retyped as Sorted of the input.
	sed '/^quick_correct ::/s/(general_sorted A R) output;/(general_sorted A R) xs;/' "$theorem" |
		check 1 wrong-general-index --legacy-intrinsic-dot --imports "$provider" -

	printf '%s\n' \
		'import Nat; import List; import quickSort; import natLessOrEqual;' \
		'one:=Nat.succ Nat.zero; two:=Nat.succ one;' \
		'input:=(List Nat).cons two ((List Nat).cons one (List Nat).nil);' \
		'expected:=(List Nat).cons one ((List Nat).cons two (List Nat).nil);' \
		'main:=quickSort Nat (&natLessOrEqual) input;' |
		check 0 ordinary-quicksort --legacy-intrinsic-dot --imports "$provider" --save "$directory/run.a" -
	"$compare" --equal-image "$directory/run.a" main expected
done
printf '%s\n' 'function witness isolation: no global star; IH/Self, general Sorted, execution and images preserved'
