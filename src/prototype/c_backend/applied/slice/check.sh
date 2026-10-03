#!/usr/bin/env bash
set -euo pipefail
backend=${1:?applied a-to-c binary}
compiler=${2:?pointer-check binary}
output=${3:?new slice evidence directory}
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"
mkdir -p "$output"
"$compiler" --save "$output/fixture.a" "$here/fixture.p" > "$output/admission.log" 2>&1
sha256sum "$output/fixture.a" > "$output/before.sha256"
for product in source object archive; do
	sed "s|artifact fixture.a|artifact $output/fixture.a|; s/product source/product $product/" \
		"$here/source.aplink" > "$output/$product.aplink"
	"$backend" --link "$output/$product.aplink" "$output/$product" > "$output/$product-emission.log" 2>&1
	test ! -e "$output/$product/runtime.c"
	! grep -E 'ap_(apply|value|context)|pg_eval' "$output/$product/component.c"
	case "$product" in
	source) input="$output/source/component.c" ;;
	object) input="$output/object/component.o" ;;
	archive) input="$output/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$output/$product" "$here/client.c" "$input" \
		-Wl,--wrap=malloc -o "$output/client-$product"
	"$output/client-$product" > "$output/$product-observations" 2> "$output/$product-client.log"
done
cmp "$output/source-observations" "$output/object-observations"
cmp "$output/source-observations" "$output/archive-observations"
"$compiler" --imports "$here/fixture.p" --run main "$here/differential.p" \
	> "$output/interpreter-observations" 2> "$output/interpreter.log"
cmp "$output/source-observations" "$output/interpreter-observations"
"$backend" --link "$output/source.aplink" "$output/repeated" > "$output/repeated.log" 2>&1
diff -ru "$output/source" "$output/repeated"
"$backend" --trust-image --steps 0 --link "$output/source.aplink" "$output/trusted" \
	> "$output/trusted.log" 2>&1
cmp "$output/source/component.c" "$output/trusted/component.c"
cmp "$output/source/component.h" "$output/trusted/component.h"
sha256sum "$output/fixture.a" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Native source List take/drop/slice: 1093 Lists, five take/drop limits and 25 slice pairs each per product; nine full-payload source observations; allocation/depth/tag/null rollback and deterministic checked/trusted emission pass\n'
