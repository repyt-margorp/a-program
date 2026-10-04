#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
if [[ -n ${3:-} ]]; then
	temporary=$3; test ! -e "$temporary"; mkdir -p "$temporary"; temporary=$(realpath "$temporary")
else
	temporary=$(mktemp -d); trap 'rm -rf "$temporary"' EXIT
fi
mkdir "$temporary/logs"
command_count=0
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
expect_status() {
	local expected=$1 status=0
	shift
	command_count=$((command_count + 1))
	printf '%q ' "$@" > "$temporary/logs/$command_count.command"
	printf '\n' >> "$temporary/logs/$command_count.command"
	"$@" > "$temporary/out" 2> "$temporary/err" || status=$?
	cp "$temporary/out" "$temporary/logs/$command_count.out"
	cp "$temporary/err" "$temporary/logs/$command_count.err"
	printf '%s\t%s\t%s\n' "$command_count" "$expected" "$status" >> "$temporary/status.tsv"
	if [[ $status != "$expected" ]]; then
		cat "$temporary/out" "$temporary/err" >&2
		printf 'expected %s, got %s: %s\n' "$expected" "$status" "$*" >&2
		exit 1
	fi
}
expect_status 0 "$compiler" --save "$temporary/fixture.a" "$here/list_fixture.p"
sha256sum "$temporary/fixture.a" > "$temporary/before"
for product in source object archive executable; do
	sed "s/product source/product $product/" "$here/list.aplink" > "$temporary/$product.aplink"
	if [[ $product == executable ]]; then printf 'entry constant\n' >> "$temporary/$product.aplink"; fi
	expect_status 0 "$backend" --link "$temporary/$product.aplink" "$temporary/$product"
	test ! -e "$temporary/$product/runtime.c"
	grep -q 'borrowed-inputs-and-caller-arena' "$temporary/$product/link.json"
	grep -q 'direct-recursive-match' "$temporary/$product/link.json"
	! grep -E 'ap_(apply|value|context)|pg_eval' "$temporary/$product/component.c"
	case "$product" in
	source) input="$temporary/source/component.c" ;;
	object) input="$temporary/object/component.o" ;;
	archive) input="$temporary/archive/library.a" ;;
	executable) expect_status 0 "$temporary/executable/program"; continue ;;
	esac
	"$cc" "${flags[@]}" -I"$temporary/$product" "$here/list_client.c" "$input" -Wl,--wrap=malloc -o "$temporary/client"
	expect_status 0 "$temporary/client"
done
expect_status 0 "$backend" --link "$temporary/source.aplink" "$temporary/repeated"
diff -ru "$temporary/source" "$temporary/repeated"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/source.aplink" "$temporary/trusted"
cmp "$temporary/source/component.c" "$temporary/trusted/component.c"
cmp "$temporary/source/component.h" "$temporary/trusted/component.h"
expect_status 3 "$backend" --steps 0 --link "$temporary/source.aplink" "$temporary/no-fuel"
test ! -e "$temporary/no-fuel"
"$cc" "${flags[@]}" -I"$temporary/source" "$here/list_differential.c" "$temporary/source/component.c" -o "$temporary/differential"
expect_status 0 "$temporary/differential"
cp "$temporary/out" "$temporary/generated"
expect_status 0 "$compiler" --imports "$here/list_fixture.p" --run main "$here/list_differential.p"
cmp "$temporary/out" "$temporary/generated"
sed '/^data /d; /^export /d' "$here/list.aplink" > "$temporary/tree.aplink"
printf 'data Tree Rejected\nexport tree_identity tree\n' >> "$temporary/tree.aplink"
expect_status 0 "$backend" --link "$temporary/tree.aplink" "$temporary/tree"
"$cc" "${flags[@]}" -I"$temporary/tree" "$here/tree_client.c" "$temporary/tree/component.c" -o "$temporary/tree-client"
expect_status 0 "$temporary/tree-client"
expect_status 0 "$backend" --trust-image --steps 0 --link "$temporary/tree.aplink" "$temporary/tree-trusted"
cmp "$temporary/tree/component.c" "$temporary/tree-trusted/component.c"
for name in ThunkList; do
	sed '/^data /d; /^export /d' "$here/list.aplink" > "$temporary/bad.aplink"
	printf 'data %s Rejected\nexport thunk_identity rejected\n' "$name" >> "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
done
for name in callback effect; do
	sed '/^export /d' "$here/list.aplink" > "$temporary/bad.aplink"
	printf 'export %s rejected\n' "$name" >> "$temporary/bad.aplink"
	expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
	test ! -e "$temporary/bad"
	test ! -s "$temporary/out"
done
sed '/^data /d' "$here/list.aplink" > "$temporary/bad.aplink"
expect_status 4 "$backend" --link "$temporary/bad.aplink" "$temporary/bad"
test ! -e "$temporary/bad"
sha256sum "$temporary/fixture.a" > "$temporary/after"
cmp "$temporary/before" "$temporary/after"
printf 'Native list C: length/sum/append/stable selection, ownership, validation, resource failures, products and evaluator agreement passed\n'
