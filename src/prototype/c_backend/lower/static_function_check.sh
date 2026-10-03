#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
oracle=${3:?static function Oracle fixture generator}
here=$(cd "$(dirname "$0")" && pwd)
fixtures="$here/../fixtures"
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
"$oracle" "$temporary/oracle.c"
"$cc" "${flags[@]}" "$temporary/oracle.c" -o "$temporary/oracle"
"$temporary/oracle"
expect_status() {
	local expected=$1 status=0
	shift
	"$@" > "$temporary/out" 2> "$temporary/err" || status=$?
	if [[ $status != "$expected" ]]; then
		cat "$temporary/out" "$temporary/err" >&2
		printf 'expected %s, got %s: %s\n' "$expected" "$status" "$*" >&2
		exit 1
	fi
}
expect_status 0 "$compiler" --save "$temporary/static-functions.a" "$fixtures/static_functions.p"
sha256sum "$temporary/static-functions.a" > "$temporary/before"
cp "$fixtures/static_functions.aplink" "$temporary/source.aplink"
for name in captured_block shadowed_block repeated_block curried_block unused_block captured64 nested_two unused_effect; do
	printf 'export %s %s\n' "$name" "$name" >> "$temporary/source.aplink"
done
for product in source object archive; do
	if [[ $product != source ]]; then
		sed "s/product source/product $product/" "$temporary/source.aplink" > "$temporary/$product.aplink"
	fi
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/static_function_client.c" "$input" -o "$temporary/client"
	expect_status 0 "$temporary/client"
	if [[ $product == source ]]; then cp "$temporary/out" "$temporary/generated"; fi
done
expect_status 0 "$compiler" --imports "$fixtures/static_functions.p" --run main "$fixtures/static_functions_differential.p"
cmp "$temporary/out" "$temporary/generated"
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$temporary/source.aplink" "$temporary/no-fuel"
test ! -e "$temporary/no-fuel"
for name in dynamic_callback effect_block nested_three; do
	sed "s/export closed_block closed_block/export $name $name/" "$fixtures/static_functions.aplink" > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
sha256sum "$temporary/static-functions.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
printf 'Native static functions: 260 Int32 and 5 Int64 cases per product, source differential and retained callback/effect/capture refusals passed\n'
bash "$here/static_native_check.sh" "$backend" "$compiler"
