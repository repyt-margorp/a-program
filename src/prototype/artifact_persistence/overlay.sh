#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source=${ARTIFACT_SOURCE:-$(cd "$here/../.." && pwd)}
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
mkdir "$overlay"
mkdir "$overlay/src"
for input in "$source"/*.[ch] "$source/Makefile"; do
	name=${input##*/}
	if [[ -f "$here/$name.patch" ]]; then
		cp "$input" "$overlay/src/$name"
		git apply --unsafe-paths --directory="$overlay/src" "$here/$name.patch"
	else
		ln -s "$input" "$overlay/src/$name"
	fi
done
cp -a "$here/artifact" "$overlay/src/artifact"
repo=$(cd "$source/.." && pwd)
cp -a "$repo/tests" "$overlay/tests"
for patch in "$here"/test_patches/*.patch; do
	[[ -f "$patch" ]] || continue
	git apply --unsafe-paths --directory="$overlay/tests" "$patch"
done
for name in examples archive training; do ln -s "$repo/$name" "$overlay/$name"; done
ln -s "$repo/print.p" "$repo/Makefile" "$overlay/"
