#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
inert=$(realpath "${3:?callback Core checker}")
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
expect_status admit-consumer 0 "$compiler" --save "$output/callbacks.a" "$here/../callback/fixture.p"
expect_status admit-provider 0 "$compiler" --save "$output/provider.a" "$here/provider.p"
expect_status differential 0 "$compiler" --run reference "$here/../callback/fixture.p"
sha256sum "$output/"*.a > "$output/before.sha256"
for product in source object archive shared; do
	for consumer in left right; do
		sed -e "s/product source/product $product/" -e "s/\(export [^ ]* \)/\1${consumer}_/" "$here/../callback/callback.aplink" > "$output/$consumer-$product.aplink"
		expect_status "emit-$consumer-$product" 0 "$backend" --link "$output/$consumer-$product.aplink" "$output/$consumer-$product"
	done
	printf 'aplink 1\nartifact provider.a\nabi c_scalar_v1\ntarget host-c11\nproduct %s\nlowering scalar_direct_v1\nfallback reject\nexport add32 provider_add32\nexport neg32 provider_neg32\nexport add64 provider_add64\n' "$product" > "$output/provider-$product.aplink"
	expect_status "emit-provider-$product" 0 "$backend" --link "$output/provider-$product.aplink" "$output/provider-$product"
done
expect_status oracle 0 "$inert" "$output/left-source.aplink" "$output/oracle.c" "$output/oracle.h" "$output/oracle-client.c"
cmp "$output/oracle.c" "$output/left-source/component.c"
cmp "$output/oracle.h" "$output/left-source/component.h"
python3 - "$output" <<'PY'
import pathlib, re, sys
output = pathlib.Path(sys.argv[1])
values = re.findall(r'== UINT(?:32|64)_C\(0x([0-9a-f]+)\)', (output / 'oracle-client.c').read_text())
assert len(values) == 400
with (output / 'core_cases.h').open('w') as file:
	file.write('#include <stdint.h>\nstatic const uint64_t core_cases[50][8] = {\n')
	for i in range(50):
		file.write('\t{' + ', '.join('UINT64_C(0x' + value + ')' for value in values[i * 8:(i + 1) * 8]) + '},\n')
	file.write('};\n')
PY
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
			expect_status "compile-$consumer-$provider-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$pair" -I"$output" "$here/client.c" "${left[@]}" "${right[@]}" "${supplied[@]}" -o "$pair/client-$order"
			expect_status "client-$consumer-$provider-$order" 0 "$pair/client-$order"
			cmp "$output/client-$consumer-$provider-$order.out" "$output/differential.out"
			if [[ $provider == shared ]]; then
				expect_status "compile-dynamic-$consumer-$order" 0 "$cc" "${flags[@]}" "${options[@]}" -DDYNAMIC_PROVIDER -I"$pair" -I"$output" "$here/client.c" "${left[@]}" "${right[@]}" -ldl -o "$pair/dynamic-$order"
				expect_status "dynamic-$consumer-$order" 0 "$pair/dynamic-$order" "$output/provider-shared/library.so"
				cmp "$output/dynamic-$consumer-$order.out" "$output/differential.out"
			fi
		done
	done
done
printf 'int main(void) { return 0; }\n' > "$output/duplicate.c"
for product in source object archive; do
	inputs "$output/left-$product" "$product"; duplicate=("${selected[@]}" "${selected[@]}")
	options=()
	[[ $product == archive ]] && options=(-Wl,--whole-archive)
	expect_status "duplicate-$product" 1 "$cc" "${flags[@]}" "$output/duplicate.c" "${options[@]}" "${duplicate[@]}" -Wl,--no-whole-archive -o "$output/duplicate-$product"
	grep -q 'multiple definition.*ap_export_left_' "$output/duplicate-$product.err"
	test ! -e "$output/duplicate-$product"
done
sha256sum "$output/"*.a > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Callback modules: sixteen product pairs, both header orders, two consumers, 800 Core comparisons/client, loaded provider lifetime and three duplicate-symbol refusals pass\n'
