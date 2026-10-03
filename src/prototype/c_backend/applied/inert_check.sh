#!/usr/bin/env bash
set -euo pipefail
probe=${1:?inert selection/emission probe}
evidence=${2:?completed ordinary gate directory}
output=${3:?new inert evidence directory}
test ! -e "$output"
mkdir -p "$output"
sha256sum "$evidence/applied-list.a" > "$output/before.sha256"
for selection in numbers flags reverse choice; do
	"$probe" "$evidence/$selection-source.aplink" "$output/$selection.c" "$output/$selection.h"
	cmp "$output/$selection.c" "$evidence/$selection-source/component.c"
	cmp "$output/$selection.h" "$evidence/$selection-source/component.h"
done
sha256sum "$evidence/applied-list.a" > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Applied emission: four admitted profiles match ordinary products byte-for-byte; no source work or evidence mutation\n'
