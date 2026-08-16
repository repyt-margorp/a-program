#!/bin/sh
set -eu

# Boundary audit: ISSUE-10-INTRINSIC-ENVIRONMENT

. src/prototype/build/test_support.sh

prototype_compile c11 warnings compiler \
	/tmp/a-program-cbpv-boundary-check \
	src/prototype/tests/checks/cbpv_boundary_check.c

/tmp/a-program-cbpv-boundary-check
rm -f /tmp/a-program-cbpv-boundary-check

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT
cat >"$tmp_dir/boundary.p" <<'EOF'
delayed := &{ #1; };
main := { x := delayed; x; };
EOF

./read_file.out "$tmp_dir/boundary.p" >"$tmp_dir/output"
grep -q 'occurrence#[0-9][0-9]* return ' "$tmp_dir/output"
grep -q 'occurrence#[0-9][0-9]* thunk ' "$tmp_dir/output"
grep -q 'occurrence#[0-9][0-9]* force ' "$tmp_dir/output"
grep -q 'has-type RETURN(INT_LITERAL(1)) COMPUTATION_TYPE(EFFECT_ROW_EMPTY, PRIMITIVE(Int)) \[return-intro proof#' "$tmp_dir/output"
grep -q 'has-type THUNK(RETURN(INT_LITERAL(1))) Thunk(COMPUTATION_TYPE(EFFECT_ROW_EMPTY, PRIMITIVE(Int))) \[thunk-intro proof#' "$tmp_dir/output"
grep -q 'has-type FORCE(THUNK(RETURN(INT_LITERAL(1)))) COMPUTATION_TYPE(EFFECT_ROW_EMPTY, PRIMITIVE(Int)) \[force-elim proof#' "$tmp_dir/output"
./read_file.out --write-artifact "$tmp_dir/boundary.apo" \
	"$tmp_dir/boundary.p" >"$tmp_dir/boundary-write.out"
if grep -Eq '^(effect_constraints|effect_constraint) ' "$tmp_dir/boundary.apo"; then
	echo "CBPV artifact persisted compiler-local effect constraints" >&2
	exit 1
fi
