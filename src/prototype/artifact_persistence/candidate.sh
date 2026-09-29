#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: candidate.sh NEW_DIRECTORY}
bash "$here/overlay.sh" "$overlay"
for name in graph.c graph.h eval.c; do
	patch --silent --fuzz=0 --output "$overlay/src/$name.new" "$(readlink -f "$overlay/src/$name")" "$here/../readback_support/$name.patch"
	mv "$overlay/src/$name.new" "$overlay/src/$name"
done
patch --silent --fuzz=0 --output "$overlay/src/conversion.c.new" "$(readlink -f "$overlay/src/conversion.c")" "$here/../conversion_head/conversion.c.patch"
mv "$overlay/src/conversion.c.new" "$overlay/src/conversion.c"
ln -s "$here/../readback_support/support.c" "$here/../readback_support/support.h" "$overlay/src/"
sed 's/$(ROOT)graph.c/$(ROOT)graph.c $(ROOT)support.c/g' "$overlay/src/Makefile" > "$overlay/src/Makefile.new"
mv "$overlay/src/Makefile.new" "$overlay/src/Makefile"
