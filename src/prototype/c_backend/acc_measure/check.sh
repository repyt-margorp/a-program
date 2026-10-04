#!/bin/sh
set -eu
test "$#" = 9
pointer=$1
emitter=$2
clause=$3
partition=$4
append=$5
accessibility=$6
comparison=$7
oracle=$8
out=$9
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/measure.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/measure.a" "$out/main/measure.inc" measure_fn outer_fn
run repeat 0 "$emitter" "$out/measure.a" "$out/repeat.inc" measure_fn outer_fn
run deterministic 0 cmp "$out/main/measure.inc" "$out/repeat.inc"
run clause 0 "$clause" "$out/measure.a" "$out/main/clause.inc" sort_acc normal
run partition 0 "$partition" "$out/measure.a" "$out/main/partition.inc" partition_nat
run append 0 "$append" "$out/measure.a" "$out/main/append.inc" append_nat
run accessibility 0 "$accessibility" "$out/measure.a" "$out/main/accessibility.inc" nat_access
run comparison 0 "$comparison" "$out/measure.a" "$out/main/comparison.inc" compare_fn
run different_measure 4 "$emitter" "$out/measure.a" "$out/different.inc" wrong_measure outer_fn
run no_partial_measure 0 test ! -s "$out/different.inc"
run same_measure 4 "$emitter" "$out/measure.a" "$out/same_measure.inc" wrong_measure outer_fn same_reference
run no_partial_same_measure 0 test ! -s "$out/same_measure.inc"
run different_outer 4 "$emitter" "$out/measure.a" "$out/outer.inc" measure_fn wrong_outer
run no_partial_outer 0 test ! -s "$out/outer.inc"
run same_outer 4 "$emitter" "$out/measure.a" "$out/same_outer.inc" measure_fn wrong_outer same_reference
run no_partial_same_outer 0 test ! -s "$out/same_outer.inc"
run wrong_family 4 "$emitter" "$out/measure.a" "$out/family.inc" measure_fn outer_fn wrong_family
run no_partial_family 0 test ! -s "$out/family.inc"
run prior_shape 4 "$emitter" "$out/measure.a" "$out/prior.inc" wrong_measure wrong_outer prior
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/measure.a"
run measure_source 0 "$pointer" --load --steps 5000000 --run measure_reference "$out/measure.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run measure_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/measure_client.c" -o "$out/main/measure_client"
run measure_client 0 "$out/main/measure_client"
run measure_match 0 cmp "$out/measure_source.out" "$out/measure_client.out"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_sort_observations 0 "$oracle" "$out/measure.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Actual source measure/outer composition:35 expected rows/341 Core sorts; manual checked down transports explicit\n'
