#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
products=${2:?completed applied-family evidence directory}
output=${3:?new two-Nat-module evidence directory}
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"
mkdir -p "$output"
inputs=("$backend" "$products/applied-list.a" "$products/numbers-source.aplink"
	"$products/choice-source.aplink")
while IFS= read -r -d '' file; do inputs+=("$file"); done < <(
	find "$products/numbers-source" "$products/numbers-object" "$products/numbers-archive" \
		"$products/choice-source" "$products/choice-object" "$products/choice-archive" \
		-maxdepth 1 -type f -print0 | sort -z
)
sha256sum "${inputs[@]}" > "$output/before.sha256"
for module in numbers choice; do
	if [[ $module == numbers ]]; then alias=NumbersNat; else alias=ChoiceNat; fi
	for product in source object archive; do
		sed "s/nat32 Nat Nat/nat32 Nat $alias/; s/product source/product $product/" \
			"$products/$module-source.aplink" > "$output/$module-$product.aplink"
		grep -qx "nat32 Nat $alias" "$output/$module-$product.aplink"
		grep -qx "product $product" "$output/$module-$product.aplink"
		"$backend" --link "$output/$module-$product.aplink" "$output/$module-$product" \
			> "$output/$module-$product.log" 2>&1
	done
done
cp "$output/numbers-source/component.h" "$output/numbers.h"
cp "$output/choice-source/component.h" "$output/choice.h"
for numbers in source object archive; do
	case "$numbers" in
	source) number_input="$output/numbers-source/component.c" ;;
	object) number_input="$output/numbers-object/component.o" ;;
	archive) number_input="$output/numbers-archive/library.a" ;;
	esac
	for choice in source object archive; do
		case "$choice" in
		source) choice_input="$output/choice-source/component.c" ;;
		object) choice_input="$output/choice-object/component.o" ;;
		archive) choice_input="$output/choice-archive/library.a" ;;
		esac
		for order in numbers-first choice-first; do
			defines=()
			if [[ $order == choice-first ]]; then defines=(-DCHOICE_FIRST); fi
			name="$numbers-$choice-$order"
			"$cc" "${flags[@]}" "${defines[@]}" -I"$output" "$here/client.c" \
				"$number_input" "$choice_input" -o "$output/$name"
			"$output/$name" > "$output/$name.log" 2>&1
		done
	done
done
if "$cc" -r "$products/numbers-object/component.o" "$products/choice-object/component.o" \
	-o "$output/duplicate.o" > "$output/duplicate.log" 2>&1; then
	printf 'duplicate default arena symbol unexpectedly linked\n' >&2
	exit 1
fi
grep -q ap_arena_Nat_destroy "$output/duplicate.log"
test ! -e "$output/duplicate.o"
sha256sum "${inputs[@]}" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Two Nat32 components: distinct helper aliases, nine product pairs/both header orders, shared-arena rollback/depth/lifetime and duplicate-symbol refusal pass; inputs unchanged\n'
