#!/usr/bin/env bash
set -euo pipefail
binary=$1
compare=$(dirname "$binary")/program_test
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

check_status() {
	local expected=$1 code=0
	shift
	"$binary" --steps 10000000 "$@" > "$directory/status" || code=$?
	cat "$directory/status"
	[[ $code == "$expected" ]]
}

check_pair() {
	"$compare" --steps 10000000 --equal-image "$directory/resaved.a" "$1" "$2"
}

for provider in frozen derived; do
	# No fuzzy matching: source changes must trigger review of the experiment.
	cp "$root/fixtures/sorted-proof-provider.p" "$directory/provider.p"
	cp "$root/acceptance/generic-quick-sorted.p" "$directory/proof.p"
	if [[ $provider == derived ]]; then
		patch --batch --silent --fuzz=0 -p1 -d "$directory" < "$root/fixtures/generic_sorted/derived-lt.patch"
	fi
	cat "$root/fixtures/generic_sorted/boolean-consumer.p" >> "$directory/proof.p"
	printf '%s\n' "LT provider: $provider"
	check_status 0 --legacy-intrinsic-dot --imports "$directory/provider.p" \
		--save "$directory/complete.a" "$directory/proof.p"
	cp "$directory/complete.a" "$directory/resaved.a"
	for pair in empty_length:zero singleton_length:one ordered_length:two reverse_length:two \
		duplicates_length:four empty_value:nil singleton_value:singleton ordered_value:ordered \
		reverse_value:ordered duplicates_value:duplicates_expected direct_value:duplicates_expected; do
		check_pair "${pair%:*}" "${pair#*:}"
	done

	# Check source execution as well as imported image consumers with the same
	# declarations. No nominal family or proof tree crosses between providers.
	cat "$directory/provider.p" > "$directory/source.p"
	sed '/^import /d' "$directory/proof.p" >> "$directory/source.p"
	for pair in duplicates_value:duplicates_expected duplicates_length:four; do
		"$compare" --legacy-intrinsic-dot --steps 10000000 --equal "$directory/source.p" "${pair%:*}" "${pair#*:}"
	done

	for mode in ordinary retained; do
		options=()
		if [[ $mode == retained ]]; then options+=(--retain-reductions); fi
		for steps in 0 300000 10000000; do
			expected=3
			if [[ $steps == 10000000 ]]; then expected=0; fi
			check_status "$expected" --steps "$steps" --legacy-intrinsic-dot --imports "$directory/provider.p" \
				--save "$directory/saved.a" "${options[@]}" "$directory/proof.p"
			check_status 3 --load --steps 0 --save "$directory/resaved.a" "${options[@]}" "$directory/saved.a"
			grep -qx 'pending steps=0' "$directory/status"
			cmp "$directory/saved.a" "$directory/resaved.a"
			check_pair reverse_length two
			check_pair duplicates_value duplicates_expected
		done
		for negative in "$root/fixtures/generic_sorted/boolean-wrong-"*.p; do
			cat "$directory/proof.p" "$negative" > "$directory/invalid.p"
			check_status 1 --legacy-intrinsic-dot --imports "$directory/provider.p" "$directory/invalid.p"
			grep -q '^rejected steps=' "$directory/status"
			check_status 3 --steps 300000 --legacy-intrinsic-dot --imports "$directory/provider.p" \
				--save "$directory/invalid.a" "${options[@]}" "$directory/invalid.p"
			check_status 3 --load --steps 0 --save "$directory/resaved.a" "${options[@]}" "$directory/invalid.a"
			cmp "$directory/invalid.a" "$directory/resaved.a"
			check_status 1 --load "$directory/resaved.a"
			grep -q '^rejected steps=' "$directory/status"
		done
	done
done
printf '%s\n' 'derived LT: both providers, universal Sorted consumers, exact outputs, ordinary/retained partial images and invalid evidence passed'
