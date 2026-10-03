#!/usr/bin/env bash
set -euo pipefail
backend=${1:?private applied a-to-c binary}
compiler=${2:?pinned pointer-check binary}
evidence=${3:?new evidence directory}
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$evidence"
mkdir -p "$evidence"
expect_status() {
	local expected=$1 status=0
	shift
	"$@" > "$evidence/out" 2> "$evidence/err" || status=$?
	if [[ $status != "$expected" ]]; then
		cat "$evidence/out" "$evidence/err" >&2
		printf 'expected %s, got %s: %s\n' "$expected" "$status" "$*" >&2
		exit 1
	fi
}
expect_status 0 "$compiler" --save "$evidence/applied-list.a" "$here/fixture.p"
cp "$evidence/out" "$evidence/save.log"
sha256sum "$evidence/applied-list.a" > "$evidence/before.sha256"
for selection in numbers flags reverse choice; do
	linker=()
	if [[ $selection == numbers ]]; then linker=(-Wl,--wrap=malloc); fi
	for product in source object archive; do
		script="$evidence/$selection-$product.aplink"
		sed "s|artifact applied-list.a|artifact $evidence/applied-list.a|; s/product source/product $product/" "$here/$selection.aplink" > "$script"
		directory="$evidence/$selection-$product"
		expect_status 0 "$backend" --link "$script" "$directory"
		test ! -e "$directory/runtime.c"
		grep -q 'value-classifier' "$directory/link.json"
		! grep -E 'ap_(apply|value|context)|pg_eval' "$directory/component.c"
		case "$product" in
		source) input="$directory/component.c" ;;
		object) input="$directory/component.o" ;;
		archive) input="$directory/library.a" ;;
		esac
		"$cc" "${flags[@]}" -I"$directory" "$here/${selection}_client.c" "$input" "${linker[@]}" -o "$evidence/client-$selection-$product"
		expect_status 0 "$evidence/client-$selection-$product"
		if [[ $selection == numbers && $product == source ]]; then cp "$evidence/out" "$evidence/generated"; fi
	done
	script="$evidence/$selection-source.aplink"
	expect_status 0 "$backend" --link "$script" "$evidence/$selection-repeated"
	diff -ru "$evidence/$selection-source" "$evidence/$selection-repeated"
	expect_status 0 "$backend" --trust-image --steps 0 --link "$script" "$evidence/$selection-trusted"
	cmp "$evidence/$selection-source/component.c" "$evidence/$selection-trusted/component.c"
	cmp "$evidence/$selection-source/component.h" "$evidence/$selection-trusted/component.h"
done
expect_status 0 "$compiler" --imports "$here/fixture.p" --run main "$here/differential.p"
cmp "$evidence/out" "$evidence/generated"
expect_status 3 "$backend" --steps 0 --link "$evidence/numbers-source.aplink" "$evidence/no-fuel"
test ! -e "$evidence/no-fuel"
for refusal in factory indexed missing duplicate unselected callback type_as_value; do
	script="$evidence/refuse-$refusal.aplink"
	sed '/^data_of /d; /^export /d' "$evidence/numbers-source.aplink" > "$script"
	case "$refusal" in
	factory) printf 'data_of list_type Rejected\nexport identity identity\n' >> "$script" ;;
	indexed) printf 'data_of empty_sized Rejected\nexport identity identity\n' >> "$script" ;;
	missing) printf 'data_of empty_flags Rejected\nexport identity_flags identity\n' >> "$script" ;;
	duplicate) printf 'enum32 Bool Bool\ndata_of empty Numbers\ndata_of empty_flags Flags\nexport identity identity\n' >> "$script" ;;
	unselected) printf 'data_of empty Numbers\nexport identity_flags identity\n' >> "$script" ;;
	callback) printf 'data_of empty Numbers\nexport callback callback\n' >> "$script" ;;
	type_as_value) printf 'data_of Nat Rejected\nexport identity identity\n' >> "$script" ;;
	esac
	for admission in checked trusted; do
		options=()
		if [[ $admission == trusted ]]; then options=(--trust-image --steps 0); fi
		expect_status 4 "$backend" "${options[@]}" --link "$script" "$evidence/refuse-$refusal-$admission"
		test ! -e "$evidence/refuse-$refusal-$admission"
		test ! -s "$evidence/out"
		cp "$evidence/err" "$evidence/refuse-$refusal-$admission.log"
	done
done
sed "s|artifact applied-list.a|artifact $evidence/applied-list.a|" "$here/baseline.aplink" > "$evidence/baseline.aplink"
expect_status 4 "$backend" --link "$evidence/baseline.aplink" "$evidence/baseline"
test ! -e "$evidence/baseline"
cp "$evidence/err" "$evidence/baseline.log"
sha256sum "$evidence/applied-list.a" > "$evidence/after.sha256"
cmp "$evidence/before.sha256" "$evidence/after.sha256"
printf 'Applied selections: 1093 Nat Lists, 127 enum Lists, 127 reversed Lists, 34 two-parameter values per source/object/archive product; 9 source observations; checked/trusted refusals and transactional limits pass\n'
