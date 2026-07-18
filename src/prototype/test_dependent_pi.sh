#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

./read_file.out src/prototype/dependent_pi_identity_check.p \
	>"$tmp_dir/identity.out"
grep -q '^term dependentIdentity := LAMBDA(' "$tmp_dir/identity.out"
grep -q '^has-type LAMBDA(.*PI(UNIVERSE' "$tmp_dir/identity.out"

./read_file.out src/prototype/dependent_pi_surface_check.p \
	>"$tmp_dir/match.out"
grep -q '^term choose := LAMBDA(' "$tmp_dir/match.out"
grep -q '^has-type LAMBDA(.*COMPUTATION_TYPE(EFFECT_LABEL(0), MATCH(' \
	"$tmp_dir/match.out"

./read_file.out --write-artifact "$tmp_dir/dependent-pi.apo" \
	src/prototype/dependent_pi_surface_check.p >"$tmp_dir/write.out"
./read_file.out --read-graph "$tmp_dir/dependent-pi.apo" \
	>"$tmp_dir/read.out"
grep -q '^interface term choose ' "$tmp_dir/read.out"

cat >"$tmp_dir/wrong-result.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };

choose := \b : Bool =>
	b
		@true => Nat.zero
		@false => Bool.true;

choose :: (b : Bool) -> Nat;
EOF

if ./read_file.out "$tmp_dir/wrong-result.p" \
	>"$tmp_dir/wrong-result.out" 2>"$tmp_dir/wrong-result.err"; then
	echo "invalid dependent result annotation unexpectedly passed" >&2
	exit 1
fi
