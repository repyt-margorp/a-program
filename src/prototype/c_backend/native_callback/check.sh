#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}"); compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?native callback inert checker}")
if [[ -n ${4:-} ]]; then output=$4
else temporary=$(mktemp -d); trap 'rm -rf "$temporary"' EXIT; output="$temporary/report"; fi
here=$(cd "$(dirname "$0")" && pwd); cc=${CC:-cc}; ar=${AR:-ar}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"; mkdir -p "$output"; output=$(realpath "$output")
expect_status() {
	local name=$1 expected=$2 actual=0; shift 2
	printf '%q ' "$@" > "$output/$name.command"; printf '\n' >> "$output/$name.command"
	"$@" > "$output/$name.out" 2> "$output/$name.err" || actual=$?
	printf '%s\t%s\t%s\n' "$name" "$expected" "$actual" >> "$output/status.tsv"
	if [[ $actual != "$expected" ]]; then
		cat "$output/$name.out" "$output/$name.err" >&2
		printf '%s: expected %s, got %s\n' "$name" "$expected" "$actual" >&2; exit 1
	fi
}
expect_status admit 0 "$compiler" --steps 5000000 --save "$output/callbacks.a" "$here/fixture.p"
sha256sum "$output/callbacks.a" > "$output/before.sha256"
expect_status source-reference 0 "$compiler" --steps 5000000 --run reference "$here/fixture.p"
expect_status loaded-reference 0 "$compiler" --load --run reference "$output/callbacks.a"
expect_status references-exact 0 cmp "$output/source-reference.out" "$output/loaded-reference.out"
for product in source object archive shared; do
	sed -e "s/product source/product $product/" -e "s|artifact callbacks.a|artifact $output/callbacks.a|" "$here/native.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$product.aplink" "$output/$product"
	grep -q 'same-width-unary-binary-int32-int64-scalars' "$output/$product/link.json"; test ! -e "$output/$product/runtime.c"
	options=()
	case "$product" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared"); options=(-DNATIVE_SHARED_PRODUCT) ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/$product" "$here/client.c" "${inputs[@]}" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "client-$product" 0 "$output/client-$product"
	expect_status "reference-$product" 0 cmp "$output/source-reference.out" "$output/client-$product.out"
done
expect_status shared-symbols 0 nm -D --defined-only "$output/shared/library.so"
awk '{print $3}' "$output/shared-symbols.out" | LC_ALL=C sort > "$output/symbols.actual"
{ sed -n 's/^export [^ ]* /ap_export_/p' "$here/native.aplink"; printf '%s\n' ap_arena_Numbers32_destroy ap_from_Numbers32 ap_from_Numbers64 ap_copy_Numbers32 ap_copy_Numbers64; } | LC_ALL=C sort > "$output/symbols.expected"
expect_status symbols-exact 0 cmp "$output/symbols.expected" "$output/symbols.actual"
expect_status inert 0 "$inert" "$output/source.aplink" "$output/raw.c" "$output/raw.h" "$output/oracle.c"
expect_status raw-source 0 cmp "$output/raw.c" "$output/source/component.c"
expect_status raw-header 0 cmp "$output/raw.h" "$output/source/component.h"
expect_status raw-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$here/client.c" "$output/raw.c" -Wl,--wrap=malloc -o "$output/raw-client"
expect_status raw-client 0 "$output/raw-client"
expect_status raw-reference 0 cmp "$output/raw-client.out" "$output/source-reference.out"
expect_status oracle-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$output/oracle.c" "$output/source/component.c" -o "$output/oracle"
expect_status oracle-client 0 "$output/oracle"
sed '/^enum32 /d; /^data /d; /^export /d' "$output/source.aplink" > "$output/scalar.aplink"
printf 'export apply64 apply64\n' >> "$output/scalar.aplink"
expect_status scalar-emit 0 "$backend" --image-limit none --steps 5000000 --link "$output/scalar.aplink" "$output/scalar"
expect_status scalar-compile 0 "$cc" "${flags[@]}" -I"$output/scalar" "$here/scalar_client.c" "$output/scalar/component.c" -o "$output/scalar-client"
expect_status scalar-client 0 "$output/scalar-client"
for policy in repeated trusted; do
	options=(); [[ $policy == trusted ]] && options=(--trust-image --steps 0)
	expect_status "$policy" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 "${options[@]}" --link "$output/source.aplink" "$output/$policy"
	expect_status "$policy-source" 0 cmp "$output/source/component.c" "$output/$policy/component.c"
	expect_status "$policy-header" 0 cmp "$output/source/component.h" "$output/$policy/component.h"
done
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"; test ! -e "$output/no-fuel"
for name in ternary mixed_width mixed_result enum_result natural_domain returned effect callable_identity indexed_identity missing-data missing-enum; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"; export_name=$name
	case "$name" in
	natural_domain) printf 'nat32 Nat Nat\n' >> "$output/$name.aplink" ;;
	callable_identity) printf 'data Callables Callables\n' >> "$output/$name.aplink" ;;
	indexed_identity) printf 'data Indexed Indexed\n' >> "$output/$name.aplink" ;;
	missing-data) sed -i '/^data Numbers32 /d' "$output/$name.aplink"; export_name=map32 ;;
	missing-enum) sed -i '/^enum32 Bool /d' "$output/$name.aplink"; export_name=packet32 ;;
	esac
	printf 'export %s refused\n' "$export_name" >> "$output/$name.aplink"
	for mode in checked trusted; do
		options=(); [[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$mode" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$mode"
		test ! -e "$output/$name-$mode"
	done
done
for profile in scalar native callback callback2 predicate_native predicate_signed; do
	sed -e "s/c_callback_native_v1/c_${profile}_v1/" -e "s/callback_native_direct_v1/${profile}_direct_v1/" "$output/source.aplink" > "$output/old-$profile.aplink"
	if [[ $profile == scalar || $profile == callback || $profile == callback2 ]]; then sed -i '/^enum32 /d; /^data /d' "$output/old-$profile.aplink"; fi
	for mode in checked trusted; do
		options=(); [[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "old-$profile-$mode" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/old-$profile.aplink" "$output/old-$profile-$mode"
		test ! -e "$output/old-$profile-$mode"
	done
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_CALLBACK_NATIVE_ABI=2 -I"$output/source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_callback_native_ABI "$output/header-abi.err"
sha256sum "$output/callbacks.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Native scalar callbacks:four products/raw,781 Lists/width/map+five combines/reductions,655 Trees/client,3432 full-value Core comparisons,source5/record/no-arena/status/rollback and17 checked/trusted refusal pairs pass\n'
