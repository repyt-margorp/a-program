#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?recursive inert test}")
if [[ -n ${4:-} ]]; then
	output=$4
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT
	output="$temporary/report"
fi
here=$(cd "$(dirname "$0")" && pwd)
legacy="$here/../predicate_integer"
cc=${CC:-cc}
ar=${AR:-ar}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"
mkdir -p "$output"
output=$(realpath "$output")
expect_status() {
	local name=$1 expected=$2 actual=0
	shift 2
	printf '%q ' "$@" > "$output/$name.command"
	printf '\n' >> "$output/$name.command"
	"$@" > "$output/$name.out" 2> "$output/$name.err" || actual=$?
	printf '%s\t%s\t%s\n' "$name" "$expected" "$actual" >> "$output/status.tsv"
	if [[ $actual != "$expected" ]]; then
		cat "$output/$name.out" "$output/$name.err" >&2
		printf '%s: expected %s, got %s\n' "$name" "$expected" "$actual" >&2
		exit 1
	fi
}
expect_status admit-legacy 0 "$compiler" --save "$output/legacy.a" "$legacy/fixture.p"
sha256sum "$output/legacy.a" > "$output/legacy-before.sha256"
sed -e '/^export /d' -e "s|artifact predicates.a|artifact $output/legacy.a|" "$legacy/predicate.aplink" > "$output/legacy-prefix.aplink"
for family in standalone reexport; do
	imports=() options=()
	fixture="$here/fixture.p"
	if [[ $family == reexport ]]; then
		fixture="$here/imported_fixture.p"
		imports=(--imports "$legacy/fixture.p")
		options=(-e 's/enum32 Bool Bool/enum32 public_bool Bool/' -e 's/data Numbers32 Numbers32/data public_numbers32 Numbers32/' -e 's/data Numbers64 Numbers64/data public_numbers64 Numbers64/')
		for name in known_keep32 known_drop32 known_keep64 known_drop64 captured_filter32 captured_filter64 captured_select32 captured_select64 length32 length64; do
			options+=(-e "s/export $name $name/export public_$name $name/")
		done
	fi
	expect_status "$family-admit" 0 "$compiler" "${imports[@]}" --save "$output/$family.a" "$fixture"
	sha256sum "$output/$family.a" > "$output/$family-before.sha256"
	expect_status "$family-reference" 0 "$compiler" "${imports[@]}" --run reference "$fixture"
	expect_status "$family-core" 0 "$compiler" --load --run reference "$output/$family.a"
	expect_status "$family-core-compare" 0 cmp "$output/$family-reference.out" "$output/$family-core.out"
	for product in source object archive; do
		sed -e "s|artifact recursive.a|artifact $output/$family.a|" -e "s/product source/product $product/" "${options[@]}" "$here/recursive.aplink" > "$output/$family-$product.aplink"
		directory="$output/$family-$product"
		expect_status "$family-emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/$family-$product.aplink" "$directory"
		test ! -e "$directory/runtime.c"
		if [[ $product != source ]]; then
			expect_status "$family-header-$product" 0 cmp "$output/$family-source/component.h" "$directory/component.h"
		fi
		case "$product" in
		source) input="$directory/component.c" ;;
		object) input="$directory/component.o" ;;
		archive) input="$directory/library.a" ;;
		esac
		expect_status "$family-compile-$product" 0 "$cc" "${flags[@]}" -I"$directory" "$here/client.c" "$input" -Wl,--wrap=malloc -o "$output/$family-client-$product"
		expect_status "$family-run-$product" 0 "$output/$family-client-$product"
		expect_status "$family-compare-$product" 0 cmp "$output/$family-run-$product.out" "$output/$family-reference.out"
	done
	expect_status "$family-inert" 0 "$inert" "$output/$family-source.aplink" "$output/$family-raw.c" "$output/$family-raw.h" "$output/$family-oracle.c"
	expect_status "$family-raw-header" 0 cmp "$output/$family-raw.h" "$output/$family-source/component.h"
	expect_status "$family-raw-compile" 0 "$cc" "${flags[@]}" -I"$output/$family-source" "$here/client.c" "$output/$family-raw.c" -Wl,--wrap=malloc -o "$output/$family-raw-client"
	expect_status "$family-raw-run" 0 "$output/$family-raw-client"
	expect_status "$family-raw-compare" 0 cmp "$output/$family-raw-run.out" "$output/$family-reference.out"
	expect_status "$family-oracle-compile" 0 "$cc" "${flags[@]}" -I"$output/$family-source" "$output/$family-oracle.c" "$output/$family-source/component.c" -o "$output/$family-oracle"
	expect_status "$family-oracle-run" 0 "$output/$family-oracle"
	expect_status "$family-repeated" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/$family-source.aplink" "$output/$family-repeated"
	expect_status "$family-deterministic" 0 diff -ru "$output/$family-source" "$output/$family-repeated"
	expect_status "$family-trusted" 0 "$backend" --cc "$cc" --ar "$ar" --trust-image --steps 0 --link "$output/$family-source.aplink" "$output/$family-trusted"
	expect_status "$family-trusted-source" 0 cmp "$output/$family-source/component.c" "$output/$family-trusted/component.c"
	expect_status "$family-trusted-header" 0 cmp "$output/$family-source/component.h" "$output/$family-trusted/component.h"
	expect_status "$family-no-fuel" 3 "$backend" --steps 0 --link "$output/$family-source.aplink" "$output/$family-no-fuel"
	test ! -e "$output/$family-no-fuel"
	for name in demanded_effect dynamic_map; do
		sed '/^export /d' "$output/$family-source.aplink" > "$output/$family-$name.aplink"
		printf 'export %s %s\n' "$name" "$name" >> "$output/$family-$name.aplink"
		expect_status "$family-$name" 4 "$backend" --link "$output/$family-$name.aplink" "$output/$family-$name"
		test ! -e "$output/$family-$name"
		test ! -s "$output/$family-$name.out"
	done
	sha256sum "$output/$family.a" > "$output/$family-after.sha256"
	expect_status "$family-image-unchanged" 0 cmp "$output/$family-before.sha256" "$output/$family-after.sha256"
done
expect_status family-source-compare 0 cmp "$output/standalone-reference.out" "$output/reexport-reference.out"
expect_status family-header-compare 0 cmp "$output/standalone-source/component.h" "$output/reexport-source/component.h"
for name in known_keep32 known_drop32 known_keep64 known_drop64 captured_filter32 captured_filter64 captured_select32 captured_select64; do
	cp "$output/legacy-prefix.aplink" "$output/legacy-$name.aplink"
	printf 'export %s %s\n' "$name" "$name" >> "$output/legacy-$name.aplink"
	expect_status "legacy-$name" 0 "$backend" --link "$output/legacy-$name.aplink" "$output/legacy-$name"
done
for name in apply32 choose32 apply64 choose64 reverse32 unused32 filter32 select32 filter64 select64 ternary mixed_width tri_result scalar_result returned effect callable_identity; do
	cp "$output/legacy-prefix.aplink" "$output/legacy-$name.aplink"
	printf 'export %s %s\n' "$name" "$name" >> "$output/legacy-$name.aplink"
	for mode in checked trusted; do
		options=()
		[[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$mode" 4 "$backend" "${options[@]}" --link "$output/legacy-$name.aplink" "$output/$name-$mode"
		test ! -e "$output/$name-$mode"
		test ! -s "$output/$name-$mode.out"
	done
done
sha256sum "$output/"*.a > "$output/images.sha256"
sha256sum "$output/legacy.a" > "$output/legacy-after.sha256"
expect_status legacy-image-unchanged 0 cmp "$output/legacy-before.sha256" "$output/legacy-after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Recursive known captures: standalone/reexport products, sixteen source/readback observations, 200 separate Core Nat comparisons/family, signed maps, resources and retained dynamic/effect/callable refusals pass\n'
