#!/bin/sh
set -eu
test "$#" = 6
pointer=$1
emitter=$2
clause=$3
partition=$4
oracle=$5
out=$6
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../../../.." && pwd)
manual="$here/../acc_quicksort_mockup"
cc=${CC:-cc}
test ! -e "$out"
mkdir -p "$out/main" "$out/drop"
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/append.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/append.a" "$out/main/append.inc" append_nat
run repeat 0 "$emitter" "$out/append.a" "$out/repeat.inc" append_nat
run deterministic 0 cmp "$out/main/append.inc" "$out/repeat.inc"
run clause 0 "$clause" "$out/append.a" "$out/main/clause.inc" sort_acc normal
run partition 0 "$partition" "$out/append.a" "$out/main/partition.inc" partition_nat
run changed_source 0 "$emitter" "$out/append.a" "$out/drop/append.inc" drop_append_nat
run changed_code 1 cmp "$out/main/append.inc" "$out/drop/append.inc"
run copy_clause 0 cp "$out/main/clause.inc" "$out/drop/clause.inc"
run copy_partition 0 cp "$out/main/partition.inc" "$out/drop/partition.inc"
run open_type 4 "$emitter" "$out/append.a" "$out/open.inc" append_fn
run no_partial_open 0 test ! -s "$out/open.inc"
run whole_left_capture 4 "$emitter" "$out/append.a" "$out/capture.inc" keep_left_nat
run no_partial_capture 0 test ! -s "$out/capture.inc"
run wrong_family 4 "$emitter" "$out/append.a" "$out/wrong.inc" append_nat wrong_family
run no_partial_family 0 test ! -s "$out/wrong.inc"
run prior_motive 4 "$emitter" "$out/append.a" "$out/prior.inc" sort_acc prior
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/append.a"
run append_source 0 "$pointer" --load --steps 5000000 --run append_reference "$out/append.a"
run drop_source 0 "$pointer" --load --steps 5000000 --run drop_reference "$out/append.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run append_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/append_client.c" -o "$out/main/append_client"
run append_client 0 "$out/main/append_client"
run append_match 0 cmp "$out/append_source.out" "$out/append_client.out"
run drop_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -DAPPEND_MUTATION -I"$out/drop" "$here/append_client.c" -o "$out/drop/append_client"
run drop_client 0 "$out/drop/append_client"
run drop_match 0 cmp "$out/drop_source.out" "$out/drop_client.out"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_observations 0 "$oracle" "$out/append.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Actual Acc/partition/append executable:36 expected rows,341 Core observations; captures/deferred tail/refusals/resources retained; remaining manual extent explicit\n'
