#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=${2:-$(dirname "$binary")/program_test}
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

check() {
	local expected=$1 label=$2 code=0
	shift 2
	"$binary" --steps 5000000 "$@" > "$directory/status" || code=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	[[ $code == "$expected" ]]
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}

provider="$directory/provider.p"
cat "$root/fixtures/sorted-proof-provider.p" > "$provider"
sed '/^import /d' "$root/fixtures/local-strong-sorted.p" \
	"$root/fixtures/quick-sort-proof-common.p" >> "$provider"
cp "$provider" "$directory/base.p"
local_proof="$root/acceptance/generic-quick-local-sorted-result.p"
strong_proof="$root/acceptance/generic-quick-strong-sorted-result.p"
check 0 local-result --legacy-intrinsic-dot --imports "$provider" --save "$directory/local.a" "$local_proof"
sed '/^import /d' "$local_proof" >> "$provider"
check 0 strong-result --legacy-intrinsic-dot --imports "$provider" --save "$directory/strong.a" "$strong_proof"
for mode in local strong; do
	check 0 "$mode-reloaded" --load "$directory/$mode.a"
done
check 1 wrong-conversion --legacy-intrinsic-dot --imports "$provider" "$root/fixtures/sortedness-wrong-conversion.p"

cycle="$directory/cycle.p"
cat "$root/fixtures/sortedness-cycle.p" > "$cycle"
check 0 cyclic-comparator --legacy-intrinsic-dot --imports "$provider" --save "$directory/cycle.a" "$cycle"
for pair in result:expected certified:expected empty_certified:empty \
	singleton_certified:singleton duplicates_certified:duplicates; do
	"$compare" --steps 50000000 --equal-image "$directory/cycle.a" "${pair%:*}" "${pair#*:}"
done
for negative in edge strong; do
	cat "$cycle" "$root/fixtures/sortedness-cycle-wrong-$negative.p" > "$directory/wrong-$negative.p"
	# Expose exactly the same predicates to the negative consumer.
	sed -i '1i import general_all_from;' "$directory/wrong-$negative.p"
	check 1 "wrong-$negative" --legacy-intrinsic-dot --imports "$provider" "$directory/wrong-$negative.p"
done

for kind in local strong; do
	case $kind in
		local) proof=$local_proof; imports="$directory/base.p"; predicate=general_locally_sorted ;;
		strong) proof=$strong_proof; imports=$provider; predicate=general_strongly_sorted ;;
	esac
	# Only the final post-check changes: the output certificate cannot certify input.
	sed "\$s/$predicate A R (quickSort A (\&le) xs);/$predicate A R xs;/" "$proof" > "$directory/wrong-result.p"
	if cmp -s "$proof" "$directory/wrong-result.p"; then exit 1; fi
	check 1 "$kind-wrong-result" --legacy-intrinsic-dot --imports "$imports" "$directory/wrong-result.p"
	for mode in ordinary retained; do
		options=()
		if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
		check 0 "$kind-$mode-complete" --legacy-intrinsic-dot --imports "$imports" \
			"${options[@]}" --save "$directory/full.a" "$proof"
		check 0 "$kind-$mode-load" --load "$directory/full.a"
		check 3 "$kind-$mode-inert-resave" --load --steps 0 "${options[@]}" \
			--save "$directory/copy.a" "$directory/full.a"
		cmp "$directory/full.a" "$directory/copy.a"
		for steps in 0 100; do
			check 3 "$kind-$mode-pending-$steps" --legacy-intrinsic-dot --imports "$imports" \
				--steps "$steps" "${options[@]}" --save "$directory/pending.a" "$proof"
			check 0 "$kind-$mode-resumed-$steps" --load "$directory/pending.a"
			check 3 "$kind-$mode-pending-wrong-$steps" --legacy-intrinsic-dot --imports "$imports" \
				--steps "$steps" "${options[@]}" --save "$directory/wrong.a" "$directory/wrong-result.p"
			check 1 "$kind-$mode-resumed-wrong-$steps" --load "$directory/wrong.a"
		done
	done
done

# Share declarations, not conclusions: the old direct proof remains independent.
sed '/^import /d' "$strong_proof" "$root/acceptance/generic-quick-sorted-result.p" >> "$provider"
printf '%s\n' 'import Bool;' 'import List;' 'import general_decision;' \
	'import general_locally_sorted;' 'import general_strongly_sorted;' \
	'import strongly_sorted_to_locally_sorted;' 'import quick_strongly_sorted;' \
	'import quick_correct_existing;' 'import quickSort;' > "$directory/consumer.p"
cat "$root/fixtures/generic_sorted/boolean-order.p" "$root/fixtures/sortedness-strong-consumer.p" >> "$directory/consumer.p"
check 0 strong-compatibility --legacy-intrinsic-dot --imports "$provider" --save "$directory/consumer.a" "$directory/consumer.p"
for pair in new_value:expected old_value:expected local_value:expected empty_value:nil singleton_value:singleton; do
	"$compare" --steps 50000000 --equal-image "$directory/consumer.a" "${pair%:*}" "${pair#*:}"
done
printf '%s\n' 'local/strong sortedness: general proofs, cyclic separation, legacy contracts and images passed'
