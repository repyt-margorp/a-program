#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
if [[ -n ${3:-} ]]; then
	output=$3
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT
	output="$temporary/report"
fi
here=$(cd "$(dirname "$0")" && pwd)
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
expect_status admit 0 "$compiler" --save "$output/predicates.a" "$here/fixture.p"
sha256sum "$output/predicates.a" > "$output/before.sha256"
expect_status reference-known 0 "$compiler" --run reference_known "$here/fixture.p"
expect_status reference-direct 0 "$compiler" --run reference_direct "$here/fixture.p"
expect_status equivalent-source 0 cmp "$output/reference-known.out" "$output/reference-direct.out"
expect_status reference-core 0 "$compiler" --load --run reference_direct "$output/predicates.a"
expect_status equivalent-core 0 cmp "$output/reference-direct.out" "$output/reference-core.out"
for product in source object archive; do
	sed -e "s/product source/product $product/" -e "s|artifact predicates.a|artifact $output/predicates.a|" "$here/predicate.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/$product.aplink" "$output/$product"
	test ! -e "$output/$product/runtime.c"
	if [[ $product != source ]]; then
		expect_status "header-$product" 0 cmp "$output/source/component.h" "$output/$product/component.h"
	fi
	case "$product" in
	source) input="$output/source/component.c" ;;
	object) input="$output/object/component.o" ;;
	archive) input="$output/archive/library.a" ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" -I"$output/$product" "$here/client.c" "$input" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "run-$product" 0 "$output/client-$product"
	expect_status "compare-$product" 0 cmp "$output/run-$product.out" "$output/reference-direct.out"
done
expect_status repeated 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/source.aplink" "$output/repeated"
expect_status deterministic 0 diff -ru "$output/source" "$output/repeated"
expect_status trusted 0 "$backend" --cc "$cc" --ar "$ar" --trust-image --steps 0 --link "$output/source.aplink" "$output/trusted"
expect_status trusted-source 0 cmp "$output/source/component.c" "$output/trusted/component.c"
expect_status trusted-header 0 cmp "$output/source/component.h" "$output/trusted/component.h"
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"
test ! -e "$output/no-fuel"
for name in known_keep32 known_drop32 known_keep64 known_drop64 captured_filter32 captured_filter64 captured_select32 captured_select64 apply32 choose32 apply64 choose64 reverse32 unused32 filter32 select32 filter64 select64 ternary mixed_width tri_result scalar_result returned effect callable_identity; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"
	printf 'export %s %s\n' "$name" "$name" >> "$output/$name.aplink"
	for mode in checked trusted; do
		options=()
		[[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$mode" 4 "$backend" "${options[@]}" --link "$output/$name.aplink" "$output/$name-$mode"
		test ! -e "$output/$name-$mode"
		test ! -s "$output/$name-$mode.out"
	done
done
sha256sum "$output/predicates.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Existing native ABI: 341 lists/width/product, six source observations, all/none controls, resource rollback and 25 checked/trusted refusals pass\n'
