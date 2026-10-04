#!/bin/sh
set -eu
test "$#" = 4
pointer=$1
emitter=$2
oracle=$3
out=$4
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
run admit 0 "$pointer" --legacy-intrinsic-dot --steps 5000000 \
	--imports "$repo/tests/fixtures/sorted-proof-provider.p" --save "$out/clause.a" "$here/fixture.p"
run emit 0 "$emitter" "$out/clause.a" "$out/main/clause.inc" sort_acc normal
run repeat 0 "$emitter" "$out/clause.a" "$out/repeat.inc" sort_acc normal
run deterministic 0 cmp "$out/main/clause.inc" "$out/repeat.inc"
run changed_source 0 "$emitter" "$out/clause.a" "$out/drop/clause.inc" drop_sort_acc normal
run different_code 1 cmp "$out/main/clause.inc" "$out/drop/clause.inc"
run wrong_helper 4 "$emitter" "$out/clause.a" "$out/wrong.inc" sort_acc swapped
run no_partial_helper 0 test ! -s "$out/wrong.inc"
run wrong_motive 4 "$emitter" "$out/clause.a" "$out/motive.inc" sort normal
run no_partial_motive 0 test ! -s "$out/motive.inc"
run source 0 "$pointer" --load --steps 5000000 --run reference "$out/clause.a"
run drop_source 0 "$pointer" --load --steps 5000000 --run drop_reference "$out/clause.a"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out/main" "$here/support.c" "$here/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run drop_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out/drop" "$here/support.c" "$here/client.c" -o "$out/drop/client"
run drop_client 0 "$out/drop/client"
run changed_execution 0 cmp "$out/drop_source.out" "$out/drop_client.out"
# Rename only the sealed C33 client/oracle translation units. The generated
# clause TU retains its own gs entry and never calls manual qs_apply_sort.
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_observations 0 "$oracle" "$out/clause.a" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} \
	-I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Actual clause generated from admitted terms:25 expected rows;341 Core observations/resources/change-of-source controls; manual helpers remain explicit\n'
