#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
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

compare_entry() {
	local image=$1 entry=$2
	sha256sum "$image" > "$temporary/before"
	expect_status 0 "$backend" "$image" "$entry" "$temporary/entry.c"
	test ! -s "$temporary/out"
	"$cc" "${flags[@]}" -I"$here" "$temporary/entry.c" "$here/runtime.c" -o "$temporary/run"
	expect_status 0 "$temporary/run"
	cp "$temporary/out" "$temporary/generated"
	expect_status 0 "$compiler" --run "$entry" --load "$image"
	cmp "$temporary/out" "$temporary/generated"
	sha256sum "$image" > "$temporary/after"
	cmp "$temporary/before" "$temporary/after"
	# Relocation/rechecking cannot change generated C or execute its effects.
	cp "$temporary/entry.c" "$temporary/first.c"
	expect_status 0 "$backend" "$image" "$entry" "$temporary/entry.c"
	cmp "$temporary/first.c" "$temporary/entry.c"
	printf 'C differential: %s/%s passed\n' "$(basename "$image")" "$entry"
}

for fixture in effects arithmetic handler recursion generic; do
	expect_status 0 "$compiler" --save "$temporary/$fixture.a" "$here/fixtures/$fixture.p"
done
for entry in main unused shared repeated quoted forced_twice captured higher value empty; do
	compare_entry "$temporary/effects.a" "$entry"
done
compare_entry "$temporary/arithmetic.a" main
for entry in main duplicated forwarded aborted; do compare_entry "$temporary/handler.a" "$entry"; done
compare_entry "$temporary/recursion.a" main
compare_entry "$temporary/recursion.a" nominal
compare_entry "$temporary/generic.a" main
cp "$temporary/entry.c" "$temporary/fixed.c"
expect_status 0 "$backend" --image-limit none "$temporary/generic.a" main "$temporary/entry.c"
cmp "$temporary/fixed.c" "$temporary/entry.c"
expect_status 0 "$compiler" --save-inputs "$temporary/recompute.a" "$here/fixtures/generic.p"
compare_entry "$temporary/recompute.a" main

# C emission has no side effects, even when a delayed computation was retained.
expect_status 3 "$compiler" --steps 0 --save "$temporary/pending.a" "$here/fixtures/effects.p"
compare_entry "$temporary/pending.a" main
cp "$temporary/entry.c" "$temporary/unchanged.c"
expect_status 3 "$backend" --steps 0 "$temporary/effects.a" main "$temporary/entry.c"
cmp "$temporary/entry.c" "$temporary/unchanged.c"
expect_status 3 "$backend" --steps 0 --image-limit none "$temporary/effects.a" main "$temporary/entry.c"
cmp "$temporary/entry.c" "$temporary/unchanged.c"
expect_status 4 "$backend" "$temporary/effects.a" function "$temporary/entry.c"
cmp "$temporary/entry.c" "$temporary/unchanged.c"
for invalid in invalid invalid-assert; do
	expect_status 1 "$compiler" --save "$temporary/invalid.a" "$here/fixtures/$invalid.p"
	expect_status 1 "$backend" "$temporary/invalid.a" main "$temporary/entry.c"
	cmp "$temporary/entry.c" "$temporary/unchanged.c"
	expect_status 1 "$backend" --image-limit none "$temporary/invalid.a" main "$temporary/entry.c"
	cmp "$temporary/entry.c" "$temporary/unchanged.c"
done
expect_status 2 "$backend" "$temporary/effects.a" main "$temporary/effects.a"
expect_status 2 "$backend" --steps -1 "$temporary/effects.a" main "$temporary/entry.c"
expect_status 2 "$backend" --image-limit 0 "$temporary/effects.a" main "$temporary/entry.c"
expect_status 2 "$backend" --image-limit 1 "$temporary/effects.a" main "$temporary/entry.c"
test -z "$(find "$temporary" -name '*.tmp.*' -print)"

printf 'main:=#print #"a\0b";\n' > "$temporary/bytes.p"
expect_status 0 "$compiler" --save "$temporary/bytes.a" "$temporary/bytes.p"
compare_entry "$temporary/bytes.a" main
printf 'a\0b' > "$temporary/expected"
cmp "$temporary/generated" "$temporary/expected"
if [[ -c /dev/full ]]; then
	status=0
	"$temporary/run" > /dev/full 2> "$temporary/err" || status=$?
	test "$status" = 2
	grep -q 'output error (not retried)' "$temporary/err"
fi
printf 'C backend: standalone C, effects, closures, arithmetic, recursive ADTs and failure gates passed\n'
