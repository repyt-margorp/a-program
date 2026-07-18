#!/bin/sh
set -eu

cc -std=c11 -Wall -Wextra -I src/prototype \
	src/prototype/cbpv_boundary_check.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o /tmp/a-program-cbpv-boundary-check

/tmp/a-program-cbpv-boundary-check
rm -f /tmp/a-program-cbpv-boundary-check

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT
cat >"$tmp_dir/boundary.p" <<'EOF'
delayed := &{ #1 };
main := { x := delayed; x };
EOF

./read_file.out "$tmp_dir/boundary.p" >"$tmp_dir/output"
grep -q 'operation#[0-9][0-9]* return ' "$tmp_dir/output"
grep -q 'operation#[0-9][0-9]* thunk ' "$tmp_dir/output"
grep -q 'operation#[0-9][0-9]* force ' "$tmp_dir/output"
grep -q 'has-type RETURN(INT_LITERAL(1)) COMPUTATION_TYPE(EFFECT_LABEL(0), PRIMITIVE(Int64)) \[return-intro\]' "$tmp_dir/output"
grep -q 'has-type THUNK(RETURN(INT_LITERAL(1))) Thunk(COMPUTATION_TYPE(EFFECT_LABEL(0), PRIMITIVE(Int64))) \[thunk-intro\]' "$tmp_dir/output"
grep -q 'has-type FORCE(THUNK(RETURN(INT_LITERAL(1)))) COMPUTATION_TYPE(EFFECT_LABEL(0), PRIMITIVE(Int64)) \[force-elim\]' "$tmp_dir/output"
./read_file.out --write-artifact "$tmp_dir/boundary.apo" \
	"$tmp_dir/boundary.p" >"$tmp_dir/boundary-write.out"
grep -Eq '^effect_constraint [0-9]+ 1 2 ' "$tmp_dir/boundary.apo"
