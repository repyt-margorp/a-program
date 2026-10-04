#!/bin/sh
set -eu
test "$#" = 7
pointer=$1
emitter=$2
clause=$3
partition=$4
append=$5
oracle=$6
out=$7
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
manual="$here/../acc_quicksort_mockup"
cc=${CC:-cc}
test ! -e "$out"
mkdir -p "$out/main"
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/access.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/access.a" "$out/main/accessibility.inc" nat_access
run repeat 0 "$emitter" "$out/access.a" "$out/repeat.inc" nat_access
run deterministic 0 cmp "$out/main/accessibility.inc" "$out/repeat.inc"
run clause 0 "$clause" "$out/access.a" "$out/main/clause.inc" sort_acc normal
run partition 0 "$partition" "$out/access.a" "$out/main/partition.inc" partition_nat
run append 0 "$append" "$out/access.a" "$out/main/append.inc" append_nat
run changed_zero 4 "$emitter" "$out/access.a" "$out/changed.inc" changed_zero_access
run no_partial_zero 0 test ! -s "$out/changed.inc"
run unsupported_succ 4 "$emitter" "$out/access.a" "$out/succ.inc" succ_access
run no_partial_succ 0 test ! -s "$out/succ.inc"
run wrong_family 4 "$emitter" "$out/access.a" "$out/wrong.inc" nat_access wrong_family
run no_partial_family 0 test ! -s "$out/wrong.inc"
run prior_shape 4 "$emitter" "$out/access.a" "$out/prior.inc" sort_acc prior
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/access.a"
run subjects_source 0 "$pointer" --load --steps 5000000 --run subjects_reference "$out/access.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run access_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/access_client.c" -o "$out/main/access_client"
run access_client 0 "$out/main/access_client"
run subjects_match 0 cmp "$out/subjects_source.out" "$out/access_client.out"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_observations 0 "$oracle" "$out/access.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Actual Nat accessibility recurrence composition:29 expected rows/341 Core observations; manual successor/zero-down transports explicit\n'
