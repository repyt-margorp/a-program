#!/usr/bin/env bash
set -euo pipefail
pointer=$(realpath "${1:?qualified pointer-check}"); oracle=$(realpath "${2:?existing-Core observation helper}")
if [[ -n ${3:-} ]]; then output=$3
else temporary=$(mktemp -d); trap 'rm -rf "$temporary"' EXIT; output="$temporary/report"; fi
here=$(cd "$(dirname "$0")" && pwd); cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
test ! -e "$output"; mkdir -p "$output"; output=$(realpath "$output")
expect_status() {
	local name=$1 expected=$2 actual=0; shift 2
	printf '%q ' "$@" > "$output/$name.command"; printf '\n' >> "$output/$name.command"
	"$@" > "$output/$name.out" 2> "$output/$name.err" || actual=$?
	printf '%s\t%s\t%s\n' "$name" "$expected" "$actual" >> "$output/status.tsv"
	if [[ $actual != "$expected" ]]; then cat "$output/$name.out" "$output/$name.err" >&2; exit 1; fi
}
expect_status admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$here/../../../../tests/fixtures/sorted-proof-provider.p" --save "$output/quick.a" "$here/fixture.p"
sha256sum "$output/quick.a" > "$output/before.sha256"
expect_status source 0 "$pointer" --load --steps 5000000 --run reference "$output/quick.a"
expect_status client-build 0 "$cc" "${flags[@]}" "$here/mockup.c" "$here/client.c" -Wl,--wrap=malloc -o "$output/client"
expect_status client 0 "$output/client"
expect_status source-match 0 cmp "$output/source.out" "$output/client.out"
expect_status Core-observations 0 "$oracle" "$output/quick.a" "$output/oracle.c"
expect_status oracle-build 0 "$cc" "${flags[@]}" -I"$here" "$here/mockup.c" "$output/oracle.c" -o "$output/oracle"
expect_status oracle-client 0 "$output/oracle"
sha256sum "$output/quick.a" > "$output/after.sha256"
expect_status image-unchanged 0 cmp "$output/before.sha256" "$output/after.sha256"
printf 'Hand-authored actual Acc QuickSort candidate executable: retained proofs/indices/raw and folded down/captures; source/Core/resource controls pass; automatic/general native lowering remains open\n'
