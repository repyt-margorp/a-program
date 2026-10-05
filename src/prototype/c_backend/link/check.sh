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
	local product=$1
	printf 'aplink 1\nartifact "image with spaces.a"\nabi isolated_v1\ntarget host-c11\nproduct %s\n' "$product"
	printf 'export "first" first\nexport second second\nexport first duplicate\nexport noop noop\n'
}

mkdir "$temporary/scripts"
expect_status 0 "$compiler" --save-materialized "$temporary/scripts/image with spaces.a" "$here/fixture.p"
sha256sum "$temporary/scripts/image with spaces.a" > "$temporary/before"
: > "$temporary/reference"
for entry in first second first noop; do
	expect_status 0 "$compiler" --run "$entry" --load "$temporary/scripts/image with spaces.a"
	cat "$temporary/out" >> "$temporary/reference"
done
for product in source object archive executable; do
	script "$product" > "$temporary/scripts/$product.aplink"
	if [[ $product == executable ]]; then printf 'entry second\n' >> "$temporary/scripts/$product.aplink"; fi
	expect_status 0 "$backend" --link "$temporary/scripts/$product.aplink" "$temporary/$product"
	test ! -s "$temporary/out"
	test -s "$temporary/$product/component.c"
	test -s "$temporary/$product/component.h"
	test -s "$temporary/$product/link.json"
	if [[ $product == executable ]]; then
		expect_status 0 "$temporary/executable/program"
		printf 'B' > "$temporary/expected"
	else
		case "$product" in
		source) files=("$temporary/source/component.c" "$temporary/source/runtime.c") ;;
		object) files=("$temporary/object/component.o" "$temporary/object/runtime.o") ;;
		archive) files=("$temporary/archive/library.a") ;;
		esac
		"$cc" "${flags[@]}" -I"$temporary/$product" "$here/client.c" "${files[@]}" -o "$temporary/client"
		expect_status 0 "$temporary/client"
		cp "$temporary/reference" "$temporary/expected"
	fi
	cmp "$temporary/out" "$temporary/expected"
done

# First and duplicate share the exact emitted tN, not just equivalent results.
first=$(sed -n '/int ap_export_first(void)/,+3p' "$temporary/source/component.c" | grep 'return run_entry')
duplicate=$(sed -n '/int ap_export_duplicate(void)/,+3p' "$temporary/source/component.c" | grep 'return run_entry')
test "$first" = "$duplicate"
test "$(nm -g --defined-only "$temporary/object/component.o" | wc -l)" = 4
! nm -g --defined-only "$temporary/object/component.o" | grep -E ' (main|t[0-9]+)$'
expect_status 0 "$backend" --link "$temporary/scripts/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 2 "$backend" --link "$temporary/scripts/source.aplink" "$temporary/source"
ln -s "$temporary/scripts/image with spaces.a" "$temporary/alias"
expect_status 2 "$backend" --link "$temporary/scripts/source.aplink" "$temporary/alias"
expect_status 2 "$backend" --link "$temporary/scripts/source.aplink" "$temporary/scripts/source.aplink"
expect_status 2 "$backend" --cc /bin/false "$temporary/scripts/image with spaces.a" first "$temporary/ignored.c"

# One invocation-wide budget, never a fresh allowance for each selected name.
spent=$(sed -n 's/.*"validation_steps": \([0-9]*\).*/\1/p' "$temporary/source/link.json")
test "$spent" -gt 1
expect_status 3 "$backend" --steps "$((spent-1))" --link "$temporary/scripts/source.aplink" "$temporary/budget-short"
test ! -e "$temporary/budget-short"
expect_status 0 "$backend" --steps "$spent" --link "$temporary/scripts/source.aplink" "$temporary/budget-exact"
expect_status 3 "$backend" --steps "$spent" --revalidate-limit 0 --link "$temporary/scripts/source.aplink" "$temporary/zero"
test ! -e "$temporary/zero"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/scripts/source.aplink" "$temporary/trusted"
grep -q 'user-trusted-unauthenticated' "$temporary/trusted/link.json"
"$cc" "${flags[@]}" -I"$temporary/trusted" "$here/client.c" "$temporary/trusted/component.c" "$temporary/trusted/runtime.c" -o "$temporary/trusted-client"
expect_status 0 "$temporary/trusted-client"
printf '42B42' > "$temporary/expected"
cmp "$temporary/out" "$temporary/expected"

# Different components reuse local ordinal numbers but exchange no handles.
expect_status 0 "$compiler" --save-materialized "$temporary/scripts/other.a" "$here/other.p"
printf 'aplink 1\nartifact other.a\nabi isolated_v1\ntarget host-c11\nproduct archive\nexport other other\n' > "$temporary/scripts/other.aplink"
expect_status 0 "$backend" --link "$temporary/scripts/other.aplink" "$temporary/other"
"$cc" "${flags[@]}" -DLINK_OTHER_COMPONENT -include "$temporary/other/component.h" -I"$temporary/archive" "$here/client.c" "$temporary/archive/library.a" "$temporary/other/library.a" -o "$temporary/combined"
expect_status 0 "$temporary/combined"
printf '42B42C' > "$temporary/expected"
cmp "$temporary/out" "$temporary/expected"
if [[ -e /dev/full ]]; then
	"$cc" "${flags[@]}" -DLINK_FAILURE_TEST -I"$temporary/source" "$here/client.c" "$temporary/source/component.c" "$temporary/source/runtime.c" -o "$temporary/recover"
	expect_status 0 "$temporary/recover"
	printf '42B42B42' > "$temporary/expected"
	cmp "$temporary/out" "$temporary/expected"
	grep -q 'output error' "$temporary/err"
fi

# A native GNU/LLD augmenting script really participates in the link.
printf 'SECTIONS { .ap_link_marker : { BYTE(0x5a) } } INSERT AFTER .text;\n' > "$temporary/scripts/layout with spaces.ld"
cp "$temporary/scripts/executable.aplink" "$temporary/scripts/native.aplink"
printf 'native_script "layout with spaces.ld"\n' >> "$temporary/scripts/native.aplink"
expect_status 0 "$backend" --link "$temporary/scripts/native.aplink" "$temporary/native"
readelf -S "$temporary/native/program" > "$temporary/sections"
grep -q '.ap_link_marker' "$temporary/sections"
expect_status 0 "$temporary/native/program"
printf 'B' > "$temporary/expected"
cmp "$temporary/out" "$temporary/expected"
cp "$temporary/scripts/native.aplink" "$temporary/scripts/missing-native.aplink"
sed -i 's/layout with spaces.ld/missing.ld/' "$temporary/scripts/missing-native.aplink"
expect_status 2 "$backend" --link "$temporary/scripts/missing-native.aplink" "$temporary/missing-native"
test ! -e "$temporary/missing-native"
printf 'this is not a native linker script\n' > "$temporary/scripts/missing.ld"
expect_status 2 "$backend" --link "$temporary/scripts/missing-native.aplink" "$temporary/bad-native"
test ! -e "$temporary/bad-native"
(cd "$temporary"; expect_status 0 "$backend" --link scripts/executable.aplink -native-product)
expect_status 0 "$temporary/-native-product/program"
printf 'B' > "$temporary/expected"
cmp "$temporary/out" "$temporary/expected"

# Explicit failures never publish a directory or change input artifacts.
for tool in cc ar; do
	expect_status 2 "$backend" --"$tool" /bin/false --link "$temporary/scripts/archive.aplink" "$temporary/failed-$tool"
	test ! -e "$temporary/failed-$tool"
done
for directive in 'aplink 2' 'abi boxed' 'target unknown' 'product shared' 'trust true' 'export first first' 'export absent other' 'export function other' 'entry missing' 'native_script missing.ld' 'artifact other.a'; do
	script source > "$temporary/scripts/bad.aplink"
	printf '%s\n' "$directive" >> "$temporary/scripts/bad.aplink"
	case "$directive" in
	'export absent other') expected=1 ;;
	'export function other') expected=4 ;;
	*) expected=2 ;;
	esac
	expect_status "$expected" "$backend" --link "$temporary/scripts/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for suffix in 'export "unterminated' 'export first "bad\nname"' 'export first a b' 'export first "ok"trailing'; do
	script source > "$temporary/scripts/bad.aplink"
	printf '%s\n' "$suffix" >> "$temporary/scripts/bad.aplink"
	expect_status 2 "$backend" --link "$temporary/scripts/bad.aplink" "$temporary/bad"
done
script source > "$temporary/scripts/bad.aplink"
printf '\0' >> "$temporary/scripts/bad.aplink"
expect_status 2 "$backend" --link "$temporary/scripts/bad.aplink" "$temporary/bad"

expect_status 3 "$compiler" --steps 0 --save "$temporary/scripts/pending.a" "$here/fixture.p"
sed 's/image with spaces.a/pending.a/' "$temporary/scripts/source.aplink" > "$temporary/scripts/pending.aplink"
expect_status 3 "$backend" --steps 0 --link "$temporary/scripts/pending.aplink" "$temporary/pending"
expect_status 3 "$backend" --trust-image --link "$temporary/scripts/pending.aplink" "$temporary/pending"
expect_status 1 "$compiler" --save "$temporary/scripts/invalid.a" "$here/../fixtures/invalid-assert.p"
printf 'aplink 1\nartifact invalid.a\nabi isolated_v1\ntarget host-c11\nproduct source\nexport main main\n' > "$temporary/scripts/invalid.aplink"
expect_status 1 "$backend" --link "$temporary/scripts/invalid.aplink" "$temporary/invalid"
expect_status 3 "$backend" --trust-image --link "$temporary/scripts/invalid.aplink" "$temporary/invalid"

# Header mismatch must fail before a client can assume another calling ABI.
if "$cc" "${flags[@]}" -DAP_C_ISOLATED_ABI=2 -I"$temporary/source" -c "$here/client.c" -o "$temporary/bad.o" > "$temporary/out" 2> "$temporary/err"; then exit 1; fi
grep -q incompatible_A_Program_isolated_ABI "$temporary/err"
sha256sum "$temporary/scripts/image with spaces.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
test -z "$(find "$temporary" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'LinkerScript: shared exports, isolated ABI, native products/scripts, budgets and failure publication passed\n'
