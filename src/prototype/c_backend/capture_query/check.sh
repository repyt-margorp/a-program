#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?current a-to-c}")
pointer=$(realpath "${2:?qualified pointer-check}")
fault=$(realpath "${3:?query-fault/inert test}")
out=${4:?new retained output}
here=$(cd "$(dirname "$0")" && pwd)
fixtures="$here/../fixtures"
cc=${CC:-cc}
ar=${AR:-ar}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$out"
mkdir -p "$out"
out=$(realpath "$out")
run()
{
	local label=$1 expected=$2 status=0
	shift 2
	printf '%s\0' "$label" "$expected" "$#" "$@" >>"$out/argv.nul"
	"$@" >"$out/$label.out" 2>"$out/$label.err" || status=$?
	printf '%s\t%s\t%s\n' "$label" "$expected" "$status" >>"$out/status.tsv"
	test "$status" = "$expected"
}
for family in static transitive; do
	fixture="$fixtures/${family}_functions.p"
	image="$out/$family.a"
	run "$family-admit" 0 "$pointer" --save-materialized "$image" "$fixture"
	sha256sum "$image" >"$out/$family-before.sha256"
	sed -e "s|artifact $family-functions.a|artifact $image|" "$fixtures/${family}_functions.aplink" >"$out/$family-source.aplink"
	if [[ $family == static ]]; then
		for name in captured_block shadowed_block repeated_block curried_block unused_block captured64 nested_two nested_three unused_effect; do
			printf 'export %s %s\n' "$name" "$name" >>"$out/$family-source.aplink"
		done
		dynamic=dynamic_callback
		effect=effect_block
	else
		dynamic=dynamic_callback
		effect=demanded_effect
	fi
	for product in source object archive; do
		sed "s/product source/product $product/" "$out/$family-source.aplink" >"$out/$family-$product.aplink.new"
		mv "$out/$family-$product.aplink.new" "$out/$family-$product.aplink"
		run "$family-emit-$product" 0 "$backend" --cc "$cc" --ar "$ar" --link "$out/$family-$product.aplink" "$out/$family-$product"
		case $product in
		source) operand="$out/$family-$product/component.c" ;;
		object) operand="$out/$family-$product/component.o" ;;
		archive) operand="$out/$family-$product/library.a" ;;
		esac
		run "$family-client-build-$product" 0 "$cc" "${flags[@]}" -I"$out/$family-$product" "$here/../lower/${family}_function_client.c" "$operand" -o "$out/$family-client-$product"
		run "$family-client-$product" 0 "$out/$family-client-$product"
	done
	run "$family-source" 0 "$pointer" --imports "$fixture" --run main "$fixtures/${family}_functions_differential.p"
	run "$family-source-match" 0 cmp "$out/$family-source.out" "$out/$family-client-source.out"
	run "$family-query-faults" 0 "$fault" "$out/$family-source.aplink"
	run "$family-repeat" 0 "$backend" --cc "$cc" --ar "$ar" --link "$out/$family-source.aplink" "$out/$family-repeat"
	run "$family-repeat-exact" 0 diff -ru "$out/$family-source" "$out/$family-repeat"
	run "$family-trusted" 0 "$backend" --trust-image --steps 0 --link "$out/$family-source.aplink" "$out/$family-trusted"
	run "$family-trusted-source" 0 cmp "$out/$family-source/component.c" "$out/$family-trusted/component.c"
	run "$family-trusted-header" 0 cmp "$out/$family-source/component.h" "$out/$family-trusted/component.h"
	run "$family-no-fuel" 3 "$backend" --steps 0 --link "$out/$family-source.aplink" "$out/$family-no-fuel"
	run "$family-no-fuel-absent" 0 test ! -e "$out/$family-no-fuel"
	for name in "$dynamic" "$effect"; do
		sed '/^export /d' "$out/$family-source.aplink" >"$out/$family-$name.aplink"
		printf 'export %s %s\n' "$name" "$name" >>"$out/$family-$name.aplink"
		for mode in checked trusted; do
			options=()
			[[ $mode == trusted ]] && options=(--trust-image --steps 0)
			run "$family-$name-$mode" 4 "$backend" "${options[@]}" --link "$out/$family-$name.aplink" "$out/$family-$name-$mode"
			run "$family-$name-$mode-absent" 0 test ! -e "$out/$family-$name-$mode"
		done
	done
	sha256sum "$image" >"$out/$family-after.sha256"
	run "$family-image-unchanged" 0 cmp "$out/$family-before.sha256" "$out/$family-after.sha256"
done
printf 'Current capture query: actual static/transitive source/products/faults/inertness and retained pending/dynamic/effect refusals pass\n'
