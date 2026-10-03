#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$here/../../.." && pwd)
checker=${1:?usage: check.sh CHECKER PROGRAM_TEST}
runtime=${2:?usage: check.sh CHECKER PROGRAM_TEST}
scratch=$(mktemp -d /tmp/ap-surface-gates.XXXXXX)
trap 'rm -rf "$scratch"' EXIT

for path in "$here"/fixtures/{ordered,permuted,assertion-free,omitted-dependent,both-valid}.p; do
	"$runtime" --equal "$path" graphMain expected
done
"$runtime" --equal "$here/fixtures/shadow-local.p" aliasMain expected
for name in main aliasMain graphMain valueMain; do
	"$runtime" --equal "$here/migrated/tests/acceptance/function-graph-named-fields.p" "$name" expected
done
for path in "$here"/fixtures/{ordered-reversed,unknown,lexical-source,wrong-case,duplicate-source,duplicate-local,invalid-graph,invalid-ih,shadow-graph,shadow-ih,wrong-assertion}.p; do
	"$runtime" --reject "$path"
done
"$runtime" --reject "$repo/tests/acceptance/function-graph-named-fields.p"
"$runtime" --equal "$here/migrated/tests/acceptance/function-graph-refined-case.p" main expected
"$runtime" --unsupported "$here/migrated/tests/acceptance/function-graph-named-not-call.p"
for name in function-graph-named-duplicate function-graph-named-wrong-proof function-graph-missing-case generated-packet-shadow generated-packet-wrong-output; do
	"$runtime" --reject "$here/migrated/tests/acceptance/$name.p"
done

for source in 'm:=g @c {{}}=>x;' 'm:=g @c {}=>x;' 'm:=g @c x {{y;}}=>x;' \
	'm:=g @c {{x:=@y;}}=>x;' 'm:=g @c {{x:=*y;}}=>x;' 'm:=g @c {{x:=y; }=>x;' \
	'm:=g @c {{{x:=y;}}}=>x;' 'm:=g @c {x:=y;}}=>x;'; do
	printf '%s\n' "$source" > "$scratch/invalid.p"
	code=0
	"$checker" "$scratch/invalid.p" > "$scratch/parse.log" 2>&1 || code=$?
	[[ $code == 1 ]]
	rg -q 'expected|named selectors' "$scratch/parse.log"
done

for path in "$here/fixtures/ordered.p" "$here/fixtures/permuted.p" "$here/fixtures/omitted-dependent.p"; do
	for steps in 0 1000000; do
		code=0
		"$checker" --steps "$steps" --save "$scratch/source.a" "$path" > "$scratch/save.log" 2>&1 || code=$?
		[[ $code == 0 || ( $steps == 0 && $code == 3 ) ]]
		"$runtime" --equal-image "$scratch/source.a" graphMain expected
	done
done
"$checker" --steps 1000000 --save "$scratch/rejected.a" "$here/fixtures/duplicate-source.p" > "$scratch/reject.log" 2>&1 && exit 1
"$checker" --steps 1000000 --load "$scratch/rejected.a" > "$scratch/reload.log" 2>&1 && exit 1
rg -q '^rejected steps=' "$scratch/reload.log"
printf '%s\n' 'surface gates: polarity, brace modes, scope/roles, migration, source/image reload and rejection passed'
