#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
fixtures="$here/../fixtures"
if [[ -n ${3:-} ]]; then
	temporary=$3
	test ! -e "$temporary"
	mkdir -p "$temporary"
	temporary=$(realpath "$temporary")
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT
fi
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
expect_status() {
	local expected=$1 status=0
	shift
	"$@" > "$temporary/out" 2> "$temporary/err" || status=$?
	if [[ $status != "$expected" ]]; then
		cat "$temporary/out" "$temporary/err" >&2
		printf 'expected %s, got %s: %s\n' "$expected" "$status" "$*" >&2
		exit 1
	fi
}
expect_status 0 "$compiler" --save "$temporary/nested-records.a" "$fixtures/nested_records.p"
sha256sum "$temporary/nested-records.a" > "$temporary/before"
for product in source object archive; do
	sed "s/product source/product $product/" "$fixtures/nested_records.aplink" > "$temporary/$product.aplink"
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	grep -q 'nested-value-fields' "$temporary/$product/link.json"
	grep -q 'selected-value-data' "$temporary/$product/link.json"
	! grep -E 'ap_(apply|value|context)|malloc|calloc' "$temporary/$product/component.c"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/nested_record_client.c" "$input" -o "$temporary/client-$product"
	expect_status 0 "$temporary/client-$product"
	if [[ $product == source ]]; then cp "$temporary/out" "$temporary/generated"; fi
done
expect_status 0 "$compiler" --imports "$fixtures/nested_records.p" --run main "$fixtures/nested_records_differential.p"
cmp "$temporary/out" "$temporary/generated"
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
for change in '/^data Packet /d' '/^data Packet /{h;d;}; /^data Envelope /G'; do
	sed "$change" "$fixtures/nested_records.aplink" > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
for name in Callback Dependent; do
	cp "$fixtures/nested_records.aplink" "$temporary/bad.aplink"
	printf 'data %s Rejected\n' "$name" >> "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
# The exact former Recursive refusal is now a selected finite-record List.
cp "$fixtures/nested_records.aplink" "$temporary/recursive.aplink"
printf 'data Recursive Recursive\n' >> "$temporary/recursive.aplink"
expect_status 0 "$backend" --link "$temporary/recursive.aplink" "$temporary/recursive"
"$cc" "${flags[@]}" -DFORMER_NESTED_RECORD -I"$temporary/recursive" \
	"$here/../record_list/former_client.c" "$temporary/recursive/component.c" -o "$temporary/recursive-client"
expect_status 0 "$temporary/recursive-client"
cp "$fixtures/nested_records.aplink" "$temporary/bad.aplink"
printf 'data Chain Chain\n' >> "$temporary/bad.aplink"
expect_status 0 "$backend" --link "$temporary/bad.aplink" "$temporary/chain"
printf 'data ChainBox Rejected\n' >> "$temporary/bad.aplink"
expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
test ! -e "$temporary/bad"
test ! -s "$temporary/out"
sha256sum "$temporary/nested-records.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
printf 'Native value records: 105 cases per C product, 16 source observations, active nested validation, whole values and explicit field/order refusals passed\n'
