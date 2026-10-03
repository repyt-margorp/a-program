#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
oracle=${3:?transitive capture Oracle fixture generator}
here=$(cd "$(dirname "$0")" && pwd)
fixtures="$here/../fixtures"
evidence=$(mktemp -d)
trap 'rm -rf "$evidence"' EXIT
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
expect_status() {
	local expected=$1 status=0
	shift
	"$@" > "$evidence/out" 2> "$evidence/err" || status=$?
	if [[ $status != "$expected" ]]; then
		cat "$evidence/out" "$evidence/err" >&2
		printf 'expected %s, got %s: %s\n' "$expected" "$status" "$*" >&2
		exit 1
	fi
}
"$oracle" "$evidence/oracle.c"
"$cc" "${flags[@]}" "$evidence/oracle.c" -o "$evidence/oracle"
"$evidence/oracle"
artifact="$evidence/transitive-functions.a"
expect_status 0 "$compiler" --save "$artifact" "$fixtures/transitive_functions.p"
sha256sum "$artifact" > "$evidence/before.sha256"
for product in source object archive; do
	sed "s/product source/product $product/" "$fixtures/transitive_functions.aplink" > "$evidence/$product.aplink"
	expect_status 0 "$backend" --link "$evidence/$product.aplink" "$evidence/$product"
	test ! -e "$evidence/$product/runtime.c"
	case "$product" in
	source) input="$evidence/source/component.c" ;;
	object) input="$evidence/object/component.o" ;;
	archive) input="$evidence/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$evidence/$product" "$here/transitive_function_client.c" "$input" -o "$evidence/client-$product"
	expect_status 0 "$evidence/client-$product"
	if [[ $product == source ]]; then cp "$evidence/out" "$evidence/generated"; fi
done
expect_status 0 "$compiler" --imports "$fixtures/transitive_functions.p" --run main "$fixtures/transitive_functions_differential.p"
cmp "$evidence/out" "$evidence/generated"
expect_status 0 "$backend" --link "$evidence/source.aplink" "$evidence/repeated"
diff -ru "$evidence/source" "$evidence/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$evidence/source.aplink" "$evidence/trusted"
cmp "$evidence/source/component.c" "$evidence/trusted/component.c"
cmp "$evidence/source/component.h" "$evidence/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$evidence/source.aplink" "$evidence/no-fuel"
test ! -e "$evidence/no-fuel"
for name in dynamic_callback demanded_effect; do
	sed '/^export /d' "$fixtures/transitive_functions.aplink" > "$evidence/$name.aplink"
	printf 'export %s %s\n' "$name" "$name" >> "$evidence/$name.aplink"
	expect_status 4 "$backend" --link "$evidence/$name.aplink" "$evidence/$name"
	test ! -e "$evidence/$name"
	test ! -s "$evidence/out"
	cp "$evidence/err" "$evidence/$name-refusal.log"
done
sha256sum "$artifact" > "$evidence/after.sha256"
cmp "$evidence/before.sha256" "$evidence/after.sha256"
printf 'Transitive static captures: 600 Int32 and 7 Int64 cases/product, 6 source observations, deterministic checked/trusted output, callback/effect refusals pass\n'
