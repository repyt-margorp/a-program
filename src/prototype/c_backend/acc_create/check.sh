#!/bin/sh
set -eu
test "$#" = 6
emitter=$1
image=$2
products=$3
core_cases=$4
pointer=$5
out=$6
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cc=${CC:-cc}
ar=${AR:-ar}
test ! -e "$out"
mkdir -p "$out"
run()
{
	label=$1 expected=$2
	shift 2
	printf '%s\0' "$label" "$expected" "$#" "$@" >>"$out/argv.nul"
	status=0
	"$@" >"$out/$label.out" 2>"$out/$label.err" || status=$?
	printf '%s\t%s\t%s\n' "$label" "$expected" "$status" >>"$out/status.tsv"
	test "$status" = "$expected"
}
sha256sum "$image" >"$out/image-before.sha256"
run descriptor 0 "$emitter" "$image" "$out/transport.inc" succ_access
run descriptor_alias 0 "$emitter" "$image" "$out/alias.inc" exact_successor
run descriptor_alias_exact 0 cmp "$out/transport.inc" "$out/alias.inc"
run descriptor_changed 4 "$emitter" "$image" "$out/changed.inc" changed_successor
run descriptor_prior 4 "$emitter" "$image" "$out/prior.inc" changed_successor prior
run descriptor_wrong_family 4 "$emitter" "$image" "$out/wrong.inc" succ_access wrong_family
run descriptor_wrong_acc 4 "$emitter" "$image" "$out/wrong-acc.inc" succ_access wrong_acc
run descriptor_wrong_lt 4 "$emitter" "$image" "$out/wrong-lt.inc" succ_access wrong_lt
run descriptor_wrong_domain 4 "$emitter" "$image" "$out/wrong-domain.inc" succ_access wrong_domain
run descriptor_wrong_relation 4 "$emitter" "$image" "$out/wrong-relation.inc" succ_access wrong_relation
run descriptor_missing_parameters 4 "$emitter" "$image" "$out/missing-parameters.inc" succ_access missing_parameters
for name in two_successors_lt extra_field_lt wrong_prior_lt; do
	run "${name}_refusal" 4 "$emitter" "$image" "$out/$name.inc" "$name" lt_only
	run "${name}_prior" 4 "$emitter" "$image" "$out/$name-prior.inc" "$name" lt_prior
done
run source_reference 0 "$pointer" --load --steps 5000000 --run reference "$image"
run adapt_Core_cases 0 sed 's/"mockup.h"/"component.h"/; s/qs_mockup_sort/gs_sort/g' "$core_cases"
cp "$out/adapt_Core_cases.out" "$out/core_cases.c"
for mode in source object archive; do
	run "${mode}_package" 0 python3 "$here/pack.py" "$products" "$out/transport.inc" "$out/$mode"
	run "${mode}_object" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -c "$out/$mode/component.c" -o "$out/$mode/component.o"
	if test "$mode" = source; then operand="$out/$mode/component.c"
	elif test "$mode" = object; then operand="$out/$mode/component.o"
	else
		run archive_build 0 "$ar" rcs "$out/$mode/library.a" "$out/$mode/component.o"
		operand="$out/$mode/library.a"
	fi
	run "${mode}_client_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$here/../acc_products/client.c" "$operand" -o "$out/$mode/client"
	run "${mode}_client" 0 "$out/$mode/client"
	run "${mode}_source_match" 0 cmp "$out/source_reference.out" "$out/${mode}_client.out"
	run "${mode}_resource_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$here/../acc_products/resource_client.c" "$operand" -Wl,--wrap=malloc -o "$out/$mode/resource"
	run "${mode}_resource" 0 "$out/$mode/resource"
	run "${mode}_Core_build" 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/$mode" "$out/core_cases.c" "$operand" -o "$out/$mode/core_cases"
	run "${mode}_Core_cases" 0 "$out/$mode/core_cases"
done
run map_client_build 0 "$cc" -std=c11 -Wall -Wextra -Werror ${CLIENT_CFLAGS:--O2} -I"$out/source" "$here/map_client.c" -o "$out/map_client"
run map_client 0 "$out/map_client"
run object_source_exact 0 cmp "$out/source/component.c" "$out/object/component.c"
run archive_source_exact 0 cmp "$out/source/component.c" "$out/archive/component.c"
run header_exact 0 cmp "$out/source/component.h" "$out/archive/component.h"
run public_symbols 0 nm -g --defined-only "$out/source/component.o"
run single_entry 0 python3 -c 'import pathlib,sys; rows=pathlib.Path(sys.argv[1]).read_text().splitlines(); assert len(rows)==1 and rows[0].split()[-2:]==["T","gs_sort"]' "$out/public_symbols.out"
run duplicate_symbol 1 "$cc" ${CLIENT_CFLAGS:--O2} -I"$out/source" "$here/../acc_products/client.c" "$out/source/component.o" "$out/source/component.o" -o "$out/duplicate"
run prior_product 2 python3 "$here/pack.py" "$products" "$out/transport.inc" "$out/source"
run prior_source_exact 0 cmp "$out/source/component.c" "$out/archive/component.c"
cp "$out/transport.inc" "$out/changed-table.inc"
run change_table 0 python3 -c 'import pathlib,sys; p=pathlib.Path(sys.argv[1]); p.write_bytes(p.read_bytes()+b"\n")' "$out/changed-table.inc"
run unqualified_table 4 python3 "$here/pack.py" "$products" "$out/changed-table.inc" "$out/unqualified"
run unqualified_absent 0 test ! -e "$out/unqualified"
sha256sum "$image" >"$out/image-after.sha256"
run image_unchanged 0 cmp "$out/image-before.sha256" "$out/image-after.sha256"
printf 'Actual Acc LT creation uses admitted prior/result recipes; products/source/Core/resources and inherited/new constructor refusals pass, manual storage/full Scope remain explicit\n'
