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
expect_status admit-consumer 0 "$compiler" --save "$output/consumers.a" "$here/../predicate_native/fixture.p"
expect_status admit-provider 0 "$compiler" --save "$output/provider.a" "$here/provider.p"
expect_status admit-reference 0 "$compiler" --imports "$here/../predicate_native/fixture.p" --save "$output/reference.a" "$here/consumer.p"
expect_status differential 0 "$compiler" --imports "$here/../predicate_native/fixture.p" --run module_reference "$here/consumer.p"
sha256sum "$output/"*.a > "$output/before.sha256"
for product in source object archive shared; do
	for consumer in left right; do
		options=()
		[[ $consumer == right ]] && options=(-e 's/LNat/RNat/g' -e 's/LFlag/RFlag/g' -e 's/LNumbers/RNumbers/g' -e 's/left_/right_/g')
		sed -e "s/product source/product $product/" "${options[@]}" "$here/consumer.aplink" > "$output/$consumer-$product.aplink"
		expect_status "emit-$consumer-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/$consumer-$product.aplink" "$output/$consumer-$product"
	done
	sed "s/product source/product $product/" "$here/provider.aplink" > "$output/provider-$product.aplink"
	expect_status "emit-provider-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/provider-$product.aplink" "$output/provider-$product"
done
inputs() {
	local directory=$1 product=$2
	case "$product" in
	source) selected=("$directory/component.c") ;;
	object) selected=("$directory/component.o") ;;
	archive) selected=("$directory/library.a") ;;
	shared) selected=("$directory/library.so" "-Wl,-rpath,$directory") ;;
	esac
}
for consumer in source object archive shared; do
	for provider in source object archive shared; do
		pair="$output/$consumer-$provider"
		mkdir -p "$pair/left" "$pair/right" "$pair/provider"
		cp "$output/left-$consumer/component.h" "$pair/left/component.h"
		cp "$output/right-$consumer/component.h" "$pair/right/component.h"
		cp "$output/provider-$provider/component.h" "$pair/provider/component.h"
		inputs "$output/left-$consumer" "$consumer"; left=("${selected[@]}")
		inputs "$output/right-$consumer" "$consumer"; right=("${selected[@]}")
		inputs "$output/provider-$provider" "$provider"; supplied=("${selected[@]}")
		for order in forward reverse; do
			options=()
			[[ $order == reverse ]] && options=(-DREVERSE_HEADERS)
			expect_status "compile-$consumer-$provider-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$pair" "$here/client.c" "${left[@]}" "${right[@]}" "${supplied[@]}" -o "$pair/client-$order"
			expect_status "client-$consumer-$provider-$order" 0 "$pair/client-$order"
			cmp "$output/client-$consumer-$provider-$order.out" "$output/differential.out"
			if [[ $provider == shared ]]; then
				expect_status "compile-dynamic-$consumer-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -DDYNAMIC_PROVIDER -I"$pair" "$here/client.c" "${left[@]}" "${right[@]}" -ldl -o "$pair/dynamic-$order"
				expect_status "dynamic-$consumer-$order" 0 "$pair/dynamic-$order" "$output/provider-shared/library.so"
				cmp "$output/dynamic-$consumer-$order.out" "$output/differential.out"
			fi
		done
	done
done
expect_status wrong-type 1 "$cc" "${flags[@]}" -I"$output/source-source" -c "$here/wrong_type.c" -o "$output/wrong-type.o"
grep -q incompatible-pointer-types "$output/wrong-type.err"
test ! -e "$output/wrong-type.o"
printf 'int main(void) { return 0; }\n' > "$output/duplicate.c"
for module in left provider; do
	for product in source object archive; do
		inputs "$output/$module-$product" "$product"; duplicate=("${selected[@]}" "${selected[@]}")
		options=()
		[[ $product == archive ]] && options=(-Wl,--whole-archive)
		expect_status "duplicate-$module-$product" 1 "$cc" "${flags[@]}" "$output/duplicate.c" "${options[@]}" "${duplicate[@]}" -Wl,--no-whole-archive -o "$output/duplicate-$module-$product"
		grep -q 'multiple definition.*ap_export_' "$output/duplicate-$module-$product.err"
		test ! -e "$output/duplicate-$module-$product"
	done
done
sha256sum "$output/"*.a > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Native predicate modules: sixteen product pairs, both header orders, two consumers, explicit tag adapters, 93 source observations/client, shared-arena transactions, eight loaded-provider clients and seven C type/symbol refusals pass\n'
