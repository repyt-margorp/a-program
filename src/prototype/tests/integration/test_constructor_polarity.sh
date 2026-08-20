#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

TYPE_DECLARATION_HEADER=src/prototype/include/a_program/kernel/type_declaration.h
TYPE_SCHEMA_SOURCE=src/prototype/src/kernel/type_schema_view.c
semantic_schema=$(sed -n \
	'/struct prototype_type_semantic_schema_db {/,/^};/p' \
	"$TYPE_DECLARATION_HEADER")
printf '%s\n' "$semantic_schema" | grep -q 'constructor_declarations'
if printf '%s\n' "$semantic_schema" | grep -q \
	'readback\|representation\|classifier_cache'; then
	echo 'semantic schema still contains readback or cache authority' >&2
	exit 1
fi
schema_query=$(sed -n \
	'/int prototype_constructor_schema_view_query(/,/^}/p' \
	"$TYPE_SCHEMA_SOURCE")
if printf '%s\n' "$schema_query" | grep -q \
	'readback\|representation_db\|constructor_classifier_cache'; then
	echo 'semantic constructor query still depends on non-semantic storage' >&2
	exit 1
fi

cat >"$tmp_dir/operation_constants.c" <<'EOF_CONSTANTS'
#include <stdio.h>

#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/core/term.h"

int main(void) {
	printf(
		"%d %d %d %d %d\n",
		PROTOTYPE_TYPED_OCCURRENCE_APP,
		PROTOTYPE_TERM_CATEGORY_VALUE,
		PROTOTYPE_TERM_CATEGORY_COMPUTATION,
		PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION,
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION
	);
	return 0;
}
EOF_CONSTANTS

cc -std=c11 -Wall -Wextra -Werror \
	-I src/prototype/include -I src/prototype \
	"$tmp_dir/operation_constants.c" -o "$tmp_dir/operation_constants"
set -- $("$tmp_dir/operation_constants")
operation_app=$1
polarity_value=$2
polarity_computation=$3
function_elimination=$4
constructor_formation=$5

cat >"$tmp_dir/constructor-category.p" <<'EOF_SOURCE'
Nat := @{ zero : *; succ : * -> *; };
Box := \A : @ => @{ box : A -> *; };

etaBox := \x : Nat => (Box Nat).box x;
higher := \f : Nat -> Box Nat => f;
wrapped := higher &etaBox;
one := (Box Nat).box Nat.zero;

Sigma := \A : @ => \B : A -> @ => @{
	mk : (a : A) -> B a -> *;
};
ConstNat := \x : Nat => Nat;
pair := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
EOF_SOURCE

./read_file.out --write-artifact "$tmp_dir/constructor-category.apo" \
	"$tmp_dir/constructor-category.p" >"$tmp_dir/constructor-category.out"

grep -q '\[constructor-spine-formation proof#' "$tmp_dir/constructor-category.out"
grep -q 'metadata label wrapped ' "$tmp_dir/constructor-category.out"
grep -q 'metadata label pair ' "$tmp_dir/constructor-category.out"

awk -v app="$operation_app" -v category="$polarity_value" \
	-v role="$constructor_formation" '
	$1 == "typed_occurrence" && $3 == app && $4 == category && $5 == role { found = 1 }
	END { exit found ? 0 : 1 }
' "$tmp_dir/constructor-category.apo"

awk -v app="$operation_app" -v category="$polarity_computation" \
	-v role="$function_elimination" '
	$1 == "typed_occurrence" && $3 == app && $4 == category && $5 == role { found = 1 }
	END { exit found ? 0 : 1 }
' "$tmp_dir/constructor-category.apo"

./read_file.out --read-graph "$tmp_dir/constructor-category.apo" \
	>"$tmp_dir/constructor-category-read.out"
grep -q '^typed_occurrences=' "$tmp_dir/constructor-category-read.out"

cat >"$tmp_dir/partial-constructor.p" <<'EOF_PARTIAL'
Nat := @{ zero : *; succ : * -> *; };
List := \A : @ => @{ nil : *; cons : A -> * -> *; };
partial := (List Nat).cons Nat.zero;
EOF_PARTIAL

if ./read_file.out "$tmp_dir/partial-constructor.p" \
	>"$tmp_dir/partial-constructor.out" 2>"$tmp_dir/partial-constructor.err"; then
	echo "escaping partial constructor unexpectedly compiled" >&2
	exit 1
fi

cat >"$tmp_dir/nested-partial-constructor.p" <<'EOF_NESTED_PARTIAL'
Nat := @{ zero : *; succ : * -> *; };
Box := \A : @ => @{ box : A -> *; };
hold := \f : Nat -> Box Nat => f;
bad := hold &(Box Nat).box;
EOF_NESTED_PARTIAL

if ./read_file.out "$tmp_dir/nested-partial-constructor.p" \
	>"$tmp_dir/nested-partial-constructor.out" \
	2>"$tmp_dir/nested-partial-constructor.err"; then
	echo "nested partial constructor unexpectedly compiled" >&2
	exit 1
fi

echo "constructor category tests passed"
