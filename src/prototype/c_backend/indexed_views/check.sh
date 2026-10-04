#!/bin/sh
set -eu
test "$#" = 3
pointer=$1
emitter=$2
out=$3
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
cc=${CC:-cc}
mkdir -p "$out"

run()
{
	label=$1
	expect=$2
	shift 2
	printf '%s\0' "$label" "$expect" "$#" "$@" >>"$out/argv.nul"
	status=0
	"$@" >"$out/$label.out" 2>"$out/$label.err" || status=$?
	printf '%s\t%s\t%s\n' "$label" "$expect" "$status" >>"$out/status.tsv"
	test "$status" = "$expect"
}

run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 \
	--imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/families.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/families.a" "$out/indexed.h"
run emit_repeat 0 "$emitter" "$out/families.a" "$out/repeat.h"
run deterministic 0 cmp "$out/indexed.h" "$out/repeat.h"

# Independent expected symbolic source-index images catch constructor identity
# confusion. These comments describe images; they are not a new source checker.
run nat_index 0 grep -F 'c0 result [self, field0, nat_c1(field0)]' "$out/indexed.h"
run sized_index 0 grep -F 'c0 result [parameter0, self, nat_c0]' "$out/indexed.h"
run acc_down_index 0 grep -F 'result index: self(argument0)' "$out/indexed.h"
run acc_edge_indices 0 grep -F 'argument1: parameter1(argument0)(field0)' "$out/indexed.h"
run partition_parameter 0 grep -F 'const struct iv_nat *parameter1;' "$out/indexed.h"
run client_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out" "$here/client.c" -o "$out/client"
run client 0 "$out/client"
run wrong_relation 1 "$cc" -std=c11 -Wall -Wextra -Werror -O2 \
	-I"$out" -c "$here/wrong_relation.c" -o "$out/wrong.o"
printf 'Indexed/callable declarations: 12 expected rows; body lowering remains separate\n'
