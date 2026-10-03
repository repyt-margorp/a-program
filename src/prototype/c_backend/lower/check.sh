#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
oracle=${3:?native Oracle fixture generator}
here=$(cd "$(dirname "$0")" && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"

"$oracle" "$temporary/oracle.c" "$temporary/enum.c" "$temporary/data.c" "$temporary/list.c" "$temporary/natural.c"
"$cc" "${flags[@]}" "$temporary/oracle.c" -o "$temporary/oracle"
"$temporary/oracle"
"$cc" "${flags[@]}" "$temporary/enum.c" -o "$temporary/enum"
"$temporary/enum"
"$cc" "${flags[@]}" "$temporary/data.c" -o "$temporary/data"
"$temporary/data"
"$cc" "${flags[@]}" "$temporary/list.c" -o "$temporary/list"
"$temporary/list"
"$cc" "${flags[@]}" "$temporary/natural.c" -o "$temporary/natural"
"$temporary/natural"

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
	printf 'aplink 1\nartifact fixture.a\nabi c_scalar_v1\ntarget host-c11\nproduct %s\nlowering scalar_direct_v1\nfallback reject\n' "$1"
}

exports() {
	for name in add sub mul neg add64 sub64 mul64 neg64 identity ignore composed nested partial builtin through_fold nested_capture shadowed scoped sequence captured constant computed; do
		printf 'export %s %s\n' "$name" "$name"
	done
	printf 'export add alias\n'
}

expect_status 0 "$compiler" --save "$temporary/fixture.a" "$here/fixture.p"
sha256sum "$temporary/fixture.a" > "$temporary/before"
for product in source object archive executable; do
	{ script "$product"; exports; } > "$temporary/$product.aplink"
	if [[ $product == executable ]]; then printf 'entry computed\n' >> "$temporary/$product.aplink"; fi
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -s "$temporary/out"
	test ! -e "$temporary/$product/runtime.c"
	test ! -e "$temporary/$product/runtime.h"
	! grep -E 'ap_(apply|function|value|context)|malloc|calloc' "$temporary/$product/component.c"
	grep -q '"runtime_abi": null' "$temporary/$product/link.json"
	grep -q '"lowering": "scalar_direct_v1"' "$temporary/$product/link.json"
	if [[ $product == executable ]]; then
		expect_status 0 "$temporary/executable/program"
	else
		case "$product" in
		source) input="$temporary/source/component.c" ;;
		object) input="$temporary/object/component.o" ;;
		archive) input="$temporary/archive/library.a" ;;
		esac
		"$cc" "${flags[@]}" -I"$temporary/$product" "$here/client.c" "$input" -o "$temporary/client"
		expect_status 0 "$temporary/client"
	fi
	test ! -s "$temporary/out"
done
test "$(ar t "$temporary/archive/library.a")" = component.o
test "$(nm -g --defined-only "$temporary/object/component.o" | wc -l)" = 23
test -z "$(nm -u "$temporary/object/component.o")"
first=$(sed -n '/int ap_export_add(/,+3p' "$temporary/source/component.c" | grep 'result = c')
alias=$(sed -n '/int ap_export_alias(/,+3p' "$temporary/source/component.c" | grep 'result = c')
test "$first" = "$alias"
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
"$cc" "${flags[@]}" -I"$temporary/source" "$here/differential.c" "$temporary/source/component.c" -o "$temporary/differential"
expect_status 0 "$temporary/differential"
cp "$temporary/out" "$temporary/generated"
expect_status 0 "$compiler" --imports "$here/fixture.p" --run main "$here/differential.p"
cmp "$temporary/out" "$temporary/generated"

# Independent components exchange ordinary integers, not interpreter handles.
{ script archive; printf 'export sub other\n'; } > "$temporary/other.aplink"
expect_status 0 "$backend" --link "$temporary/other.aplink" "$temporary/other"
"$cc" "${flags[@]}" -DLINK_OTHER_COMPONENT -include "$temporary/other/component.h" -I"$temporary/archive" \
	"$here/client.c" "$temporary/archive/library.a" "$temporary/other/library.a" -o "$temporary/combined"
expect_status 0 "$temporary/combined"

# Reject unsupported representations without secretly invoking a boxed fallback.
for name in effect ignored_effect thunk_arg text higher; do
	{ script source; printf 'export add valid\nexport %s rejected\n' "$name"; } > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
{ script executable; printf 'export add add\nentry add\n'; } > "$temporary/bad.aplink"
expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
test ! -e "$temporary/bad"
for change in 's/abi c_scalar_v1/abi isolated_v1/' '/fallback/d' 's/fallback reject/fallback boxed/' \
	's/lowering scalar_direct_v1/lowering unknown/' 's/lowering scalar_direct_v1/lowering structural_v1/'; do
	sed "$change" "$temporary/source.aplink" > "$temporary/bad.aplink"
	expect_status 2 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for directive in 'lowering scalar_direct_v1' 'fallback reject'; do
	cp "$temporary/source.aplink" "$temporary/bad.aplink"
	printf '%s\n' "$directive" >> "$temporary/bad.aplink"
	expect_status 2 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
done
expect_status 3 "$compiler" --steps 0 --save "$temporary/pending.a" "$here/fixture.p"
sed 's/fixture.a/pending.a/' "$temporary/source.aplink" > "$temporary/pending.aplink"
expect_status 3 "$backend" --trust-image --link "$temporary/pending.aplink" "$temporary/pending"
test ! -e "$temporary/pending"
if "$cc" "${flags[@]}" -DAP_C_SCALAR_ABI=2 -I"$temporary/source" -c "$here/client.c" -o "$temporary/bad.o" > "$temporary/out" 2> "$temporary/err"; then exit 1; fi
grep -q incompatible_A_Program_scalar_ABI "$temporary/err"
sha256sum "$temporary/fixture.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
test -z "$(find "$temporary" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Native scalar C: fixed-width inputs/results, calls, sequencing, products and explicit rejection passed\n'
