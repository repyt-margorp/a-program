#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
fixtures="$here/../fixtures"
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
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
expect_status 0 "$compiler" --save "$temporary/static-native.a" "$fixtures/static_native.p"
sha256sum "$temporary/static-native.a" > "$temporary/before"
for product in source object archive; do
	sed "s/product source/product $product/" "$fixtures/static_native.aplink" > "$temporary/$product.aplink"
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	grep -q 'checked-nat32' "$temporary/$product/link.json"
	grep -q 'array-to-list-copy' "$temporary/$product/link.json"
	! grep -E 'ap_(apply|value|context)|pg_eval' "$temporary/$product/component.c"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/static_native_client.c" "$input" -Wl,--wrap=malloc -o "$temporary/client"
	expect_status 0 "$temporary/client"
	mkdir "$temporary/modules-$product"
	cp -R "$temporary/$product" "$temporary/modules-$product/left"
	sed 's/nat32 Nat Nat/nat32 Nat OtherNat/; s/data Numbers Numbers/data Numbers OtherNumbers/; s/^export \([^ ]*\) \([^ ]*\)$/export \1 other_\2/' "$temporary/$product.aplink" > "$temporary/other.aplink"
	expect_status 0 "$backend" --link "$temporary/other.aplink" "$temporary/modules-$product/right"
	case "$product" in
	source) other="$temporary/modules-$product/right/component.c" ;;
	object) other="$temporary/modules-$product/right/component.o" ;;
	archive) other="$temporary/modules-$product/right/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/modules-$product" "$here/static_native_modules.c" "$input" "$other" -Wl,--wrap=malloc -o "$temporary/modules-client"
	expect_status 0 "$temporary/modules-client"
done
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$temporary/source.aplink" "$temporary/no-fuel"
test ! -e "$temporary/no-fuel"
sha256sum "$temporary/static-native.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
printf 'Native static functions: source/object/archive captures, Nat32 overflow, depth, allocation rollback and two-module C use passed\n'
