#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?multi-tail inert test}")
if [[ -n ${4:-} ]]; then output=$4
else temporary=$(mktemp -d); trap 'rm -rf "$temporary"' EXIT; output="$temporary/report"; fi
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
ar=${AR:-ar}
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
expect_status admit 0 "$compiler" --steps 5000000 --save "$output/tree.a" "$here/fixture.p"
sha256sum "$output/tree.a" > "$output/before.sha256"
expect_status reference-source 0 "$compiler" --steps 5000000 --run reference "$here/fixture.p"
expect_status reference-core 0 "$compiler" --load --run reference "$output/tree.a"
expect_status reference-compare 0 cmp "$output/reference-source.out" "$output/reference-core.out"
for product in source object archive; do
	sed -e "s/product source/product $product/" -e "s|artifact tree.a|artifact $output/tree.a|" "$here/tree.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$product.aplink" "$output/$product"
	test ! -e "$output/$product/runtime.c"
	grep -q 'two-self-nodes' "$output/$product/link.json"
	grep -q 'at-most-two-direct-self-fields' "$output/$product/link.json"
	if [[ $product != source ]]; then expect_status "header-$product" 0 cmp "$output/source/component.h" "$output/$product/component.h"; fi
	case "$product" in
	source) input="$output/$product/component.c" ;;
	object) input="$output/$product/component.o" ;;
	archive) input="$output/$product/library.a" ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" -I"$output/$product" "$here/client.c" "$input" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "run-$product" 0 "$output/client-$product"
	expect_status "compare-$product" 0 cmp "$output/run-$product.out" "$output/reference-source.out"
done
expect_status inert 0 "$inert" "$output/source.aplink" "$output/raw.c" "$output/raw.h" "$output/oracle.c"
expect_status raw-header 0 cmp "$output/raw.h" "$output/source/component.h"
expect_status raw-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$here/client.c" "$output/raw.c" -Wl,--wrap=malloc -o "$output/raw-client"
expect_status raw-run 0 "$output/raw-client"
expect_status raw-compare 0 cmp "$output/raw-run.out" "$output/reference-source.out"
expect_status oracle-compile 0 "$cc" "${flags[@]}" -I"$output/source" "$output/oracle.c" "$output/source/component.c" -o "$output/oracle"
expect_status oracle-run 0 "$output/oracle"
expect_status repeated 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/source.aplink" "$output/repeated"
expect_status deterministic 0 diff -ru "$output/source" "$output/repeated"
expect_status trusted 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --trust-image --steps 0 --link "$output/source.aplink" "$output/trusted"
expect_status trusted-source 0 cmp "$output/source/component.c" "$output/trusted/component.c"
expect_status trusted-header 0 cmp "$output/source/component.h" "$output/trusted/component.h"
expect_status no-fuel 3 "$backend" --steps 0 --link "$output/source.aplink" "$output/no-fuel"
test ! -e "$output/no-fuel"
for name in effect callback wide_identity callable_identity indexed record_identity unselected-enum; do
	sed '/^export /d' "$output/source.aplink" > "$output/$name.aplink"
	case "$name" in
	wide_identity) printf 'data Wide Wide\n' >> "$output/$name.aplink"; export_name=$name ;;
	callable_identity) printf 'data Callable Callable\n' >> "$output/$name.aplink"; export_name=$name ;;
	indexed) printf 'data_of empty_indexed Indexed\n' >> "$output/$name.aplink"; export_name=identity ;;
	record_identity) printf 'data Record Record\ndata RecordTree RecordTree\n' >> "$output/$name.aplink"; export_name=$name ;;
	unselected-enum) sed -i '/^enum32 /d' "$output/$name.aplink"; export_name=identity ;;
	*) export_name=$name ;;
	esac
	printf 'export %s rejected\n' "$export_name" >> "$output/$name.aplink"
	for mode in checked trusted; do
		options=(); [[ $mode == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$name-$mode" 4 "$backend" --image-limit none --steps 5000000 "${options[@]}" --link "$output/$name.aplink" "$output/$name-$mode"
		test ! -e "$output/$name-$mode"; test ! -s "$output/$name-$mode.out"
	done
done
sha256sum "$output/tree.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Two direct Self fields: products/raw, six observations, 1616 separate Core comparisons, 202 finite Trees/reversed fields, shared DAGs/cycles/depth/allocation rollback and seven checked/trusted refusals pass\n'
