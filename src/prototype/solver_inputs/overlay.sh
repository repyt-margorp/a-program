#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
overlay=${1:?usage: overlay.sh NEW_DIRECTORY}
bash "$here/../artifact_persistence/candidate.sh" "$overlay"
for patch in "$here"/*.patch; do
	name=${patch##*/}
	name=${name%.patch}
	if [[ -L "$overlay/src/$name" ]]; then
		cp --remove-destination "$(readlink -f "$overlay/src/$name")" "$overlay/src/$name"
	fi
	git apply --unsafe-paths --directory="$overlay/src" "$patch"
done
for patch in "$here"/artifact_patches/*.patch; do
	git apply --unsafe-paths --directory="$overlay/src/artifact" "$patch"
done
for patch in "$here"/test_patches/*.patch; do
	git apply --unsafe-paths --directory="$overlay/tests" "$patch"
done
mkdir -p "$overlay/checkpoint_tests"
for file in "$here"/../artifact_persistence/*_checkpoint_test.c; do
	ln -s "$(readlink -f "$file")" "$overlay/checkpoint_tests/${file##*/}"
done
for patch in "$here"/checkpoint_test_patches/*.patch; do
	name=${patch##*/}
	name=${name%.patch}
	cp --remove-destination "$(readlink -f "$overlay/checkpoint_tests/$name")" "$overlay/checkpoint_tests/$name"
	git apply --unsafe-paths --directory="$overlay/checkpoint_tests" "$patch"
done
mkdir "$overlay/artifact_tests"
for name in semantic_test.c semantic_consumer.c metrics.c; do
	ln -s "$(readlink -f "$here/../artifact_persistence/$name")" "$overlay/artifact_tests/$name"
done
for patch in "$here"/artifact_test_patches/*.patch; do
	name=${patch##*/}
	name=${name%.patch}
	cp --remove-destination "$(readlink -f "$overlay/artifact_tests/$name")" "$overlay/artifact_tests/$name"
	git apply --unsafe-paths --directory="$overlay/artifact_tests" "$patch"
done
