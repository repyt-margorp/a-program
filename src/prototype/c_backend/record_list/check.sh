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
no_product() {
	test ! -e "$1"
	local paths=("$1".tmp.*)
	test ! -e "${paths[0]}"
}
expect_status admit 0 "$compiler" --save "$output/records.a" "$here/fixture.p"
sha256sum "$output/records.a" > "$output/before.sha256"
for product in source object archive; do
	sed "s/product source/product $product/" "$here/records.aplink" > "$output/$product.aplink"
	expect_status "emit-$product" 0 "$backend" --link "$output/$product.aplink" "$output/$product"
	test ! -e "$output/$product/runtime.c"
	grep -q 'selected-value-data-single-self-tail' "$output/$product/link.json"
	grep -q 'list_copy_in.*arena' "$output/$product/link.json"
	case "$product" in
	source) input="$output/source/component.c" ;;
	object) input="$output/object/component.o" ;;
	archive) input="$output/archive/library.a" ;;
	esac
	expect_status "compile-$product" 0 "$cc" "${flags[@]}" -I"$output/$product" "$here/client.c" "$input" -Wl,--wrap=malloc -o "$output/client-$product"
	expect_status "client-$product" 0 "$output/client-$product"
	cmp "$output/client-source.out" "$output/client-$product.out"
done
expect_status differential 0 "$compiler" --imports "$here/fixture.p" --run main "$here/differential.p"
cmp "$output/differential.out" "$output/client-source.out"
expect_status repeated 0 "$backend" --link "$output/source.aplink" "$output/repeated"
cmp "$output/source/component.c" "$output/repeated/component.c"
cmp "$output/source/component.h" "$output/repeated/component.h"
expect_status trusted 0 "$backend" --trust-image --steps 0 --link "$output/source.aplink" "$output/trusted"
cmp "$output/source/component.c" "$output/trusted/component.c"
cmp "$output/source/component.h" "$output/trusted/component.h"
for change in missing-packet later-packet missing-envelope later-envelope; do
	case "$change" in
	missing-packet) sed '/^data Packet /d' "$output/source.aplink" > "$output/$change.aplink" ;;
	later-packet) sed '/^data Packet /{h;d;}; /^data Envelope /G' "$output/source.aplink" > "$output/$change.aplink" ;;
	missing-envelope) sed '/^data Envelope /d' "$output/source.aplink" > "$output/$change.aplink" ;;
	later-envelope) sed '/^data Envelope /{h;d;}; /^data Records /G' "$output/source.aplink" > "$output/$change.aplink" ;;
	esac
done
for name in ChainBox Aggregate DirectAggregate MultiPayload Tree Callback Dependent Indexed; do
	cp "$output/source.aplink" "$output/$name.aplink"
	case "$name" in
	ChainBox|Aggregate|DirectAggregate) printf 'data Chain Chain\n' >> "$output/$name.aplink" ;;
	Indexed) printf 'nat32 Nat Nat\n' >> "$output/$name.aplink" ;;
	esac
	[[ $name == Aggregate ]] && printf 'data ChainBox ChainBox\n' >> "$output/$name.aplink"
	printf 'data %s Rejected\n' "$name" >> "$output/$name.aplink"
done
for profile in missing-packet later-packet missing-envelope later-envelope ChainBox Aggregate DirectAggregate MultiPayload Tree Callback Dependent Indexed; do
	for trust in checked trusted; do
		options=()
		[[ $trust == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$profile-$trust" 4 "$backend" "${options[@]}" --link "$output/$profile.aplink" "$output/$profile-$trust"
		no_product "$output/$profile-$trust"
		test ! -s "$output/$profile-$trust.out"
	done
done
sha256sum "$output/records.a" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Finite value-record Lists: 259 cases/product, 13 source observations, nested active validation, reversed fields, transactional arrays and 24 checked/trusted refusals pass\n'
