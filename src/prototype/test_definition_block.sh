#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

./read_file.out --write-artifact "$tmp_dir/definition.apo" \
	src/prototype/definition_block_check.p >"$tmp_dir/definition.out"
grep -q '^term id := THUNK(LAMBDA(' "$tmp_dir/definition.out"
grep -q '^term main := THUNK(RETURN(INT_LITERAL(1)))$' \
	"$tmp_dir/definition.out"
grep -q '^A_PROGRAM_ARTIFACT 64$' "$tmp_dir/definition.apo"
awk '$1 == "compile_policy" && $2 == 2 && $3 == 1 && $4 == "main" &&
	$5 != 4294967295 && $6 != 4294967295 && $7 != 4294967295 {
	found = 1
}
END { exit found ? 0 : 1 }' "$tmp_dir/definition.apo"
./read_file.out --read-graph "$tmp_dir/definition.apo" \
	>"$tmp_dir/definition-read.out"
grep -q '^interface term main ' "$tmp_dir/definition-read.out"

cat >"$tmp_dir/support.p" <<'EOF_SOURCE'
support := #2;
EOF_SOURCE
./read_file.out --write-artifact "$tmp_dir/support.apo" \
	"$tmp_dir/support.p" >"$tmp_dir/support.out"
./read_file.out --aggregate-artifact "$tmp_dir/selected-link.apo" \
	"$tmp_dir/support.apo" "$tmp_dir/definition.apo" \
	>"$tmp_dir/selected-link.out"
awk '$1 == "compile_policy" && $4 == "main" { found = 1 }
END { exit found ? 0 : 1 }' "$tmp_dir/selected-link.apo"

cat >"$tmp_dir/other-entry.p" <<'EOF_SOURCE'
{{
	other := { #2; };
}}.other
EOF_SOURCE
./read_file.out --write-artifact "$tmp_dir/other-entry.apo" \
	"$tmp_dir/other-entry.p" >"$tmp_dir/other-entry.out"
if ./read_file.out --aggregate-artifact "$tmp_dir/two-entries.apo" \
	"$tmp_dir/definition.apo" "$tmp_dir/other-entry.apo" \
	>"$tmp_dir/two-entries.out" 2>"$tmp_dir/two-entries.err"; then
	echo 'artifact aggregation silently selected one of two entries' >&2
	exit 1
fi

if ./read_file.out --no-implicit-definition-thunks \
	src/prototype/definition_block_check.p >"$tmp_dir/implicit-disabled.out" \
	2>"$tmp_dir/implicit-disabled.err"; then
	echo 'explicit definition policy accepted a bare computation' >&2
	exit 1
fi
grep -q 'metadata resolve-error kind=compile' "$tmp_dir/implicit-disabled.err"

./read_file.out --no-implicit-definition-thunks \
	src/prototype/definition_block_explicit_check.p \
	>"$tmp_dir/explicit.out"
grep -q '^term id := THUNK(LAMBDA(' "$tmp_dir/explicit.out"
grep -q '^term main := THUNK(RETURN(INT_LITERAL(1)))$' "$tmp_dir/explicit.out"

./read_file.out src/prototype/definition_block_forward_check.p \
	>"$tmp_dir/forward.out"
grep -q '^term main := THUNK(APP(FORCE(THUNK(LAMBDA(' "$tmp_dir/forward.out"

./read_file.out --write-artifact "$tmp_dir/typed-shared-core.apo" \
	src/prototype/typed_shared_core_definition_check.p \
	>"$tmp_dir/typed-shared-core.out"
identity_bool_term=$(awk '/metadata label identityBool -> operation#[0-9]+ -> term#/ {
	sub("term#", "", $7); print $7
}' "$tmp_dir/typed-shared-core.out")
identity_nat_term=$(awk '/metadata label identityNat -> operation#[0-9]+ -> term#/ {
	sub("term#", "", $7); print $7
}' "$tmp_dir/typed-shared-core.out")
identity_bool_classifier=$(awk '$1 == "term" && $2 == "identityBool" { print $4 }' \
	"$tmp_dir/typed-shared-core.apo")
identity_nat_classifier=$(awk '$1 == "term" && $2 == "identityNat" { print $4 }' \
	"$tmp_dir/typed-shared-core.apo")
test "$identity_bool_term" = "$identity_nat_term"
test "$identity_bool_classifier" != "$identity_nat_classifier"

./read_file.out src/prototype/definition_block_effect_check.p \
	>"$tmp_dir/effect-compile.out"
if grep -qx 'definition-entry' "$tmp_dir/effect-compile.out"; then
	echo 'definition entry executed while compiling' >&2
	exit 1
fi
printf ':q\n' | ./a.out src/prototype/definition_block_effect_check.p \
	>"$tmp_dir/effect-run.out"
test "$(grep -c '^definition-entry$' "$tmp_dir/effect-run.out")" -eq 1
grep -q '^value main := RETURN(TEXT_LITERAL("definition-entry"))$' \
	"$tmp_dir/effect-run.out"

cat >"$tmp_dir/missing-selection.p" <<'EOF_SOURCE'
{{
	value := #1;
}}.missing
EOF_SOURCE
if ./read_file.out "$tmp_dir/missing-selection.p" \
	>"$tmp_dir/missing-selection.out" 2>"$tmp_dir/missing-selection.err"; then
	echo 'definition block accepted an unknown selected name' >&2
	exit 1
fi
grep -q 'selected definition is not a direct member' \
	"$tmp_dir/missing-selection.err"

cat >"$tmp_dir/duplicate-definition.p" <<'EOF_SOURCE'
{{
	main := { #1; };
	main := { #2; };
}}.main
EOF_SOURCE
if ./read_file.out "$tmp_dir/duplicate-definition.p" \
	>"$tmp_dir/duplicate-definition.out" \
	2>"$tmp_dir/duplicate-definition.err"; then
	echo 'definition block accepted duplicate definitions' >&2
	exit 1
fi
grep -Eq 'selected definition is ambiguous|duplicate definition' \
	"$tmp_dir/duplicate-definition.err"

echo 'definition block tests passed'
