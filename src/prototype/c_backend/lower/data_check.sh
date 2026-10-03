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
expect_status 0 "$compiler" --save "$temporary/fixture.a" "$here/data_fixture.p"
sha256sum "$temporary/fixture.a" > "$temporary/before"
for product in source object archive executable; do
	{
		script "$product"
		printf 'enum32 Bool Bool\ndata Packet Packet\ndata Twin Twin\n'
		for name in small wide number large identity rebuild captured partial shadowed other constant; do
			printf 'export %s %s\n' "$name" "$name"
		done
		if [[ $product == executable ]]; then printf 'entry constant\n'; fi
	} > "$temporary/$product.aplink"
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	! grep -E 'ap_(apply|value|context)|malloc|calloc' "$temporary/$product/component.c"
	grep -q 'fieldful-tagged-values' "$temporary/$product/link.json"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	executable) expect_status 0 "$temporary/executable/program"; continue ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/data_client.c" "$input" -o "$temporary/client"
	expect_status 0 "$temporary/client"
done
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$temporary/source.aplink" "$temporary/no-fuel"
test ! -e "$temporary/no-fuel"
"$cc" "${flags[@]}" -I"$temporary/source" "$here/data_differential.c" "$temporary/source/component.c" -o "$temporary/differential"
expect_status 0 "$temporary/differential"
cp "$temporary/out" "$temporary/generated"
expect_status 0 "$compiler" --imports "$here/data_fixture.p" --run main "$here/data_differential.p"
cmp "$temporary/out" "$temporary/generated"
if printf '#include "component.h"\nint main(void) {struct ap_data_Packet p = {0}; struct ap_data_Twin t = p; return t.tag;}\n' |
	"$cc" "${flags[@]}" -I"$temporary/source" -x c -c -o "$temporary/bad.o" - > "$temporary/out" 2> "$temporary/err"; then exit 1; fi
for name in Recursive Dependent Nested Callback number; do
	{ script source; printf 'enum32 Bool Bool\ndata %s Rejected\nexport constant constant\n' "$name"; } > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for change in '/enum32 Bool/d' 's/data Twin Twin/data Packet Again/'; do
	sed "$change" "$temporary/source.aplink" > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for name in effect block_callback callback recursive; do
	{ script source; printf 'enum32 Bool Bool\ndata Packet Packet\ndata Recursive Recursive\nexport %s rejected\n' "$name"; } > "$temporary/bad.aplink"
	# Recursive selection is needed only for the recursive export.
	if [[ $name != recursive ]]; then sed -i '/data Recursive/d' "$temporary/bad.aplink"; fi
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
for change in 's/data Twin Twin/data Twin Packet/' 's/native_direct_v1/scalar_direct_v1/;s/c_native_v1/c_scalar_v1/'; do
	sed "$change" "$temporary/source.aplink" > "$temporary/bad.aplink"
	expect_status 2 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
sha256sum "$temporary/fixture.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
test -z "$(find "$temporary" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Native data C: constructors, fieldful Match, captures, input validation, products and evaluator agreement passed\n'
