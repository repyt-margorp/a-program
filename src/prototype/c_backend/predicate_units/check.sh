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
modules="$here/../predicate_modules"
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
expect_status admit-provider 0 "$compiler" --save "$output/provider.a" "$modules/provider.p"
expect_status admit-reference 0 "$compiler" --imports "$here/../predicate_native/fixture.p" --save "$output/reference.a" "$modules/consumer.p"
expect_status differential 0 "$compiler" --imports "$here/../predicate_native/fixture.p" --run module_reference "$modules/consumer.p"
sha256sum "$output/"*.a > "$output/before.sha256"
for product in source object archive shared; do
	for consumer in left right; do
		options=()
		[[ $consumer == right ]] && options=(-e 's/LNat/RNat/g' -e 's/LFlag/RFlag/g' -e 's/LNumbers/RNumbers/g' -e 's/left_/right_/g')
		sed -e "s/product source/product $product/" "${options[@]}" "$modules/consumer.aplink" > "$output/$consumer-$product.aplink"
		expect_status "emit-$consumer-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/$consumer-$product.aplink" "$output/$consumer-$product"
	done
	sed "s/product source/product $product/" "$modules/provider.aplink" > "$output/provider-$product.aplink"
	expect_status "emit-provider-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$output/provider-$product.aplink" "$output/provider-$product"
done
mkdir -p "$output/include/left" "$output/include/right" "$output/include/provider"
for module in left right provider; do
	cp "$output/$module-source/component.h" "$output/include/$module/component.h"
	for product in object archive shared; do
		expect_status "header-$module-$product" 0 cmp "$output/$module-source/component.h" "$output/$module-$product/component.h"
	done
	expect_status "body-$module" 0 "$cc" "${flags[@]}" -c "$output/$module-source/component.c" -o "$output/$module-source/client-body.o"
done
for order in forward reverse; do
	client_options=() adapter_options=(-DREVERSE_HEADERS)
	[[ $order == reverse ]] && { client_options=(-DREVERSE_HEADERS); adapter_options=(); }
	expect_status "header-$order" 0 "$cc" "${flags[@]}" "${client_options[@]}" -I"$here" -I"$output/include" -x c -include adapter.h -fsyntax-only "$here/adapter.h"
	expect_status "adapter-$order" 0 "$cc" "${flags[@]}" "${adapter_options[@]}" -Wstrict-prototypes -Wmissing-prototypes -I"$here" -I"$output/include" -c "$here/adapter.c" -o "$output/adapter-$order.o"
	expect_status "client-$order" 0 "$cc" "${flags[@]}" "${client_options[@]}" -Wstrict-prototypes -Wmissing-prototypes -I"$here" -I"$output/include" -c "$here/client.c" -o "$output/client-$order.o"
	expect_status "dynamic-$order" 0 "$cc" "${flags[@]}" "${client_options[@]}" -Wstrict-prototypes -Wmissing-prototypes -DDYNAMIC_PROVIDER -I"$here" -I"$output/include" -c "$here/client.c" -o "$output/dynamic-$order.o"
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
for consumer in source object archive shared; do
	inputs "$output/left-$consumer" "$consumer"; left=("${selected[@]}")
	inputs "$output/right-$consumer" "$consumer"; right=("${selected[@]}")
	for provider in source object archive shared; do
		inputs "$output/provider-$provider" "$provider"; supplied=("${selected[@]}")
		for order in forward reverse; do
			expect_status "link-$consumer-$provider-$order" 0 "$cc" "${flags[@]}" "$output/client-$order.o" "$output/adapter-$order.o" "${left[@]}" "${right[@]}" "${supplied[@]}" -o "$output/client-$consumer-$provider-$order"
			expect_status "run-$consumer-$provider-$order" 0 "$output/client-$consumer-$provider-$order"
			cmp "$output/run-$consumer-$provider-$order.out" "$output/differential.out"
			if [[ $provider == shared ]]; then
				expect_status "link-dynamic-$consumer-$order" 0 "$cc" "${flags[@]}" "$output/dynamic-$order.o" "$output/adapter-$order.o" "${left[@]}" "${right[@]}" -ldl -o "$output/dynamic-$consumer-$order"
				expect_status "run-dynamic-$consumer-$order" 0 "$output/dynamic-$consumer-$order" "$output/provider-shared/library.so"
				cmp "$output/run-dynamic-$consumer-$order.out" "$output/differential.out"
			fi
		done
	done
done
objects=("$output/client-forward.o" "$output/left-source/client-body.o" "$output/right-source/client-body.o" "$output/provider-source/client-body.o")
expect_status missing-adapter 1 "$cc" "${flags[@]}" "${objects[@]}" -o "$output/missing-adapter"
grep -q 'undefined reference.*module_left_unary' "$output/missing-adapter.err"
test ! -e "$output/missing-adapter"
expect_status duplicate-adapter 1 "$cc" "${flags[@]}" "${objects[@]}" "$output/adapter-forward.o" "$output/adapter-forward.o" -o "$output/duplicate-adapter"
grep -q 'multiple definition.*module_left_unary' "$output/duplicate-adapter.err"
test ! -e "$output/duplicate-adapter"
expect_status wrong-descriptor 1 "$cc" "${flags[@]}" -I"$here" -I"$output/include" -c "$here/wrong_type.c" -o "$output/wrong-descriptor.o"
grep -q 'incompatible type.*argument 2' "$output/wrong-descriptor.err"
test ! -e "$output/wrong-descriptor.o"
sha256sum "$output/"*.a > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Native predicate units: separate declarative adapter/client, opposite header orders, sixteen product pairs, 32 linked+8 loaded clients, 93 source observations/client and three type/link refusals pass\n'
