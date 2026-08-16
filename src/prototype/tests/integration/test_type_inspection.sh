#!/bin/sh
set -eu

# Boundary audit: ISSUE-9-READ-ONLY-TYPE-INSPECTION

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-type-inspection.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
. src/prototype/build/test_support.sh
make -f src/prototype/Makefile >/dev/null

prototype_compile c11 werror graph "$TMP_DIR/type-inspection-projection-check" \
	src/prototype/tests/checks/type_inspection_projection_check.c
"$TMP_DIR/type-inspection-projection-check"

SOURCE=src/prototype/tests/fixtures/typing/type_inspection_check.p

printf ':state\n:type identityBool\n:type identityNat\n:type selected\n:type literal\n:type missing\n:state\n:q\n' |
	./a.out "$SOURCE" >"$TMP_DIR/repl.out"

grep -Eq '^(prototype> )?type identityBool := PI\(TYPE_VIEW\(Bool,' "$TMP_DIR/repl.out"
grep -Eq '^(prototype> )?type identityNat := PI\(TYPE_VIEW\(Nat,' "$TMP_DIR/repl.out"
grep -q '^expected identityBool := PI(TYPE_VIEW(Bool,' "$TMP_DIR/repl.out"
grep -q '^expected identityNat := PI(TYPE_VIEW(Nat,' "$TMP_DIR/repl.out"
grep -Eq '^(prototype> )?type selected := APP\(LAMBDA\(' "$TMP_DIR/repl.out"
grep -Eq '^(prototype> )?type literal := PRIMITIVE\(Int\)$' "$TMP_DIR/repl.out"
grep -Eq '^expected literal := PRIMITIVE\(Int\) \[accepted claim#[0-9]+\]$' \
	"$TMP_DIR/repl.out"
grep -q 'missing has no available type inspection' "$TMP_DIR/repl.out"

# The REPL prints state once after loading and twice around the queries. Every
# selected storage counter must have one value across all three snapshots.
for pattern in \
	'^asts=' \
	'^types=' \
	'^judgements=' \
	'^labels=' \
	'^typed-occurrences=' \
	'^resolution_items=' \
	'^universe-levels='
do
	test "$(grep -E "$pattern" "$TMP_DIR/repl.out" | wc -l)" -eq 3
	test "$(grep -E "$pattern" "$TMP_DIR/repl.out" | sort -u | wc -l)" -eq 1
done

./read_file.out --write-artifact "$TMP_DIR/before.apo" "$SOURCE" >/dev/null
printf ':type literal\n:q\n' | ./a.out "$SOURCE" >/dev/null
./read_file.out --write-artifact "$TMP_DIR/after.apo" "$SOURCE" >/dev/null
cmp "$TMP_DIR/before.apo" "$TMP_DIR/after.apo"
test "$(wc -c <"$TMP_DIR/before.apo")" -eq "$(wc -c <"$TMP_DIR/after.apo")"

echo 'type inspection tests passed'
