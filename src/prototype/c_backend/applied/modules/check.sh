#!/usr/bin/env bash
set -euo pipefail
products=${1:?completed applied-List gate directory}
output=${2:?new combined-module evidence directory}
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"
mkdir -p "$output"
inputs=("$products/applied-list.a")
while IFS= read -r -d '' file; do inputs+=("$file"); done < <(
	find "$products/numbers-source" "$products/numbers-object" "$products/numbers-archive" \
		"$products/flags-source" "$products/flags-object" "$products/flags-archive" \
		-maxdepth 1 -type f -print0 | sort -z
)
sha256sum "${inputs[@]}" > "$output/before.sha256"
cp "$products/numbers-source/component.h" "$output/numbers.h"
cp "$products/flags-source/component.h" "$output/flags.h"
for numbers in source object archive; do
	case "$numbers" in
	source) number_input="$products/numbers-source/component.c" ;;
	object) number_input="$products/numbers-object/component.o" ;;
	archive) number_input="$products/numbers-archive/library.a" ;;
	esac
	for booleans in source object archive; do
		case "$booleans" in
		source) flag_input="$products/flags-source/component.c" ;;
		object) flag_input="$products/flags-object/component.o" ;;
		archive) flag_input="$products/flags-archive/library.a" ;;
		esac
		for order in numbers-first flags-first; do
			defines=()
			if [[ $order == flags-first ]]; then defines=(-DFLAGS_FIRST); fi
			name="$numbers-$booleans-$order"
			"$cc" "${flags[@]}" "${defines[@]}" -I"$output" "$here/client.c" \
				"$number_input" "$flag_input" -o "$output/$name"
			"$output/$name" > "$output/$name.log" 2>&1
		done
	done
done
if "$cc" "${flags[@]}" -I"$output" -c "$here/refuse_nominal_client.c" \
	-o "$output/refused.o" > "$output/refuse-nominal.log" 2>&1; then
	printf 'incompatible nominal node pointers unexpectedly compiled\n' >&2
	exit 1
fi
grep -Eiq 'incompatible.*pointer' "$output/refuse-nominal.log"
test ! -e "$output/refused.o"
sha256sum "${inputs[@]}" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Mixed C products: 9 source/object/archive pairs, both header orders, 1093 rows per client; incompatible nominal call refused; products/image unchanged\n'
