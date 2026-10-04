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
	--imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/folds.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/folds.a" "$out/fold.c" "$out/fold.h" "$out/indexed.h"
run repeat 0 "$emitter" "$out/folds.a" "$out/repeat.c" "$out/repeat.h" "$out/repeat-indexed.h"
run same_source 0 cmp "$out/fold.c" "$out/repeat.c"
run same_header 0 cmp "$out/fold.h" "$out/repeat.h"
run same_data 0 cmp "$out/indexed.h" "$out/repeat-indexed.h"
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/folds.a"
run client_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out" "$out/fold.c" "$here/client.c" -o "$out/client"
run client 0 "$out/client"
run source_match 0 cmp "$out/source.out" "$out/client.out"
run wrong_ih 1 "$cc" -std=c11 -Wall -Wextra -Werror -O2 -I"$out" \
	-c "$here/wrong_ih.c" -o "$out/wrong.o"
printf 'Actual Acc Fold/IH adapter:11 expected rows; manual size0/1 clauses only, full body lowering remains open\n'
