#!/usr/bin/env bash
set -euo pipefail

binary=$1
retained=${2:-0}
root=$(dirname "${BASH_SOURCE[0]}")
fixtures="$root/fixtures/generic_sorted"
provider="$root/fixtures/sorted-proof-provider.p"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

check_result() {
	local expected=$1 path=$2
	shift 2
	local output code=0 status
	output=$("$binary" --steps 10000000 --legacy-intrinsic-dot "$@" "$path" 2>&1) || code=$?
	case $expected in
		0) status=done ;;
		1) status=rejected ;;
		3) status=pending ;;
		4) status=unsupported ;;
		*) return 1 ;;
	esac
	if [[ $code != "$expected" ]]; then
		printf '%s: expected exit %s, got %s: %s\n' "$path" "$expected" "$code" "$output" >&2
		return 1
	fi
	if [[ $output != "$status steps="* ]]; then
		printf '%s: unexpected result: %s\n' "$path" "$output" >&2
		return 1
	fi
}

for entry in box-fixed-cast:1 box-fixed:0 decision-no-check:0 decision-computed:0 \
	decision-graph-general:1 comparator-bridge-rejected:1 comparator-bridge-computed:0; do
	check_result "${entry#*:}" "$fixtures/${entry%:*}.p"
done
for entry in generic-conditional:0 generic-quick-original:1 generic-quick-projection-outside:1; do
	check_result "${entry#*:}" "$fixtures/${entry%:*}.p" --imports "$provider"
done

for entry in decision-explicit-result:0 decision-explicit-recomputed:0 \
	decision-explicit-wrong-arity:1 decision-explicit-wrong-comparator:1 \
	decision-explicit-wrong-index:1 decision-explicit-wrong-scope:1 \
	decision-explicit-effectful:4; do
	check_result "${entry#*:}" "$root/acceptance/${entry%:*}.p"
done
derived="$root/acceptance/lt-derived-lift.p"
check_result 0 "$root/acceptance/lt-derived-helper-graph-direct.p" --imports "$derived"
# Flip this to done when a specialized helper reuses its checked generic graph.
check_result 4 "$root/acceptance/lt-derived-helper-graph-shifted.p" --imports "$derived"
proof="$root/acceptance/generic-quick-sorted.p"
check_result 0 "$proof" --imports "$provider" --save "$directory/complete.a"
check_result 0 "$directory/complete.a" --load
if [[ $retained == 1 ]]; then
	check_result 0 "$proof" --imports "$provider" --save "$directory/retained.a" --retain-reductions
	check_result 0 "$directory/retained.a" --load
	code=0
	"$binary" --load --steps 0 --save "$directory/retained-resaved.a" \
		--retain-reductions "$directory/retained.a" > "$directory/retained-status" || code=$?
	[[ $code == 3 ]] && grep -qx 'pending steps=0' "$directory/retained-status"
	cmp "$directory/retained.a" "$directory/retained-resaved.a"
fi

for steps in 0 100; do
	code=0
	"$binary" --legacy-intrinsic-dot --steps "$steps" --imports "$provider" \
		--save "$directory/partial.a" "$proof" > "$directory/source-status" || code=$?
	[[ $code == 3 ]] && grep -qx "pending steps=$steps" "$directory/source-status"
	code=0
	"$binary" --load --steps 0 --save "$directory/resaved.a" \
		"$directory/partial.a" > "$directory/load-status" || code=$?
	[[ $code == 3 ]] && grep -qx 'pending steps=0' "$directory/load-status"
	cmp "$directory/partial.a" "$directory/resaved.a"
	check_result 0 "$directory/resaved.a" --load
	if [[ $retained == 1 ]]; then
		code=0
		"$binary" --legacy-intrinsic-dot --steps "$steps" --imports "$provider" \
			--save "$directory/partial-retained.a" --retain-reductions "$proof" \
			> "$directory/retained-source-status" || code=$?
		[[ $code == 3 ]] && grep -qx "pending steps=$steps" "$directory/retained-source-status"
		code=0
		"$binary" --load --steps 0 --save "$directory/retained-resaved.a" \
			--retain-reductions "$directory/partial-retained.a" \
			> "$directory/retained-load-status" || code=$?
		[[ $code == 3 ]] && grep -qx 'pending steps=0' "$directory/retained-load-status"
		cmp "$directory/partial-retained.a" "$directory/retained-resaved.a"
		check_result 0 "$directory/retained-resaved.a" --load
	fi
done

code=0
"$binary" --legacy-intrinsic-dot --steps 10000000 --save "$directory/invalid.a" \
	"$root/acceptance/decision-explicit-wrong-comparator.p" > "$directory/invalid-status" || code=$?
[[ $code == 1 ]] && grep -q '^rejected steps=' "$directory/invalid-status"
check_result 1 "$directory/invalid.a" --load
if [[ $retained == 1 ]]; then
	code=0
	"$binary" --legacy-intrinsic-dot --steps 10000000 --save "$directory/invalid-retained.a" \
		--retain-reductions "$root/acceptance/decision-explicit-wrong-comparator.p" \
		> "$directory/invalid-retained-status" || code=$?
	[[ $code == 1 ]] && grep -q '^rejected steps=' "$directory/invalid-retained-status"
	check_result 1 "$directory/invalid-retained.a" --load
fi
printf '%s\n' 'generic Sorted: audit boundary, explicit motives, complete proof and recompute images passed'
