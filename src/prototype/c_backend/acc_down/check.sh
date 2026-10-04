#!/bin/sh
set -eu
test "$#" = 5
pointer=$1
image=$2
products=$3
oracle=$4
out=$5
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
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
for name in accessibility comparison partition append clause measure; do
	run "copy_$name" 0 cp "$products/$name.inc" "$out/main/$name.inc"
done
run source 0 "$pointer" --load --steps 5000000 --run reference "$image"
run main_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$here/../acc_clause/client.c" -o "$out/main/client"
run main_client 0 "$out/main/client"
run source_match 0 cmp "$out/source.out" "$out/main_client.out"
run action_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/action_client.c" -o "$out/main/action_client"
run action_client 0 "$out/main/action_client"
run resource_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -c "$manual/client.c" -o "$out/resource.o"
run resource_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/resource.o" -Wl,--wrap=malloc -o "$out/resource"
run resource 0 "$out/resource"
run Core_sort_observations 0 "$oracle" "$image" "$out/oracle.c"
run oracle_compile 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -Dqs_mockup_sort=gs_sort -I"$manual" -c "$out/oracle.c" -o "$out/oracle.o"
run oracle_link 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/main" "$here/support.c" "$out/oracle.o" -o "$out/oracle"
run oracle_client 0 "$out/oracle"
printf 'Manual scoped Acc action candidate:19 matching rows/341 Core sorts; no generated/general Identity claim\n'
