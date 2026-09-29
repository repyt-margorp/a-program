#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
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
script() {
	printf 'aplink 1\nartifact fixture.a\nabi c_native_v1\ntarget host-c11\nproduct %s\nlowering native_direct_v1\nfallback reject\n' "$1"
}
expect_status 0 "$compiler" --save "$temporary/fixture.a" "$here/enum_fixture.p"
sha256sum "$temporary/fixture.a" > "$temporary/before"
for product in source object archive executable; do
	{
		script "$product"
		printf 'enum32 Bool Bool\nenum32 Colour Colour\nenum32 Twin Twin\n'
		for name in negate to_int choose colour to_colour twin same nested curried through_fold fold_arg constant; do
			printf 'export %s %s\n' "$name" "$name"
		done
		if [[ $product == executable ]]; then printf 'entry constant\n'; fi
	} > "$temporary/$product.aplink"
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	! grep -E 'ap_(apply|value|context)|malloc|calloc' "$temporary/$product/component.c"
	grep -q 'switch (' "$temporary/$product/component.c"
	grep -q '"runtime_abi": null' "$temporary/$product/link.json"
	grep -q '"lowering": "native_direct_v1"' "$temporary/$product/link.json"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	executable) expect_status 0 "$temporary/executable/program"; continue ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/enum_client.c" "$input" -o "$temporary/client"
	expect_status 0 "$temporary/client"
done
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
"$cc" "${flags[@]}" -I"$temporary/source" "$here/enum_differential.c" "$temporary/source/component.c" -o "$temporary/differential"
expect_status 0 "$temporary/differential"
cp "$temporary/out" "$temporary/generated"
expect_status 0 "$compiler" --imports "$here/enum_fixture.p" --run main "$here/enum_differential.p"
cmp "$temporary/out" "$temporary/generated"

# Nominally distinct source types are not implicitly interchangeable C values.
if printf '#include "component.h"\nint main(void) {struct ap_enum_Bool b = {0}; struct ap_enum_Twin t = b; return t.tag;}\n' |
	"$cc" "${flags[@]}" -I"$temporary/source" -x c -c -o "$temporary/bad.o" - > "$temporary/out" 2> "$temporary/err"; then exit 1; fi
for name in number negate Empty Box Family; do
	{ script source; printf 'enum32 %s Rejected\nexport number value\n' "$name"; } > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for name in effect boxed; do
	{ script source; printf 'enum32 Bool Bool\nexport %s rejected\n' "$name"; } > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for change in '/enum32 Bool/d' 's/enum32 Twin Twin/enum32 Bool Again/'; do
	sed "$change" "$temporary/source.aplink" > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for change in 's/enum32 Twin Twin/enum32 Twin Bool/' 's/native_direct_v1/scalar_direct_v1/;s/c_native_v1/c_scalar_v1/'; do
	sed "$change" "$temporary/source.aplink" > "$temporary/bad.aplink"
	expect_status 2 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
sha256sum "$temporary/fixture.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
test -z "$(find "$temporary" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Native enum C: explicit representations, Match, captures, tag validation, products and artifact immutability passed\n'
