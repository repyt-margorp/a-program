#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}"); compiler=$(realpath "${2:?pointer-check}"); inert=$(realpath "${3:?buffer query inert checker}")
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
expect_status admit 0 "$compiler" --steps 5000000 --save "$output/lists.a" "$here/fixture.p"
expect_status source 0 "$compiler" --steps 5000000 --run reference "$here/fixture.p"
expect_status loaded 0 "$compiler" --load --run reference "$output/lists.a"
expect_status reference-exact 0 cmp "$output/source.out" "$output/loaded.out"
sha256sum "$output/lists.a" > "$output/before.sha256"
for mode in source object archive shared; do
	sed -e "s/product source/product $mode/" -e "s|artifact lists.a|artifact $output/lists.a|" "$here/native.aplink" > "$output/$mode.aplink"
	expect_status "emit-$mode" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$mode.aplink" "$output/$mode"
	grep -q validated-finite-list-size-query "$output/$mode/link.json"; test ! -e "$output/$mode/runtime.c"
	case "$mode" in
	source) inputs=("$output/source/component.c") ;;
	object) inputs=("$output/object/component.o") ;;
	archive) inputs=("$output/archive/library.a") ;;
	shared) inputs=("$output/shared/library.so" "-Wl,-rpath,$output/shared") ;;
	esac
	expect_status "compile-$mode" 0 "$cc" "${flags[@]}" -I"$output/$mode" "$here/client.c" "${inputs[@]}" -Wl,--wrap=malloc -o "$output/client-$mode"
	expect_status "client-$mode" 0 "$output/client-$mode"
	expect_status "reference-$mode" 0 cmp "$output/source.out" "$output/client-$mode.out"
done
expect_status shared-symbols 0 nm -D --defined-only "$output/shared/library.so"
awk '{print $3}' "$output/shared-symbols.out" | LC_ALL=C sort > "$output/symbols.actual"
{ sed -n 's/^export [^ ]* /ap_export_/p' "$here/native.aplink"; printf '%s\n' ap_arena_Nat_destroy; for alias in Numbers32 Numbers64 Flags Records Nats; do printf 'ap_from_%s\nap_copy_%s\nap_measure_%s\n' "$alias" "$alias" "$alias"; done; } | LC_ALL=C sort > "$output/symbols.expected"
expect_status symbols-exact 0 cmp "$output/symbols.expected" "$output/symbols.actual"
expect_status inert 0 "$inert" "$output/source.aplink" "$output/raw.c" "$output/raw.h"
expect_status raw-source 0 cmp "$output/raw.c" "$output/source/component.c"
expect_status raw-header 0 cmp "$output/raw.h" "$output/source/component.h"
expect_status raw-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$here/client.c" "$output/raw.c" -Wl,--wrap=malloc -o "$output/raw-client"
expect_status raw-client 0 "$output/raw-client"
expect_status raw-reference 0 cmp "$output/raw-client.out" "$output/source.out"
for policy in repeated trusted; do
	options=(); [[ $policy == trusted ]] && options=(--trust-image --steps 0)
	expect_status "$policy" 0 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/source.aplink" "$output/$policy"
	expect_status "$policy-source" 0 cmp "$output/source/component.c" "$output/$policy/component.c"
	expect_status "$policy-header" 0 cmp "$output/source/component.h" "$output/$policy/component.h"
done
sed 's/native_buffer_query_v1/native_direct_v1/' "$output/source.aplink" > "$output/old.aplink"
expect_status old-native 0 "$backend" --image-limit none --steps 5000000 --link "$output/old.aplink" "$output/old"
test "$(grep -c '^int ap_measure_' "$output/source/component.h")" == 5
if grep -q ap_measure_ "$output/old/component.h"; then exit 1; fi
expect_status boundary-compile 0 "$cc" "${flags[@]}" -I"$output/old" "$here/boundary_client.c" "$output/old/component.c" -o "$output/boundary-client"
expect_status boundary-client 0 "$output/boundary-client"
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"; test ! -e "$output/no-fuel"
for name in bool-only multi-only tree-only callback effect callable indexed missing-enum missing-packet missing-nat; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"; export_name=$name
	case "$name" in
	bool-only) sed -i '/^nat32 /d; /^data /d' "$output/$name.aplink"; export_name=identity_bool ;;
	multi-only) sed -i '/^nat32 /d; /^data /{ /^data Multi /!d; }' "$output/$name.aplink"; export_name=identity_multi ;;
	tree-only) sed -i '/^nat32 /d; /^data /{ /^data Tree /!d; }' "$output/$name.aplink"; export_name=identity_tree ;;
	callable) printf 'data Callables Callables\n' >> "$output/$name.aplink"; export_name=identity_callable ;;
	indexed) printf 'data Indexed Indexed\n' >> "$output/$name.aplink"; export_name=identity_indexed ;;
	missing-enum) sed -i '/^enum32 /d' "$output/$name.aplink"; export_name=identity_flags ;;
	missing-packet) sed -i '/^data Packet /d' "$output/$name.aplink"; export_name=identity_records ;;
	missing-nat) sed -i '/^nat32 /d' "$output/$name.aplink"; export_name=identity_nats ;;
	esac
	printf 'export %s refused\n' "$export_name" >> "$output/$name.aplink"
	for trust in checked trusted; do
		options=(); [[ $trust == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$trust" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$trust"
		test ! -e "$output/$name-$trust"
	done
	if [[ $name == *-only ]]; then
		sed 's/native_buffer_query_v1/native_direct_v1/' "$output/$name.aplink" > "$output/$name-old.aplink"
		expect_status "$name-old" 0 "$backend" --image-limit none --steps 5000000 --link "$output/$name-old.aplink" "$output/$name-old"
	fi
done
expect_status header-abi 1 "$cc" "${flags[@]}" -DAP_C_NATIVE_ABI=2 -I"$output/source" -c "$here/client.c" -o "$output/bad-abi.o"
grep -q incompatible_A_Program_native_ABI "$output/header-abi.err"; test ! -e "$output/bad-abi.o"
sha256sum "$output/lists.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Finite buffer queries:five payload shapes/four products/raw,List121/slice4356/source4/300 nodes/inert/allocation-free/size-before-copy and ten checked/trusted refusals pass\n'
