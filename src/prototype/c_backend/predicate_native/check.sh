#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?inert native predicate checker}")
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
expect_status admit 0 "$compiler" --save "$output/predicates.a" "$here/fixture.p"
sha256sum "$output/predicates.a" > "$output/before.sha256"
for product in source object archive shared; do
	sed "s/product source/product $product/" "$here/predicate.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --link "$output/$product.aplink" "$output/$product"
	grep -q 'selected-nat32-unary-binary-two-case-enum' "$output/$product/link.json"
	grep -q '"invalid_result":2' "$output/$product/link.json"
	grep -q 'finite-list-copy-out' "$output/$product/link.json"
	test ! -e "$output/$product/runtime.c"
	options=()
	case "$product" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared"); options=(-DPREDICATE_SHARED_PRODUCT) ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/$product" "$here/client.c" "${inputs[@]}" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "client-$product" 0 "$output/client-$product"
	cmp "$output/client-source.out" "$output/client-$product.out"
done
expect_status shared-symbols 0 nm -D --defined-only "$output/shared/library.so"
awk '{print $3}' "$output/shared-symbols.out" | LC_ALL=C sort > "$output/shared-symbols.actual"
printf '%s\n' ap_arena_Nat_destroy ap_copy_Numbers ap_from_Numbers \
	ap_export_apply_predicate ap_export_apply_comparator ap_export_apply_reverse \
	ap_export_unused ap_export_filter ap_export_select ap_export_post_check \
	ap_export_length ap_export_fingerprint ap_export_increment | LC_ALL=C sort > "$output/shared-symbols.expected"
cmp "$output/shared-symbols.actual" "$output/shared-symbols.expected"
for product in source object archive shared; do
	sed "s/product source/product $product/" "$here/alias.aplink" > "$output/alias-$product.aplink"
	expect_status "alias-emit-$product" 0 "$backend" --link "$output/alias-$product.aplink" "$output/alias-$product"
	case "$product" in
	source) inputs=("$output/alias-source/component.c") ;;
	object) inputs=("$output/alias-object/component.o") ;;
	archive) inputs=("$output/alias-archive/library.a") ;;
	shared) inputs=("$output/alias-shared/library.so" "-Wl,-rpath,$output/alias-shared") ;;
	esac
	expect_status "alias-compile-$product" 0 "$cc" "${flags[@]}" -I"$output/alias-$product" "$here/alias_client.c" "${inputs[@]}" -o "$output/alias-client-$product"
	expect_status "alias-client-$product" 0 "$output/alias-client-$product"
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
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"
no_product "$output/no-fuel"
sha256sum "$output/source"/* > "$output/prior.sha256"
expect_status prior-output 2 "$backend" --link "$output/source.aplink" "$output/source"
sha256sum -c "$output/prior.sha256" > "$output/prior-check.out"
for name in ternary mixed_domains tri_result host_domain host_result returned effect callable_identity; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"
	case "$name" in
	mixed_domains) printf 'nat32 OtherNat OtherNat\n' >> "$output/$name.aplink" ;;
	tri_result) printf 'enum32 Tri Tri\n' >> "$output/$name.aplink" ;;
	callable_identity) printf 'data Callables Callables\n' >> "$output/$name.aplink" ;;
	esac
	printf 'export %s refused\n' "$name" >> "$output/$name.aplink"
	for policy in checked trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$policy" 4 "$backend" "${options[@]}" --link "$output/$name.aplink" "$output/$name-$policy"
		no_product "$output/$name-$policy"
	done
done
for selection in nat32 enum32; do
	sed "/^$selection /d" "$output/source.aplink" > "$output/missing-$selection.aplink"
	for policy in checked trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "missing-$selection-$policy" 4 "$backend" "${options[@]}" --link "$output/missing-$selection.aplink" "$output/missing-$selection-$policy"
		no_product "$output/missing-$selection-$policy"
	done
done
for profile in scalar native callback callback2; do
	sed -e "s/c_predicate_native_v1/c_${profile}_v1/" -e "s/predicate_native_direct_v1/${profile}_direct_v1/" "$output/source.aplink" > "$output/old-$profile.aplink"
	if [[ $profile != native ]]; then sed -i '/^enum32 /d; /^nat32 /d; /^data /d' "$output/old-$profile.aplink"; fi
	for policy in checked trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "old-$profile-$policy" 4 "$backend" "${options[@]}" --link "$output/old-$profile.aplink" "$output/old-$profile-$policy"
		no_product "$output/old-$profile-$policy"
	done
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_PREDICATE_NATIVE_ABI=2 -I"$output/source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_predicate_native_ABI "$output/header-abi.err"
sha256sum "$output/predicates.a" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Native predicates: 1365 filters/13650 partition selections and 574 Core comparisons per product, six source observations, distinct alias pairs, inert emission, transactional failures and 28 refusals pass\n'
