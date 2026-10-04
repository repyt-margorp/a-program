#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?signed predicate inert checker}")
if [[ -n ${4:-} ]]; then output=$4
else temporary=$(mktemp -d); trap 'rm -rf "$temporary"' EXIT; output="$temporary/report"; fi
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}; ar=${AR:-ar}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"; mkdir -p "$output"; output=$(realpath "$output")
expect_status() {
	local name=$1 expected=$2 actual=0
	shift 2
	printf '%q ' "$@" > "$output/$name.command"; printf '\n' >> "$output/$name.command"
	"$@" > "$output/$name.out" 2> "$output/$name.err" || actual=$?
	printf '%s\t%s\t%s\n' "$name" "$expected" "$actual" >> "$output/status.tsv"
	if [[ $actual != "$expected" ]]; then
		cat "$output/$name.out" "$output/$name.err" >&2
		printf '%s: expected %s, got %s\n' "$name" "$expected" "$actual" >&2; exit 1
	fi
}
expect_status admit 0 "$compiler" --steps 5000000 --save "$output/predicates.a" "$here/../predicate_integer/fixture.p"
sha256sum "$output/predicates.a" > "$output/before.sha256"
expect_status source-reference 0 "$compiler" --steps 5000000 --run reference "$here/../predicate_integer/fixture.p"
expect_status loaded-reference 0 "$compiler" --load --run reference "$output/predicates.a"
expect_status references-exact 0 cmp "$output/source-reference.out" "$output/loaded-reference.out"
for product in source object archive shared; do
	sed -e "s/product source/product $product/" -e "s|artifact predicates.a|artifact $output/predicates.a|" "$here/predicate.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$product.aplink" "$output/$product"
	grep -q 'same-width-unary-binary-int32-int64-two-case-enum' "$output/$product/link.json"
	grep -q '"invalid_result":2' "$output/$product/link.json"; test ! -e "$output/$product/runtime.c"
	options=()
	case "$product" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared"); options=(-DSIGNED_SHARED_PRODUCT) ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/$product" "$here/client.c" "${inputs[@]}" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "client-$product" 0 "$output/client-$product"
	expect_status "reference-$product" 0 cmp "$output/source-reference.out" "$output/client-$product.out"
done
expect_status shared-symbols 0 nm -D --defined-only "$output/shared/library.so"
awk '{print $3}' "$output/shared-symbols.out" | LC_ALL=C sort > "$output/symbols.actual"
{ sed -n 's/^export [^ ]* /ap_export_/p' "$here/predicate.aplink"; printf '%s\n' ap_arena_Numbers32_destroy ap_copy_Numbers32 ap_copy_Numbers64 ap_from_Numbers32 ap_from_Numbers64; } | LC_ALL=C sort > "$output/symbols.expected"
expect_status symbols-exact 0 cmp "$output/symbols.expected" "$output/symbols.actual"
expect_status inert 0 "$inert" "$output/source.aplink" "$output/raw.c" "$output/raw.h" "$output/oracle.c"
expect_status raw-source 0 cmp "$output/raw.c" "$output/source/component.c"
expect_status raw-header 0 cmp "$output/raw.h" "$output/source/component.h"
expect_status raw-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$here/client.c" "$output/raw.c" -Wl,--wrap=malloc -o "$output/raw-client"
expect_status raw-client 0 "$output/raw-client"
expect_status raw-reference 0 cmp "$output/raw-client.out" "$output/source-reference.out"
expect_status oracle-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$output/oracle.c" "$output/source/component.c" -o "$output/oracle"
expect_status oracle-client 0 "$output/oracle"
sed '/^data /d; /^export /d' "$output/source.aplink" > "$output/scalar.aplink"
printf 'export choose64 choose64\n' >> "$output/scalar.aplink"
expect_status scalar-emit 0 "$backend" --image-limit none --steps 5000000 --link "$output/scalar.aplink" "$output/scalar"
expect_status scalar-compile 0 "$cc" "${flags[@]}" -I"$output/scalar" "$here/scalar_client.c" "$output/scalar/component.c" -o "$output/scalar-client"
expect_status scalar-client 0 "$output/scalar-client"
expect_status nested-admit 0 "$compiler" --steps 5000000 --save "$output/scalar.a" "$here/scalar_fixture.p"
sed "s|artifact scalar.a|artifact $output/scalar.a|" "$here/scalar.aplink" > "$output/nested.aplink"
expect_status nested-emit 0 "$backend" --image-limit none --steps 5000000 --link "$output/nested.aplink" "$output/nested"
expect_status nested-compile 0 "$cc" "${flags[@]}" -I"$output/nested" "$here/nested_client.c" "$output/nested/component.c" -o "$output/nested-client"
expect_status nested-client 0 "$output/nested-client"
sed -e 's/product source/product executable/' "$output/nested.aplink" > "$output/executable.aplink"
printf 'entry truth\n' >> "$output/executable.aplink"
expect_status executable-emit 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/executable.aplink" "$output/executable"
expect_status executable-run 0 "$output/executable/program"
for policy in repeated trusted; do
	options=(); [[ $policy == trusted ]] && options=(--trust-image --steps 0)
	expect_status "$policy" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 "${options[@]}" --link "$output/source.aplink" "$output/$policy"
	expect_status "$policy-source" 0 cmp "$output/source/component.c" "$output/$policy/component.c"
	expect_status "$policy-header" 0 cmp "$output/source/component.h" "$output/$policy/component.h"
done
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"
test ! -e "$output/no-fuel"
for name in ternary mixed_width tri_result scalar_result returned effect callable_identity missing-enum; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"; export_name=$name
	case "$name" in
	tri_result) printf 'enum32 Tri Tri\n' >> "$output/$name.aplink" ;;
	callable_identity) printf 'data Callables Callables\n' >> "$output/$name.aplink" ;;
	missing-enum) sed -i '/^enum32 Bool /d' "$output/$name.aplink"; export_name=apply32 ;;
	esac
	printf 'export %s refused\n' "$export_name" >> "$output/$name.aplink"
	for mode in checked trusted; do
		options=(); [[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$mode" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$mode"
		test ! -e "$output/$name-$mode"
	done
done
for profile in scalar native callback callback2 predicate_native; do
	sed -e "s/c_predicate_signed_v1/c_${profile}_v1/" -e "s/predicate_signed_direct_v1/${profile}_direct_v1/" "$output/source.aplink" > "$output/old-$profile.aplink"
	if [[ $profile != native && $profile != predicate_native ]]; then sed -i '/^enum32 /d; /^data /d' "$output/old-$profile.aplink"; fi
	for mode in checked trusted; do
		options=(); [[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "old-$profile-$mode" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/old-$profile.aplink" "$output/old-$profile-$mode"
		test ! -e "$output/old-$profile-$mode"
	done
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_PREDICATE_SIGNED_ABI=2 -I"$output/source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_predicate_signed_ABI "$output/header-abi.err"
sha256sum "$output/predicates.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Signed predicates: four products/raw, 781 Lists/width and 18744 foreign partition selections/product, source-reference8, 120 Core tags/1248 Core lengths, no-arena status propagation, invalid results/rollback and 13 checked/trusted refusal pairs pass\n'
