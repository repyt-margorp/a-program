#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}"); compiler=$(realpath "${2:?pointer-check}"); inert=$(realpath "${3:?array call inert checker}")
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
expect_status lists-admit 0 "$compiler" --steps 5000000 --save "$output/lists.a" "$here/../buffer_query/fixture.p"
expect_status lists-source 0 "$compiler" --steps 5000000 --run reference "$here/../buffer_query/fixture.p"
expect_status lists-loaded 0 "$compiler" --load --run reference "$output/lists.a"
expect_status lists-readback 0 cmp "$output/lists-source.out" "$output/lists-loaded.out"
awk -F'|' '{for (i=1; i<=3; ++i) printf "%s|",$i}' "$output/lists-source.out" > "$output/lists-reference.out"
expect_status empty-admit 0 "$compiler" --steps 5000000 --save "$output/empty.a" "$here/empty.p"
provider="$here/../../../../tests/fixtures/sorted-proof-provider.p"
expect_status sort-admit 0 "$compiler" --legacy-intrinsic-dot --steps 5000000 --imports "$provider" --save "$output/sort.a" "$here/../source_sort/fixture.p"
expect_status sort-source 0 "$compiler" --legacy-intrinsic-dot --steps 5000000 --imports "$provider" --run reference "$here/../source_sort/fixture.p"
expect_status sort-loaded 0 "$compiler" --load --run reference "$output/sort.a"
expect_status sort-readback 0 cmp "$output/sort-source.out" "$output/sort-loaded.out"
awk -F'|' '{for (i=1; i<=6; ++i) printf "%s|",$i}' "$output/sort-source.out" > "$output/sort-reference.out"
sha256sum "$output/"*.a > "$output/before.sha256"
for family in lists empty sort; do
	case "$family" in lists) script=native; client=client ;; empty) script=empty; client=empty_client ;; sort) script=sort; client=sort_client ;; esac
	for mode in source object archive shared; do
		name="$family-$mode"
		sed -e "s/product source/product $mode/" -e "s|artifact $family.a|artifact $output/$family.a|" "$here/$script.aplink" > "$output/$name.aplink"
		expect_status "emit-$name" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$name.aplink" "$output/$name"
		grep -q temporary-arena-array-calls "$output/$name/link.json"; test ! -e "$output/$name/runtime.c"
		options=()
		case "$mode" in
		source) inputs=("$output/$name/component.c") ;;
		object) inputs=("$output/$name/component.o") ;;
		archive) inputs=("$output/$name/library.a") ;;
		shared) inputs=("$output/$name/library.so" "-Wl,-rpath,$output/$name"); options=(-DARRAY_SHARED) ;;
		esac
		expect_status "compile-$name" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/$name" "$here/$client.c" "${inputs[@]}" -Wl,--wrap=malloc -o "$output/client-$name"
		expect_status "client-$name" 0 "$output/client-$name"
		if [[ $family != empty ]]; then expect_status "reference-$name" 0 cmp "$output/$family-reference.out" "$output/client-$name.out"; fi
	done
	expect_status "$family-shared-symbols" 0 nm -D --defined-only "$output/$family-shared/library.so"
	awk '{print $3}' "$output/$family-shared-symbols.out" | LC_ALL=C sort > "$output/$family-symbols.actual"
	{
		awk '/^export / {print "ap_export_" $3; print "ap_buffer_" $3}' "$here/$script.aplink"
		case "$family" in lists) aliases=(Numbers32 Numbers64 Flags Records Nats) ;; empty) aliases=(List Flags Records Nats) ;; sort) aliases=(SList) ;; esac
		if [[ $family == sort ]]; then printf '%s\n' ap_arena_SNat_destroy; else printf '%s\n' ap_arena_Nat_destroy; fi
		for alias in "${aliases[@]}"; do printf 'ap_from_%s\nap_copy_%s\nap_measure_%s\n' "$alias" "$alias" "$alias"; done
	} | LC_ALL=C sort > "$output/$family-symbols.expected"
	expect_status "$family-symbols-exact" 0 cmp "$output/$family-symbols.expected" "$output/$family-symbols.actual"
	options=(); [[ $family == sort ]] && options=("$output/oracle.c")
	expect_status "$family-inert" 0 "$inert" "$output/$family-source.aplink" "$output/$family-raw.c" "$output/$family-raw.h" "${options[@]}"
	expect_status "$family-raw-source" 0 cmp "$output/$family-raw.c" "$output/$family-source/component.c"
	expect_status "$family-raw-header" 0 cmp "$output/$family-raw.h" "$output/$family-source/component.h"
	expect_status "$family-raw-compile" 0 "$cc" "${flags[@]}" -I"$output/$family-source" "$here/$client.c" "$output/$family-raw.c" -Wl,--wrap=malloc -o "$output/$family-raw-client"
	expect_status "$family-raw-client" 0 "$output/$family-raw-client"
	expect_status "$family-raw-exact" 0 cmp "$output/client-$family-source.out" "$output/$family-raw-client.out"
	for policy in repeated trusted; do
		options=(); [[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$family-$policy" 0 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$family-source.aplink" "$output/$family-$policy"
		expect_status "$family-$policy-source" 0 cmp "$output/$family-source/component.c" "$output/$family-$policy/component.c"
		expect_status "$family-$policy-header" 0 cmp "$output/$family-source/component.h" "$output/$family-$policy/component.h"
	done
	sed 's/native_array_calls_v1/native_direct_v1/' "$output/$family-source.aplink" > "$output/$family-old.aplink"
	expect_status "$family-old-native" 0 "$backend" --image-limit none --steps 5000000 --link "$output/$family-old.aplink" "$output/$family-old"
	if grep -q ap_buffer_ "$output/$family-old/component.h"; then exit 1; fi
	done
for mode in source object archive shared; do
	case "$mode" in source) inputs=("$output/sort-source/component.c") ;; object) inputs=("$output/sort-object/component.o") ;; archive) inputs=("$output/sort-archive/library.a") ;; shared) inputs=("$output/sort-shared/library.so" "-Wl,-rpath,$output/sort-shared") ;; esac
	expect_status "oracle-compile-$mode" 0 "$cc" "${flags[@]}" -I"$output/sort-$mode" "$output/oracle.c" "${inputs[@]}" -o "$output/oracle-$mode"
	expect_status "oracle-client-$mode" 0 "$output/oracle-$mode"
done
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/lists-source.aplink" "$output/no-fuel"; test ! -e "$output/no-fuel"
for name in sum32 identity_bool identity_multi identity_tree callback effect identity_callable identity_indexed missing-enum missing-packet missing-nat; do
	sed '/^export /d' "$output/lists-source.aplink" > "$output/$name.aplink"; export_name=$name
	case "$name" in
	identity_callable) printf 'data Callables Callables\n' >> "$output/$name.aplink" ;;
	identity_indexed) printf 'data Indexed Indexed\n' >> "$output/$name.aplink" ;;
	missing-enum) sed -i '/^enum32 /d' "$output/$name.aplink"; export_name=identity_flags ;;
	missing-packet) sed -i '/^data Packet /d' "$output/$name.aplink"; export_name=identity_records ;;
	missing-nat) sed -i '/^nat32 /d' "$output/$name.aplink"; export_name=identity_nats ;;
	esac
	printf 'export %s refused\n' "$export_name" >> "$output/$name.aplink"
	for trust in checked trusted; do
		options=(); [[ $trust == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$trust" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$trust"; test ! -e "$output/$name-$trust"
	done
	if [[ $name == sum32 || $name == identity_bool || $name == identity_multi || $name == identity_tree ]]; then
		sed 's/native_array_calls_v1/native_direct_v1/' "$output/$name.aplink" > "$output/$name-old.aplink"
		expect_status "$name-old" 0 "$backend" --image-limit none --steps 5000000 --link "$output/$name-old.aplink" "$output/$name-old"
	fi
done
for name in tree_return callback_return; do
	sed '/^export /d' "$output/empty-source.aplink" > "$output/$name.aplink"
	[[ $name == tree_return ]] && printf 'data Tree Tree\n' >> "$output/$name.aplink"
	printf 'export %s refused\n' "$name" >> "$output/$name.aplink"
	for trust in checked trusted; do
		options=(); [[ $trust == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$trust" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$trust"; test ! -e "$output/$name-$trust"
	done
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_NATIVE_ABI=2 -I"$output/lists-source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_native_ABI "$output/header-abi.err"; test ! -e "$output/bad-abi.o"
sha256sum "$output/"*.a > "$output/after.sha256"; expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Array calls:three families/four products/raw/source3+6/inert/Core510/List121/slice4356/sort341/temporary arena/rollback statuses1-6 and13 checked/trusted refusals pass\n'
