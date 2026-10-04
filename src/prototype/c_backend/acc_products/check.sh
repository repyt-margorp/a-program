#!/bin/sh
set -eu
test "$#" = 6
products=$1
core_cases=$2
pointer=$3
image=$4
backend=$5
out=$6
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cc=${CC:-cc}
ar=${AR:-ar}
test ! -e "$out"
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
sha256sum "$image" >"$out/image-before.sha256"
run source_reference 0 "$pointer" --load --steps 5000000 --run reference "$image"
# Reuse exact retained current-Core cases, only adapting the private C header/name.
run adapt_Core_cases 0 sed 's/"mockup.h"/"component.h"/; s/qs_mockup_sort/gs_sort/g' "$core_cases"
cp "$out/adapt_Core_cases.out" "$out/core_cases.c"
for mode in source object archive; do
	run "${mode}_package" 0 python3 "$here/pack.py" "$products" "$out/$mode"
	run "${mode}_object" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -c "$out/$mode/component.c" -o "$out/$mode/component.o"
	if test "$mode" = source; then operand="$out/$mode/component.c"
	elif test "$mode" = object; then operand="$out/$mode/component.o"
	else
		run archive_build 0 "$ar" rcs "$out/$mode/library.a" "$out/$mode/component.o"
		operand="$out/$mode/library.a"
	fi
	run "${mode}_client_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$here/client.c" "$operand" -o "$out/$mode/client"
	run "${mode}_client" 0 "$out/$mode/client"
	run "${mode}_source_match" 0 cmp "$out/source_reference.out" "$out/${mode}_client.out"
	run "${mode}_resource_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$here/resource_client.c" "$operand" -Wl,--wrap=malloc -o "$out/$mode/resource"
	run "${mode}_resource" 0 "$out/$mode/resource"
	run "${mode}_Core_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$out/core_cases.c" "$operand" -o "$out/$mode/core_cases"
	run "${mode}_Core_cases" 0 "$out/$mode/core_cases"
done
run object_source_exact 0 cmp "$out/source/component.c" "$out/object/component.c"
run archive_source_exact 0 cmp "$out/source/component.c" "$out/archive/component.c"
run header_exact 0 cmp "$out/source/component.h" "$out/archive/component.h"
run public_symbols 0 nm -g --defined-only "$out/source/component.o"
run single_entry 0 python3 -c 'import pathlib,sys; rows=pathlib.Path(sys.argv[1]).read_text().splitlines(); assert len(rows)==1 and rows[0].split()[-2:]==["T","gs_sort"]' "$out/public_symbols.out"
run duplicate_symbol 1 "$cc" ${CLIENT_CFLAGS:--O2} -I"$out/source" "$here/client.c" "$out/source/component.o" "$out/source/component.o" -o "$out/duplicate"
run prior_product 2 python3 "$here/pack.py" "$products" "$out/source"
run prior_source_exact 0 cmp "$out/source/component.c" "$out/archive/component.c"
mkdir "$out/changed"
cp "$products"/*.inc "$out/changed/"
run change_body 0 python3 -c 'import pathlib,sys; p=pathlib.Path(sys.argv[1]); p.write_bytes(p.read_bytes()+b"\n")' "$out/changed/actions.inc"
run unqualified_body 4 python3 "$here/pack.py" "$out/changed" "$out/unqualified"
run unqualified_absent 0 test ! -e "$out/unqualified"
sed "s|artifact actual.a|artifact $image|" "$here/native.aplink" >"$out/native.aplink"
if test "$backend" = unavailable; then
	printf 'native_checked\t4\tNOT_RUN\nnative_trusted\t4\tNOT_RUN\n' >"$out/native-pending.tsv"
	printf 'Current backend build lacks removed support.h; native source probing not executed.\n' >"$out/native-pending.txt"
else
	for mode in checked trusted; do
		if test "$mode" = checked; then
			run "native_$mode" 4 "$backend" --steps 5000000 --image-limit none --link "$out/native.aplink" "$out/native-$mode"
		else
			run "native_$mode" 4 "$backend" --trust-image --steps 0 --image-limit none --link "$out/native.aplink" "$out/native-$mode"
		fi
		run "native_${mode}_absent" 0 test ! -e "$out/native-$mode"
	done
fi
sha256sum "$image" >"$out/image-after.sha256"
run image_unchanged 0 cmp "$out/image-before.sha256" "$out/image-after.sha256"
printf 'Actual Acc candidate source/object/archive clients match source/Core/resources; native probe outcome/pending state is separately reported\n'
