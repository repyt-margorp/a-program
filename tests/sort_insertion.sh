#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$2
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
provider="$root/fixtures/sorted-proof-provider.p"
proof="$root/acceptance/sort-insertion-property.p"
# The original provider is unchanged; the reviewed suffix only shares nominal
# predicates with the general ordinary-result proof.
expected=a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6
actual=$(sha256sum "$provider")
test "${actual%% *}" = "$expected"

# Preserve the exact historical provider, including its legacy #. spelling.
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$provider" --save "$directory/proof.a" "$proof"
"$compare" --packet-image "$directory/proof.a" insertNat expected_value one sample
for pair in main:three duplicate:three after:three empty:one packet_value:expected_value direct_value:expected_value; do
	"$compare" --equal-image "$directory/proof.a" "${pair%:*}" "${pair#*:}"
done
for steps in 0 100; do
	code=0
	"$binary" --legacy-intrinsic-dot --imports "$provider" --steps "$steps" --save "$directory/partial.a" "$proof" || code=$?
	test "$code" = 3
	code=0
	"$binary" --load --steps 0 --save "$directory/resaved.a" "$directory/partial.a" || code=$?
	test "$code" = 3
	cmp "$directory/partial.a" "$directory/resaved.a"
	"$compare" --equal-image "$directory/resaved.a" main three
done

# Assemble the same definitions without imports as a single exported module
# for the negative consumer; no alternate algorithm or order axiom is added.
cat "$provider" > "$directory/provider.p"
sed '/^import /d' "$proof" >> "$directory/provider.p"
code=0
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$directory/provider.p" "$root/acceptance/sort-insertion-property-wrong.p" || code=$?
test "$code" = 1
printf '%s\n' 'sort insertion: exact provider, universal graph/result properties, images and wrong result index passed'

# The generic helper graph is consumed directly, not identified with @insertNat.
proof="$root/acceptance/sort-insertion-sort-property.p"
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$directory/provider.p" --save "$directory/sort.a" "$proof"
"$compare" --packet-image "$directory/sort.a" insertionSort expected_value sample
for pair in main:four empty:zero singleton:one_value already:four packet_value:expected_value direct_value:expected_value; do
	"$compare" --equal-image "$directory/sort.a" "${pair%:*}" "${pair#*:}"
done
for steps in 0 100; do
	code=0
	"$binary" --legacy-intrinsic-dot --imports "$directory/provider.p" --steps "$steps" --save "$directory/partial.a" "$proof" || code=$?
	test "$code" = 3
	code=0
	"$binary" --load --steps 0 --save "$directory/resaved.a" "$directory/partial.a" || code=$?
	test "$code" = 3
	cmp "$directory/partial.a" "$directory/resaved.a"
	"$compare" --equal-image "$directory/resaved.a" main four
done
cat "$proof" > "$directory/wrong-sort.p"
sed '/^import /d' "$root/acceptance/sort-insertion-sort-property-wrong.p" >> "$directory/wrong-sort.p"
code=0
"$binary" --legacy-intrinsic-dot --steps 1000000 --imports "$directory/provider.p" "$directory/wrong-sort.p" || code=$?
test "$code" = 1
printf '%s\n' 'insertionSort: universal graph/result Sorted proofs, source/image agreement and wrong input-index rejection passed'

# Reuse the checked order lemmas, excluding their local execution examples.
sed '/^import read_sorted;/,$d; /^import /d' "$proof" >> "$directory/provider.p"
for sort in tree merge quick; do
	sort_provider="$directory/provider.p"
	comparison_steps=2000000
	if [ "$sort" = quick ]; then
		# Reuse the tree proof's general append/bounds lemmas, not its algorithm.
		sort_provider="$directory/quick-provider.p"
		cat "$directory/provider.p" > "$sort_provider"
		sed -n '/^AllTo :=/,/^list_lower :=/{ /^list_lower :=/!p; }' "$root/acceptance/sort-tree-property.p" >> "$sort_provider"
		sed -n '/^le_refl :=/p' "$root/acceptance/sort-tree-property.p" >> "$sort_provider"
		# Share the general ordinary-result theorem, whose helper names have
		# their own prefix; keep the older Nat graph theorem as a separate test.
		sed '/^import /d' "$root/acceptance/generic-quick-sorted-result.p" >> "$sort_provider"
		# This also executes the instantiated general proof, not only QuickSort.
		comparison_steps=50000000
	fi
	proof="$root/acceptance/sort-$sort-property.p"
	"$binary" --legacy-intrinsic-dot --steps "$comparison_steps" --imports "$sort_provider" --save "$directory/$sort.a" "$proof"
	case "$sort" in
		tree) function=treeSort; arguments=(Nat '&natLessOrEqual' sample) ;;
		merge) function=mergeSort; arguments=('&natLessOrEqual' sample) ;;
		quick) function=quickSort; arguments=(Nat '&natLessOrEqual' sample) ;;
	esac
	"$compare" --steps "$comparison_steps" --packet-image "$directory/$sort.a" "$function" expected_value "${arguments[@]}"
	for pair in main:four empty:zero singleton:one_value already:four reversed:four packet_value:expected_value direct_value:expected_value; do
		"$compare" --steps "$comparison_steps" --equal-image "$directory/$sort.a" "${pair%:*}" "${pair#*:}"
	done
	for steps in 0 100; do
		code=0
		"$binary" --legacy-intrinsic-dot --imports "$sort_provider" --steps "$steps" --save "$directory/partial.a" "$proof" || code=$?
		test "$code" = 3
		code=0
		"$binary" --load --steps 0 --save "$directory/resaved.a" "$directory/partial.a" || code=$?
		test "$code" = 3
		cmp "$directory/partial.a" "$directory/resaved.a"
		"$compare" --steps "$comparison_steps" --equal-image "$directory/resaved.a" main four
	done
	for negative in "$root/acceptance/sort-$sort-"*wrong.p; do
		cat "$proof" > "$directory/valid-sort.p"
		sed '/^wrong :=/,$d; /^import /d' "$negative" >> "$directory/valid-sort.p"
		"$binary" --legacy-intrinsic-dot --steps "$comparison_steps" --imports "$sort_provider" "$directory/valid-sort.p"
		cat "$proof" > "$directory/wrong-sort.p"
		sed '/^import /d' "$negative" >> "$directory/wrong-sort.p"
		code=0
		"$binary" --legacy-intrinsic-dot --steps "$comparison_steps" --imports "$sort_provider" "$directory/wrong-sort.p" || code=$?
		test "$code" = 1
		for steps in 0 100; do
			code=0
			"$binary" --legacy-intrinsic-dot --steps "$steps" --imports "$sort_provider" --save "$directory/invalid.a" "$directory/wrong-sort.p" || code=$?
			test "$code" = 3
			code=0
			"$binary" --load --steps 0 --save "$directory/resaved.a" "$directory/invalid.a" || code=$?
			test "$code" = 3
			cmp "$directory/invalid.a" "$directory/resaved.a"
			code=0
			"$binary" --load --steps "$comparison_steps" "$directory/resaved.a" || code=$?
			test "$code" = 1
		done
	done
	printf '%s\n' "$sort sort: universal Sorted proof, execution witnesses, images and invalid claims passed"
done
