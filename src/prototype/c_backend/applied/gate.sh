#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
probe=${3:?inert applied selection/emission probe}
here=$(cd "$(dirname "$0")" && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
bash "$here/check.sh" "$backend" "$compiler" "$temporary/families"
bash "$here/inert_check.sh" "$probe" "$temporary/families" "$temporary/inert"
bash "$here/modules/check.sh" "$temporary/families" "$temporary/modules"
bash "$here/slice/check.sh" "$backend" "$compiler" "$temporary/slice"
"$probe" "$temporary/slice/source.aplink" "$temporary/slice.c" "$temporary/slice.h"
cmp "$temporary/slice.c" "$temporary/slice/source/component.c"
cmp "$temporary/slice.h" "$temporary/slice/source/component.h"
printf 'Applied family gate: ordinary C products, source slices, module composition, inert emission and explicit refusal controls pass\n'
