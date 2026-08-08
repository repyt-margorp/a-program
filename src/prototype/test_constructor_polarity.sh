#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/operation_constants.c" <<'EOF_CONSTANTS'
#include <stdio.h>

#include "ast.h"
#include "term.h"

int main(void) {
	printf(
		"%d %d %d %d %d\n",
		PROTOTYPE_OPERATION_APP,
		PROTOTYPE_OPERATION_POLARITY_VALUE,
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION,
		PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION,
		PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION
	);
	return 0;
}
EOF_CONSTANTS

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	"$tmp_dir/operation_constants.c" -o "$tmp_dir/operation_constants"
set -- $("$tmp_dir/operation_constants")
operation_app=$1
polarity_value=$2
polarity_computation=$3
function_elimination=$4
constructor_formation=$5

cat >"$tmp_dir/constructor-polarity.p" <<'EOF_SOURCE'
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

./read_file.out --write-artifact "$tmp_dir/constructor-polarity.apo" \
	"$tmp_dir/constructor-polarity.p" >"$tmp_dir/constructor-polarity.out"

grep -q '\[constructor-spine-formation proof#' "$tmp_dir/constructor-polarity.out"
grep -q 'metadata label wrapped ' "$tmp_dir/constructor-polarity.out"
grep -q 'metadata label pair ' "$tmp_dir/constructor-polarity.out"

awk -v app="$operation_app" -v polarity="$polarity_value" \
	-v role="$constructor_formation" '
	$1 == "operation" && $3 == app && $4 == polarity && $6 == role { found = 1 }
	END { exit found ? 0 : 1 }
' "$tmp_dir/constructor-polarity.apo"

awk -v app="$operation_app" -v polarity="$polarity_computation" \
	-v role="$function_elimination" '
	$1 == "operation" && $3 == app && $4 == polarity && $6 == role { found = 1 }
	END { exit found ? 0 : 1 }
' "$tmp_dir/constructor-polarity.apo"

./read_file.out --read-graph "$tmp_dir/constructor-polarity.apo" \
	>"$tmp_dir/constructor-polarity-read.out"
grep -q '^operation_occurrences=' "$tmp_dir/constructor-polarity-read.out"

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

echo "constructor polarity tests passed"
