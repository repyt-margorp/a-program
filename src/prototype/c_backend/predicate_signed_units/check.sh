#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
if [[ -n ${3:-} ]]; then output=$3
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
expect_status admit-consumers 0 "$compiler" --steps 5000000 --save "$output/consumers.a" "$here/../predicate_integer/fixture.p"
expect_status admit-provider 0 "$compiler" --steps 5000000 --save "$output/provider.a" "$here/provider.p"
for module in consumer provider; do
	fixture="$here/provider.p"; image="$output/provider.a"
	if [[ $module == consumer ]]; then fixture="$here/../predicate_integer/fixture.p"; image="$output/consumers.a"; fi
	expect_status "source-$module-reference" 0 "$compiler" --steps 5000000 --run reference "$fixture"
	expect_status "loaded-$module-reference" 0 "$compiler" --load --run reference "$image"
	expect_status "$module-reference-exact" 0 cmp "$output/source-$module-reference.out" "$output/loaded-$module-reference.out"
done
expect_status combined-reference 0 cat "$output/source-provider-reference.out" "$output/source-consumer-reference.out"
sha256sum "$output/"*.a > "$output/before.sha256"
for product in source object archive shared; do
	for module in left right provider; do
		options=(); script="$here/provider.aplink"
		if [[ $module != provider ]]; then script="$here/consumer.aplink"; fi
		[[ $module == right ]] && options=(-e 's/LFlag/RFlag/g' -e 's/LNumbers/RNumbers/g' -e 's/left_/right_/g')
		sed -e "s/product source/product $product/" "${options[@]}" "$script" > "$output/$module-$product.aplink"
		expect_status "emit-$module-$product" 0 "$backend" --cc "$cc" --ar "$ar" --image-limit none --steps 5000000 --link "$output/$module-$product.aplink" "$output/$module-$product"
		test ! -e "$output/$module-$product/runtime.c"
	done
done
mkdir -p "$output/include/left" "$output/include/right" "$output/include/provider"
for module in left right provider; do
	cp "$output/$module-source/component.h" "$output/include/$module/component.h"
	for product in object archive shared; do
		expect_status "header-$module-$product" 0 cmp "$output/$module-source/component.h" "$output/$module-$product/component.h"
	done
	expect_status "source-body-$module" 0 "$cc" "${flags[@]}" -c "$output/$module-source/component.c" -o "$output/$module-source/client-body.o"
	expect_status "symbols-$module" 0 nm -D --defined-only "$output/$module-shared/library.so"
	awk '{print $3}' "$output/symbols-$module.out" | LC_ALL=C sort > "$output/$module-symbols.actual"
	sed -n 's/^export [^ ]* /ap_export_/p' "$output/$module-shared.aplink" > "$output/$module-symbols.unsorted"
	if [[ $module != provider ]]; then
		prefix=L; [[ $module == right ]] && prefix=R
		printf 'ap_arena_%sNumbers32_destroy\nap_from_%sNumbers32\nap_from_%sNumbers64\nap_copy_%sNumbers32\nap_copy_%sNumbers64\n' "$prefix" "$prefix" "$prefix" "$prefix" "$prefix" >> "$output/$module-symbols.unsorted"
	fi
	LC_ALL=C sort "$output/$module-symbols.unsorted" > "$output/$module-symbols.expected"
	expect_status "symbols-$module-exact" 0 cmp "$output/$module-symbols.expected" "$output/$module-symbols.actual"
done
for order in forward reverse; do
	options=(); [[ $order == reverse ]] && options=(-DREVERSE_HEADERS)
	expect_status "header-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$here" -I"$output/include" -include "$here/adapter.h" -include "$here/adapter.h" -x c -c /dev/null -o "$output/header-$order.o"
	expect_status "client-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$here" -I"$output/include" -c "$here/client.c" -o "$output/client-$order.o"
	expect_status "adapter-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$here" -I"$output/include" -c "$here/adapter.c" -o "$output/adapter-$order.o"
	expect_status "dynamic-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -DDYNAMIC_PROVIDER -I"$here" -I"$output/include" -c "$here/client.c" -o "$output/dynamic-$order.o"
done
inputs() {
	local directory=$1 product=$2
	case "$product" in
	source) selected=("$directory/client-body.o") ;;
	object) selected=("$directory/component.o") ;;
	archive) selected=("$directory/library.a") ;;
	shared) selected=("$directory/library.so" "-Wl,-rpath,$directory") ;;
	esac
}
for left_product in source object archive shared; do
	inputs "$output/left-$left_product" "$left_product"; left=("${selected[@]}")
	for right_product in source object archive shared; do
		inputs "$output/right-$right_product" "$right_product"; right=("${selected[@]}")
		for provider_product in source object archive shared; do
			inputs "$output/provider-$provider_product" "$provider_product"; supplied=("${selected[@]}")
			for order in forward reverse; do
				name="$left_product-$right_product-$provider_product-$order"
				expect_status "link-$name" 0 "$cc" "${flags[@]}" "$output/client-$order.o" "$output/adapter-$order.o" "${left[@]}" "${right[@]}" "${supplied[@]}" -o "$output/client-$name"
				expect_status "run-$name" 0 "$output/client-$name"
				cmp "$output/run-$name.out" "$output/combined-reference.out"
				if [[ $provider_product == shared ]]; then
					expect_status "link-dynamic-$name" 0 "$cc" "${flags[@]}" "$output/dynamic-$order.o" "$output/adapter-$order.o" "${left[@]}" "${right[@]}" -ldl -o "$output/dynamic-$name"
					expect_status "run-dynamic-$name" 0 "$output/dynamic-$name" "$output/provider-shared/library.so"
					cmp "$output/run-dynamic-$name.out" "$output/combined-reference.out"
				fi
			done
		done
	done
done
objects=("$output/client-forward.o" "$output/left-source/client-body.o" "$output/right-source/client-body.o" "$output/provider-source/client-body.o")
expect_status missing-adapter 1 "$cc" "${flags[@]}" "${objects[@]}" -o "$output/missing-adapter"
grep -q 'undefined reference.*signed_left_unary32' "$output/missing-adapter.err"; test ! -e "$output/missing-adapter"
expect_status duplicate-adapter 1 "$cc" "${flags[@]}" "${objects[@]}" "$output/adapter-forward.o" "$output/adapter-forward.o" -o "$output/duplicate-adapter"
grep -q 'multiple definition.*signed_left_unary32' "$output/duplicate-adapter.err"; test ! -e "$output/duplicate-adapter"
expect_status duplicate-provider 1 "$cc" "${flags[@]}" "${objects[@]}" "$output/provider-source/client-body.o" "$output/adapter-forward.o" -o "$output/duplicate-provider"
grep -q 'multiple definition.*ap_export_provider_keep32' "$output/duplicate-provider.err"; test ! -e "$output/duplicate-provider"
for kind in enum width; do
	options=(); [[ $kind == width ]] && options=(-DWRONG_WIDTH)
	expect_status "wrong-$kind" 1 "$cc" "${flags[@]}" "${options[@]}" -I"$here" -I"$output/include" -c "$here/wrong_type.c" -o "$output/wrong-$kind.o"
	grep -q 'incompatible type.*argument 2' "$output/wrong-$kind.err"; test ! -e "$output/wrong-$kind.o"
done
sha256sum "$output/"*.a > "$output/after.sha256"
expect_status images-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
test -z "$(find "$output" -maxdepth 1 -name '*.tmp.*' -print)"
printf 'Signed predicate units:64 mixed triples/both header orders,128 linked+32 loaded clients,16 source observations/client,121 Lists/width/two enums, flat provider status/shared-arena rollback and five type/link refusals pass\n'
