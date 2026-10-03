#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?inert callback checker}")
if [[ -n ${4:-} ]]; then
	output=$4
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT
	output="$temporary/report"
fi
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
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
no_product() {
	test ! -e "$1"
	local paths=("$1".tmp.*)
	test ! -e "${paths[0]}"
}
expect_status admit 0 "$compiler" --save "$output/callbacks.a" "$here/fixture.p"
sha256sum "$output/callbacks.a" > "$output/before.sha256"
for product in source object archive shared; do
	sed "s/product source/product $product/" "$here/callback.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --link "$output/$product.aplink" "$output/$product"
	grep -q 'same-width-unary-binary-int32-int64' "$output/$product/link.json"
	test ! -e "$output/$product/runtime.c"
	case "$product" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared") ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" -I"$output/$product" "$here/client.c" "${inputs[@]}" -o "$output/client-$product"
	expect_status "client-$product" 0 "$output/client-$product"
	cmp "$output/client-source.out" "$output/client-$product.out"
done
expect_status differential 0 "$compiler" --run reference "$here/fixture.p"
cmp "$output/differential.out" "$output/client-source.out"
expect_status inert 0 "$inert" "$output/source.aplink" "$output/inert.c" "$output/inert.h" "$output/oracle-client.c"
cmp "$output/inert.c" "$output/source/component.c"
cmp "$output/inert.h" "$output/source/component.h"
for product in source object archive shared; do
	case "$product" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared") ;;
	esac
	expect_status "compile-oracle-$product" 0 "$cc" "${flags[@]}" -I"$output/$product" "$output/oracle-client.c" "${inputs[@]}" -o "$output/oracle-$product"
	expect_status "oracle-$product" 0 "$output/oracle-$product"
done
for policy in repeated trusted; do
	options=()
	[[ $policy == trusted ]] && options=(--trust-image --steps 0)
	expect_status "$policy" 0 "$backend" "${options[@]}" --link "$output/source.aplink" "$output/$policy"
	cmp "$output/$policy/component.c" "$output/source/component.c"
	cmp "$output/$policy/component.h" "$output/source/component.h"
done
for name in ternary mixed_width mixed_result returned effect; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"
	printf 'export %s refused\n' "$name" >> "$output/$name.aplink"
	for policy in checked trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$policy" 4 "$backend" "${options[@]}" --link "$output/$name.aplink" "$output/$name-$policy"
		no_product "$output/$name-$policy"
	done
done
for profile in scalar native callback; do
	if [[ $profile == callback ]]; then lowering=callback; else lowering=$profile; fi
	sed -e "s/c_callback2_v1/c_${profile}_v1/" -e "s/callback2_direct_v1/${lowering}_direct_v1/" "$output/source.aplink" > "$output/old-$profile.aplink"
	for policy in checked trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "old-$profile-$policy" 4 "$backend" "${options[@]}" --link "$output/old-$profile.aplink" "$output/old-$profile-$policy"
		no_product "$output/old-$profile-$policy"
	done
done
cp "$output/source.aplink" "$output/selection.aplink"
printf 'nat32 Nat Nat\n' >> "$output/selection.aplink"
expect_status forbidden-selection 2 "$backend" --link "$output/selection.aplink" "$output/selection"
no_product "$output/selection"
sed -e '/^export /d' -e 's/c_callback2_v1/c_callback_v1/' -e 's/callback2_direct_v1/callback_direct_v1/' "$output/source.aplink" > "$output/legacy.aplink"
printf 'export subtract legacy_subtract\n' >> "$output/legacy.aplink"
expect_status emit-legacy 0 "$backend" --link "$output/legacy.aplink" "$output/legacy"
for order in forward reverse; do
	options=()
	[[ $order == reverse ]] && options=(-DREVERSE_HEADERS)
	expect_status "compile-headers-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/source" -I"$output" "$here/header_client.c" "$output/source/component.c" "$output/legacy/component.c" -o "$output/headers-$order"
	expect_status "headers-$order" 0 "$output/headers-$order"
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_CALLBACK2_ABI=2 -I"$output/source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_callback2_ABI "$output/header-abi.err"
sha256sum "$output/callbacks.a" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Unary/binary callbacks: 4000 manual and 400 Core comparisons/product, source20, inert emission, determinism and sixteen checked/trusted refusals pass\n'
