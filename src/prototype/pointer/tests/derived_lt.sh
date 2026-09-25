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
	"$compare" --steps 50000000 --equal-image "$directory/resaved.a" "$1" "$2"
}

for variant in frozen derived frozen-tail-first derived-tail-first; do
	provider=${variant%-tail-first}
	# No fuzzy matching: source changes must trigger review of the experiment.
	cp "$root/fixtures/sorted-proof-provider.p" "$directory/provider.p"
	cp "$root/acceptance/generic-quick-sorted.p" "$directory/proof.p"
	cp "$root/fixtures/generic_sorted/content-proof.p" "$directory/content.p"
	if [[ $provider == derived ]]; then
		patch --batch --silent --fuzz=0 -p1 -d "$directory" < "$root/fixtures/generic_sorted/derived-lt.patch"
	fi
	if [[ $variant != "$provider" ]]; then
		patch --batch --silent --fuzz=0 -p1 -d "$directory" < "$root/fixtures/generic_sorted/partition-tail-first.patch"
		# The graph records recursive evidence before comparison evidence now.
		# The old field order must not silently acquire the new meaning.
		check_status 1 --legacy-intrinsic-dot --imports "$directory/provider.p" "$directory/proof.p"
		grep -q '^rejected steps=' "$directory/status"
		sed -E -i 's/^(\t@case[12] k h t) comparison( l left r right lb rb rest)( lifted lifting)? =>$/\1\2 comparison\3 =>/' \
			"$directory/proof.p" "$directory/content.p"
	fi
	# The direct theorem shares the provider's nominal predicates. Its private
	# lemma names stay in the import provider, outside the graph proof's scope.
	if [[ $provider == derived ]]; then
		sed '/^import /d; s/LT\.lift /ltLift /g' "$root/acceptance/generic-quick-sorted-result.p" >> "$directory/provider.p"
	else
		sed '/^import /d' "$root/acceptance/generic-quick-sorted-result.p" >> "$directory/provider.p"
	fi
	cat "$directory/content.p" "$root/fixtures/generic_sorted/content-result-proof.p" \
		"$root/fixtures/generic_sorted/boolean-consumer.p" \
		"$root/fixtures/generic_sorted/boolean-content-consumer.p" >> "$directory/proof.p"
	printf '%s\n' "LT provider/partition order: $variant"
	check_status 0 --legacy-intrinsic-dot --imports "$directory/provider.p" \
		--save "$directory/complete.a" "$directory/proof.p"
	cp "$directory/complete.a" "$directory/resaved.a"
	for pair in empty_length:zero singleton_length:one ordered_length:two reverse_length:two \
		duplicates_length:four empty_value:nil singleton_value:singleton ordered_value:ordered \
		reverse_value:ordered duplicates_value:duplicates_expected direct_value:duplicates_expected \
		empty_content:nil singleton_content:singleton reverse_content:ordered \
		duplicates_content:duplicates_expected duplicate_origin:duplicates; do
		check_pair "${pair%:*}" "${pair#*:}"
	done

	# Check source execution as well as imported image consumers with the same
	# declarations. No nominal family or proof tree crosses between providers.
	# Preserve import scoping and exercise source Solve directly, without
	# serializing first or conflating the two sets of private lemma names.
	for pair in duplicates_value:duplicates_expected duplicates_length:four duplicates_content:duplicates_expected; do
		"$compare" --legacy-intrinsic-dot --steps 50000000 --equal-imports \
			"$directory/provider.p" "$directory/proof.p" "${pair%:*}" "${pair#*:}"
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
			check_pair duplicates_content duplicates_expected
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
printf '%s\n' 'derived LT: both providers and partition orders, universal Sorted/permutation witnesses, exact outputs, ordinary/retained partial images and invalid evidence passed'
