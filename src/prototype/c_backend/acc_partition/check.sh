#!/bin/sh
set -eu
test "$#" = 5
pointer=$1
emitter=$2
clause=$3
oracle=$4
out=$5
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
manual="$here/../acc_quicksort_mockup"
cc=${CC:-cc}
test ! -e "$out"
mkdir -p "$out/main" "$out/reverse"
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/partition.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/partition.a" "$out/main/partition.inc" partition_nat
run repeat 0 "$emitter" "$out/partition.a" "$out/repeat.inc" partition_nat
run deterministic 0 cmp "$out/main/partition.inc" "$out/repeat.inc"
run clause 0 "$clause" "$out/partition.a" "$out/main/clause.inc" sort_acc normal
run changed_source 0 "$emitter" "$out/partition.a" "$out/reverse/partition.inc" reverse_partition_nat
run changed_code 1 cmp "$out/main/partition.inc" "$out/reverse/partition.inc"
run copy_clause 0 cp "$out/main/clause.inc" "$out/reverse/clause.inc"
run open_type 4 "$emitter" "$out/partition.a" "$out/open.inc" partition_fn
run no_partial_open 0 test ! -s "$out/open.inc"
run wrong_family 4 "$emitter" "$out/partition.a" "$out/wrong.inc" partition_nat wrong_family
run no_partial_family 0 test ! -s "$out/wrong.inc"
run prior_motive 4 "$emitter" "$out/partition.a" "$out/prior.inc" sort_acc prior
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/partition.a"
run part_source 0 "$pointer" --load --steps 5000000 --run partition_reference "$out/partition.a"
run reverse_source 0 "$pointer" --load --steps 5000000 --run reverse_reference "$out/partition.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run part_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/partition_client.c" -o "$out/main/partition_client"
run part_client 0 "$out/main/partition_client"
run part_match 0 cmp "$out/part_source.out" "$out/part_client.out"
run reverse_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -DPARTITION_MUTATION -I"$out/reverse" "$here/partition_client.c" -o "$out/reverse/partition_client"
run reverse_client 0 "$out/reverse/partition_client"
run reverse_match 0 cmp "$out/reverse_source.out" "$out/reverse_client.out"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_observations 0 "$oracle" "$out/partition.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Actual source partition/helpers and Acc clause executable;32 expected rows,341 Core observations; remaining manual extent explicit\n'
