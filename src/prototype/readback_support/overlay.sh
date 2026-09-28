#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=$(cd "$here/../.." && pwd)
overlay=${1:?usage: overlay.sh BUILD_DIRECTORY}
mkdir -p "$overlay/src"
for input in "$source"/*.[ch] "$source/Makefile"; do
	name=${input##*/}
	case $name in graph.h|graph.c|eval.c|Makefile) continue;; esac
	ln -sf "$input" "$overlay/src/$name"
done
for name in graph.h graph.c eval.c; do
	patch --silent --fuzz=0 --output "$overlay/src/$name" "$source/$name" "$here/$name.patch"
done
# Generated build input: include the support owner wherever graph.c is linked.
sed 's/$(ROOT)graph.c/$(ROOT)graph.c $(ROOT)support.c/g' "$source/Makefile" > "$overlay/src/Makefile.generated"
mv "$overlay/src/Makefile.generated" "$overlay/src/Makefile"
ln -sf "$here/support.c" "$here/support.h" "$overlay/src/"
ln -sfn "$source/../tests" "$overlay/tests"
ln -sfn "$source/../examples" "$overlay/examples"
ln -sfn "$source/../archive" "$overlay/archive"
ln -sfn "$source/../training" "$overlay/training"
ln -sf "$source/../print.p" "$overlay/print.p"
ln -sf "$source/../Makefile" "$overlay/Makefile"
