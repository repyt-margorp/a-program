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
expect_status 0 "$compiler" --save "$temporary/fixture.a" "$fixtures/enum_list.p"
sha256sum "$temporary/fixture.a" > "$temporary/before"
for product in source object archive; do
	sed "s/product source/product $product/" "$fixtures/enum_list.aplink" > "$temporary/$product.aplink"
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	grep -q 'finite-list-copy-out' "$temporary/$product/link.json"
	grep -q 'array-to-list-copy' "$temporary/$product/link.json"
	! grep -E 'ap_(apply|value|context)|pg_eval' "$temporary/$product/component.c"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/enum_list_client.c" "$input" -Wl,--wrap=malloc -o "$temporary/client-$product"
	expect_status 0 "$temporary/client-$product"
	if [[ $product == source ]]; then cp "$temporary/out" "$temporary/generated"; fi
done
expect_status 0 "$compiler" --imports "$fixtures/enum_list.p" --run main "$fixtures/enum_list_differential.p"
cmp "$temporary/out" "$temporary/generated"
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$temporary/source.aplink" "$temporary/no-fuel"
test ! -e "$temporary/no-fuel"
if printf '#include "component.h"\nint main(void) {struct ap_c_arena a={0}; struct ap_enum_Flag f={0}; const struct ap_data_Signals *s; return ap_from_Signals(&a,&f,1,&s);}\n' |
	"$cc" "${flags[@]}" -I"$temporary/source" -x c -c -o "$temporary/bad.o" - > "$temporary/out" 2> "$temporary/err"; then exit 1; fi
for change in '/^enum32 Flag /d' '/^enum32 Signal /d'; do
	sed "$change" "$fixtures/enum_list.aplink" > "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for name in callback effect; do
	sed '/^export /d' "$fixtures/enum_list.aplink" > "$temporary/bad.aplink"
	printf 'export %s Rejected\n' "$name" >> "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
cp "$fixtures/enum_list.aplink" "$temporary/bad.aplink"
printf 'data Pairs Pairs\n' >> "$temporary/bad.aplink"
expect_status 0 "$backend" --link "$temporary/bad.aplink" "$temporary/multi-payload"
! grep -E '^int ap_(copy|from)_Pairs' "$temporary/multi-payload/component.h"
# The exact former finite-value Aggregates refusal now exposes List arrays.
cp "$fixtures/enum_list.aplink" "$temporary/aggregates.aplink"
printf 'data Value Value\ndata Aggregates Aggregates\n' >> "$temporary/aggregates.aplink"
expect_status 0 "$backend" --link "$temporary/aggregates.aplink" "$temporary/aggregates"
"$cc" "${flags[@]}" -I"$temporary/aggregates" "$here/../record_list/former_client.c" \
	"$temporary/aggregates/component.c" -o "$temporary/aggregates-client"
expect_status 0 "$temporary/aggregates-client"
cp "$fixtures/enum_list.aplink" "$temporary/bad.aplink"
printf 'data Tree Rejected\n' >> "$temporary/bad.aplink"
printf 'export tree_identity tree\n' >> "$temporary/bad.aplink"
expect_status 0 "$backend" --link "$temporary/bad.aplink" "$temporary/tree"
"$cc" "${flags[@]}" -DTREE_DESTROY=ap_arena_Flags_destroy -I"$temporary/tree" \
	"$here/tree_client.c" "$temporary/tree/component.c" -o "$temporary/tree-client"
expect_status 0 "$temporary/tree-client"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/bad.aplink" "$temporary/tree-trusted"
cmp "$temporary/tree/component.c" "$temporary/tree-trusted/component.c"
sha256sum "$temporary/fixture.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
printf 'Native enum arrays: 875 cases per C product, 24 source observations, transactional input validation/rollback and 300-node conversion passed\n'
