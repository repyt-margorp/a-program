#!/bin/sh
set -eu
test "$#" = 9
pointer=$1
emitter=$2
clause=$3
partition=$4
append=$5
accessibility=$6
sort_oracle=$7
compare_oracle=$8
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/compare.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/compare.a" "$out/main/comparison.inc" compare_fn
run repeat 0 "$emitter" "$out/compare.a" "$out/repeat.inc" compare_fn
run deterministic 0 cmp "$out/main/comparison.inc" "$out/repeat.inc"
run clause 0 "$clause" "$out/compare.a" "$out/main/clause.inc" sort_acc normal
run partition 0 "$partition" "$out/compare.a" "$out/main/partition.inc" partition_nat
run append 0 "$append" "$out/compare.a" "$out/main/append.inc" append_nat
run accessibility 0 "$accessibility" "$out/compare.a" "$out/main/accessibility.inc" nat_access
run different_source 4 "$emitter" "$out/compare.a" "$out/different.inc" always_true
run no_partial_source 0 test ! -s "$out/different.inc"
run unsupported_reference 4 "$emitter" "$out/compare.a" "$out/reference.inc" always_true same_reference
run no_partial_reference 0 test ! -s "$out/reference.inc"
run wrong_family 4 "$emitter" "$out/compare.a" "$out/wrong.inc" compare_fn wrong_family
run no_partial_family 0 test ! -s "$out/wrong.inc"
run prior_shape 4 "$emitter" "$out/compare.a" "$out/prior.inc" sort_acc prior
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/compare.a"
run comparison_source 0 "$pointer" --load --steps 5000000 --run comparison_reference "$out/compare.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run compare_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/compare_client.c" -o "$out/main/compare_client"
run compare_client 0 "$out/main/compare_client"
run comparison_match 0 cmp "$out/comparison_source.out" "$out/compare_client.out"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_sort_observations 0 "$sort_oracle" "$out/compare.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
run Core_comparison_observations 0 "$compare_oracle" "$out/compare.a" "$out/compare_oracle.c"
run compare_oracle_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" -I"$here" "$out/compare_oracle.c" -o "$out/compare_oracle"
run compare_oracle_client 0 "$out/compare_oracle"
printf 'Actual source comparator composition:33 expected rows/341 Core sorts/256 Core comparisons; deferred right-before-IH and transport boundaries explicit\n'
