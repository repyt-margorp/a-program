#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/shared-id.p" <<'EOF'
identityInt := \x : #.Int => x;
identityText := \y : #.Text => y;
intResult := identityInt #1;
textResult := identityText #"x";
EOF

./read_file.out "$tmp_dir/shared-id.p" >"$tmp_dir/output"

int_term=$(sed -n 's/^metadata label identityInt -> operation#[0-9][0-9]* -> term#\([0-9][0-9]*\)$/\1/p' "$tmp_dir/output")
text_term=$(sed -n 's/^metadata label identityText -> operation#[0-9][0-9]* -> term#\([0-9][0-9]*\)$/\1/p' "$tmp_dir/output")

test -n "$int_term"
test "$int_term" = "$text_term"
grep -q 'has-type LAMBDA(_#0, RETURN(VAR(_#0))) PI(PRIMITIVE(Int),' "$tmp_dir/output"
grep -q 'has-type LAMBDA(_#0, RETURN(VAR(_#0))) PI(PRIMITIVE(Text),' "$tmp_dir/output"
grep -q 'metadata label intResult ' "$tmp_dir/output"
grep -q 'metadata label textResult ' "$tmp_dir/output"
