#!/bin/sh
set -eu

# PR20: exact conditional authority must survive publication and must not be
# confused with an unconditional typing Claim.

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-meaning-boundaries.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

cd "$root_dir"
. src/prototype/build/test_support.sh
prototype_test_timing_initialize "$tmp_dir"
make -f src/prototype/Makefile reader >/dev/null

prototype_test_phase accepted_and_conditional
./read_file.out --write-artifact "$tmp_dir/accepted.apo" examples/07_add.p \
	>"$tmp_dir/accepted.out"
grep -Eq '^term main .* evidence 1 [0-9]+ 0$' "$tmp_dir/accepted.apo"

conditional_source=src/prototype/tests/fixtures/typing/explicit_index_family_map_check.p
conditional_export=map
./read_file.out --write-artifact "$tmp_dir/conditional.apo" \
	"$conditional_source" >"$tmp_dir/conditional.out"
./read_file.out --write-artifact "$tmp_dir/conditional-repeat.apo" \
	"$conditional_source" >"$tmp_dir/conditional-repeat.out"
cmp "$tmp_dir/conditional.apo" "$tmp_dir/conditional-repeat.apo"
grep -Eq "^term $conditional_export .* occurrence [0-9]+ evidence 2 [0-9]+ 1 [0-9]+$" \
	"$tmp_dir/conditional.apo"
grep -qx 'verification_obligations 1' "$tmp_dir/conditional.apo"
grep -qx 'verification_dependencies 1' "$tmp_dir/conditional.apo"
grep -Eq '^verification 0 2 1 ' "$tmp_dir/conditional.apo"
./read_file.out --read-graph "$tmp_dir/conditional.apo" \
	>"$tmp_dir/conditional-read.out"
if ./read_file.out --policy strict "$conditional_source" \
	>"$tmp_dir/conditional-strict.out" 2>"$tmp_dir/conditional-strict.err"
then
	echo 'strict policy accepted conditional computation evidence' >&2
	exit 1
fi

prototype_test_phase relocation_and_import
./read_file.out --aggregate-artifact "$tmp_dir/conditional-aggregate.apo" \
	"$tmp_dir/conditional.apo" >"$tmp_dir/conditional-aggregate.out"
./read_file.out --read-graph "$tmp_dir/conditional-aggregate.apo" \
	>"$tmp_dir/conditional-aggregate-read.out"
test "$(awk '$1 == "verification" { print $2 ":" $3 ":" $5 ":" $6 }' \
	"$tmp_dir/conditional.apo")" = \
	"$(awk '$1 == "verification" { print $2 ":" $3 ":" $5 ":" $6 }' \
	"$tmp_dir/conditional-aggregate.apo")"
test "$(awk '$1 == "verification_dependency" { print $3 ":" $4 }' \
	"$tmp_dir/conditional.apo")" = \
	"$(awk '$1 == "verification_dependency" { print $3 ":" $4 }' \
	"$tmp_dir/conditional-aggregate.apo")"

printf '%s\n' 'alias := map;' >"$tmp_dir/conditional-consumer.p"
if ./read_file.out --import-interface "$tmp_dir/conditional.apo" \
	"$tmp_dir/conditional-consumer.p" >"$tmp_dir/conditional-consumer.out" \
	2>"$tmp_dir/conditional-consumer.err"
then
	echo 'conditional import lost its provider evidence at first use' >&2
	exit 1
fi
grep -q 'failed to compile AST graph' "$tmp_dir/conditional-consumer.err"

expect_read_failure() {
	artifact=$1
	message=$2
	if ./read_file.out --read-graph "$artifact" \
		>"$artifact.out" 2>"$artifact.err"
	then
		echo "$message" >&2
		exit 1
	fi
}

prototype_test_phase forged_dependencies
awk '
	$1 == "verification_dependency" { $3 = $3 - 1 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-occurrence.apo"
expect_read_failure "$tmp_dir/wrong-occurrence.apo" \
	'wrong residual dependency occurrence was accepted'

awk '
	$1 == "verification_dependency" { $4 = 99 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-obligation.apo"
expect_read_failure "$tmp_dir/wrong-obligation.apo" \
	'unknown residual dependency ID was accepted'

awk '
	$1 == "verification_dependencies" { $2 = 0; skip = 1; print; next }
	skip && $1 == "verification_dependency" { skip = 0; next }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/missing-dependency.apo"
expect_read_failure "$tmp_dir/missing-dependency.apo" \
	'pending obligation without its source dependency was accepted'

awk '
	$1 == "verification_obligations" { $2 = 2 }
	$1 == "verification" {
		print
		$2 = 1
		print
		next
	}
	$1 == "verification_dependencies" { $2 = 2 }
	$1 == "verification_dependency" {
		print
		$2 = 1
		$4 = 1
		print
		next
	}
	$1 == "term" && $2 == "map" {
		for (i = 1; i <= NF; ++i) {
			if ($i == "evidence") {
				$(i + 3) = 2
				$(NF + 1) = 1
				break
			}
		}
	}
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/duplicate-obligation.apo"
expect_read_failure "$tmp_dir/duplicate-obligation.apo" \
	'duplicate residual obligation was accepted as a distinct condition'

# A computation-fold residual is valid conditional evidence for its own
# occurrence, but cannot replace the exact effect-row condition of map.
fold_source=src/prototype/tests/fixtures/effects/dependent_handler_result_check.p
./read_file.out --write-artifact "$tmp_dir/fold.apo" "$fold_source" \
	>"$tmp_dir/fold.out"
./read_file.out --aggregate-artifact "$tmp_dir/mixed.apo" \
	"$tmp_dir/conditional.apo" "$tmp_dir/fold.apo" >"$tmp_dir/mixed.out"
fold_obligation=$(awk '$1 == "verification" && $3 == 1 { print $2; exit }' \
	"$tmp_dir/mixed.apo")
if [ -z "$fold_obligation" ]; then
	echo 'mixed artifact did not retain its computation-fold obligation' >&2
	exit 1
fi
awk '
	$1 == "term" && $2 == "map" {
		for (i = 1; i <= NF; ++i) {
			if ($i == "evidence" && $(i + 1) == 2) {
				$NF = replacement
				done = 1
				break
			}
		}
	}
	{ print }
	END { if (!done) exit 1 }
' replacement="$fold_obligation" "$tmp_dir/mixed.apo" \
	>"$tmp_dir/unrelated-condition.apo"
expect_read_failure "$tmp_dir/unrelated-condition.apo" \
	'unrelated computation-fold obligation authorized an effect-conditional export'

awk '
	$1 == "term" && $2 == "map" {
		for (i = 1; i <= NF; ++i) {
			if ($i == "evidence") {
				$(i + 2) = 0
				break
			}
		}
	}
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-base-claim.apo"
expect_read_failure "$tmp_dir/wrong-base-claim.apo" \
	'conditional export accepted an unrelated base Claim'

awk '
	$1 == "term" && $2 == "map" { $4 = 0 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-classifier.apo"
expect_read_failure "$tmp_dir/wrong-classifier.apo" \
	'conditional export accepted a forged classifier'

awk '
	$1 == "verification" { $11 = $10 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/unexpected-family.apo"
expect_read_failure "$tmp_dir/unexpected-family.apo" \
	'exact effect equation accepted an unexpected right row'

awk '
	$1 == "verification" { $13 = 99 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-effect-kind.apo"
expect_read_failure "$tmp_dir/wrong-effect-kind.apo" \
	'effect equation accepted an unknown constraint kind'

awk '
	$1 == "verification" { $14 = 3 }
	{ print }
' "$tmp_dir/conditional.apo" >"$tmp_dir/wrong-effect-profile.apo"
expect_read_failure "$tmp_dir/wrong-effect-profile.apo" \
	'effect equation accepted a noncanonical normalization profile'

prototype_test_phase effect_is_not_typing
cat >"$tmp_dir/effect-only.p" <<'EOF_EFFECT_ONLY'
Nat := @{
	zero : *;
	succ : * -> *;
};

idNat : Nat -> Nat;
main := idNat Nat.zero;
EOF_EFFECT_ONLY
if ./read_file.out --write-artifact "$tmp_dir/effect-only.apo" \
	"$tmp_dir/effect-only.p" >"$tmp_dir/effect-only.out" \
	2>"$tmp_dir/effect-only.err"
then
	echo 'an effect equation replaced missing typing evidence' >&2
	exit 1
fi
grep -q 'failed to write artifact' "$tmp_dir/effect-only.err"

prototype_test_phase_finish
echo 'PR20 meaning boundary tests passed'
