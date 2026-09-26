#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"

HEADER=src/prototype/include/a_program/kernel/type_declaration.h
SOURCE=src/prototype/src/kernel/type_declaration.c

semantic_type=$(sed -n \
	'/struct prototype_type_declaration {/,/^};/p' "$HEADER")
if printf '%s\n' "$semantic_type" | grep -q 'first_parameter'; then
	echo 'semantic TypeDeclaration still stores a readback offset' >&2
	exit 1
fi

readback_type=$(sed -n \
	'/struct prototype_type_readback_entry {/,/^};/p' "$HEADER")
printf '%s\n' "$readback_type" | grep -q 'first_parameter;'

semantic_validator=$(sed -n \
	'/int prototype_constructor_telescopes_validate(/,/^}/p' "$SOURCE")
if printf '%s\n' "$semantic_validator" | grep -q 'readback'; then
	echo 'semantic telescope validation reads presentation metadata' >&2
	exit 1
fi

grep -q 'prototype_type_constructor_readback_get(' "$HEADER"
grep -q 'const struct prototype_type_readback_db\* readback' "$HEADER"
grep -q 'const struct prototype_type_semantic_schema_db\* semantic_schema' \
	"$HEADER"

if grep -R -q 'curried_classifier_cache' \
	src/prototype/include src/prototype/src src/prototype/spec; then
	echo 'persistent constructor classifier still has a runtime-cache name' >&2
	exit 1
fi
grep -q 'uint32_t constructor_classifier;' \
	src/prototype/include/a_program/artifact/interface.h

echo 'type capability authority checks passed'
