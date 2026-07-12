#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-artifact-flow.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

make reader >/dev/null
make >/dev/null

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/repl.c \
	src/prototype/ast.c \
	src/prototype/ast_inspect.c \
	src/prototype/reader.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/universe.c \
	src/prototype/symbol.c \
	-o "$TMP_DIR/prototype-repl"

cc -std=c99 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/whnf_profile_cache_check.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/symbol.c \
	-o "$TMP_DIR/whnf_profile_cache_check"
"$TMP_DIR/whnf_profile_cache_check"

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/universe_defeq_check.c \
	src/prototype/term.c \
	src/prototype/type_declaration.c \
	src/prototype/typing.c \
	src/prototype/symbol.c \
	-o "$TMP_DIR/universe_defeq_check"
"$TMP_DIR/universe_defeq_check"

c_enum_value_in() {
	awk -v enum_name="$2" -v target_name="$3" '
		$0 ~ "enum " enum_name " " {
			in_enum = 1;
			next_value = 0;
			next;
		}
		in_enum && $0 ~ /^};/ {
			exit;
		}
		in_enum {
			line = $0;
			sub(/\/\/.*/, "", line);
			gsub(/,/, "", line);
			gsub(/^[ \t]+|[ \t]+$/, "", line);
			if (line == "") {
				next;
			}
			split(line, parts, "=");
			name = parts[1];
			gsub(/[ \t]+/, "", name);
			if (parts[2] != "") {
				value = parts[2] + 0;
			} else {
				value = next_value;
			}
			if (name == target_name) {
				print value;
				found = 1;
				exit;
			}
			next_value = value + 1;
		}
		END {
			if (!found) {
				exit 1;
			}
		}
	' "$1"
}

c_enum_value() {
	c_enum_value_in src/prototype/judgement.h "$1" "$2"
}

JUDGEMENT_KIND_HAS_TYPE=$(c_enum_value prototype_judgement_kind PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE)
JUDGEMENT_KIND_IS_TYPE=$(c_enum_value prototype_judgement_kind PROTOTYPE_JUDGEMENT_KIND_IS_TYPE)
PROOF_KIND_TYPE_FORMATION_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO)
PROOF_KIND_CONSTRUCTOR_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO)
PROOF_KIND_BINDER_ASSUMPTION=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION)
PROOF_KIND_MATCH_PATTERN_ASSUMPTION=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION)
PROOF_KIND_LAMBDA_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO)
PROOF_KIND_APP_ELIM=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM)
PROOF_KIND_MATCH_TYPE_FORMATION_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO)
PROOF_KIND_MATCH_ELIM=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM)
PROOF_KIND_INDUCTION_HYPOTHESIS_ELIM=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM)
PROOF_KIND_TEXT_LITERAL_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO)
PROOF_KIND_INTRINSIC_TYPE_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_INTRINSIC_TYPE_INTRO)
PROOF_KIND_INT_LITERAL_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO)
PROOF_KIND_CONVERSION=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_CONVERSION)
PROOF_KIND_HOST_TYPE_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO)
PROOF_KIND_IS_TYPE_FROM_HAS_TYPE=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE)
PROOF_KIND_DECLARATION=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_DECLARATION)
PROOF_KIND_UNIVERSE_CUMULATIVITY=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY)
PROOF_KIND_PI_FORMATION_INTRO=$(c_enum_value prototype_judgement_proof_kind PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO)
TERM_TAG_CONSTRUCTOR=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_CONSTRUCTOR)
TERM_TAG_PI=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_PI)
TERM_TAG_TEXT_LITERAL=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_TEXT_LITERAL)
TERM_TAG_EXTERNAL_REF=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_EXTERNAL_REF)
TERM_TAG_EFFECT_LABEL=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_EFFECT_LABEL)
TERM_TAG_EFFECT_TYPE=$(c_enum_value_in src/prototype/term.h prototype_term_tag PROTOTYPE_TERM_EFFECT_TYPE)

if grep -q 'prototype_type_declaration_find_by_code_shape_key' src/prototype/typing.c; then
	echo "typing must not resolve imported type expressions by TypeCodeShapeKey" >&2
	exit 1
fi
if rg -q 'prototype_type_declaration_find_by_code_shape_key' src/prototype --glob '*.[ch]'; then
	echo "import resolution must not expose a TypeCodeShapeKey lookup path" >&2
	exit 1
fi

cat >"$TMP_DIR/identity.p" <<'EOF_IDENTITY'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

identityBool := \x : Bool => x;
identityNat := \y : Nat => y;
identityBool :: Bool -> Bool;
identityNat :: Nat -> Nat;
boolExpected := Bool.true;
boolMain := identityBool Bool.true;
natExpected := Nat.succ Nat.zero;
natMain := identityNat (Nat.succ Nat.zero);
EOF_IDENTITY

./read_file.out --check-source-exports-normalization-equal boolMain boolExpected \
	--reduction-mode default "$TMP_DIR/identity.p" >"$TMP_DIR/identity-source-bool.out"
grep -q '^source-exports-normalization-equal boolMain boolExpected mode=default yes$' \
	"$TMP_DIR/identity-source-bool.out"
./read_file.out --check-source-exports-normalization-equal natMain natExpected \
	--reduction-mode default "$TMP_DIR/identity.p" >"$TMP_DIR/identity-source-nat.out"
grep -q '^source-exports-normalization-equal natMain natExpected mode=default yes$' \
	"$TMP_DIR/identity-source-nat.out"
./read_file.out --write-artifact "$TMP_DIR/identity.apo" "$TMP_DIR/identity.p" >"$TMP_DIR/identity.out"
grep -q '^A_PROGRAM_ARTIFACT 28$' "$TMP_DIR/identity.apo"
sed '1s/28$/27/' "$TMP_DIR/identity.apo" >"$TMP_DIR/identity-v27.apo"
if ./read_file.out --read-graph "$TMP_DIR/identity-v27.apo" >"$TMP_DIR/identity-v27.out" 2>"$TMP_DIR/identity-v27.err"; then
	echo "v27 artifact unexpectedly passed after v28 format bump" >&2
	exit 1
fi
grep -q '^term identityBool .* namespace identity$' "$TMP_DIR/identity.apo"
grep -q '^type Bool .* namespace identity$' "$TMP_DIR/identity.apo"
grep -q 'metadata label identityBool -> operation#[0-9][0-9]* -> term#' "$TMP_DIR/identity.out"
grep -q 'metadata label identityNat -> operation#[0-9][0-9]* -> term#' "$TMP_DIR/identity.out"
identity_bool_operation=$(awk '/metadata label identityBool -> operation#[0-9]+ -> term#/ { sub("operation#", "", $5); print $5 }' "$TMP_DIR/identity.out")
identity_nat_operation=$(awk '/metadata label identityNat -> operation#[0-9]+ -> term#/ { sub("operation#", "", $5); print $5 }' "$TMP_DIR/identity.out")
test -n "$identity_bool_operation"
test -n "$identity_nat_operation"
test "$identity_bool_operation" != "$identity_nat_operation"
identity_bool_term=$(awk '$1 == "term" && $2 == "identityBool" { print $3 }' "$TMP_DIR/identity.apo")
identity_nat_term=$(awk '$1 == "term" && $2 == "identityNat" { print $3 }' "$TMP_DIR/identity.apo")
identity_bool_classifier=$(awk '$1 == "term" && $2 == "identityBool" { print $4 }' "$TMP_DIR/identity.apo")
identity_nat_classifier=$(awk '$1 == "term" && $2 == "identityNat" { print $4 }' "$TMP_DIR/identity.apo")
identity_bool_fields=$(awk '$1 == "term" && $2 == "identityBool" { print NF }' "$TMP_DIR/identity.apo")
identity_bool_key=$(awk '$1 == "term" && $2 == "identityBool" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/identity.apo")
identity_nat_key=$(awk '$1 == "term" && $2 == "identityNat" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/identity.apo")
identity_bool_classifier_key=$(awk '$1 == "term" && $2 == "identityBool" { print $14 ":" $15 ":" $16 ":" $17 ":" $18 ":" $19 ":" $20 ":" $21 }' "$TMP_DIR/identity.apo")
identity_nat_classifier_key=$(awk '$1 == "term" && $2 == "identityNat" { print $14 ":" $15 ":" $16 ":" $17 ":" $18 ":" $19 ":" $20 ":" $21 }' "$TMP_DIR/identity.apo")
test "$identity_bool_term" = "$identity_nat_term"
test "$identity_bool_classifier" != "$identity_nat_classifier"
test "$identity_bool_fields" -eq 23
test "$identity_bool_key" = "$identity_nat_key"
test "$identity_bool_classifier_key" != "$identity_nat_classifier_key"
grep -q "term_name identityBool $identity_bool_term " "$TMP_DIR/identity.apo"
grep -q 'type_name Bool ' "$TMP_DIR/identity.apo"
grep -q 'constructor_name 0 true 0 ' "$TMP_DIR/identity.apo"
./read_file.out --read-graph "$TMP_DIR/identity.apo" >"$TMP_DIR/identity-read.out"
grep -q 'debug_term_names=4 debug_type_names=2 debug_constructor_names=4' "$TMP_DIR/identity-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/identity-read.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/identity.apo" \
	boolMain boolExpected --reduction-mode default >"$TMP_DIR/identity-read-bool.out"
grep -q '^exports-normalization-equal boolMain boolExpected mode=default yes$' \
	"$TMP_DIR/identity-read-bool.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/identity.apo" \
	natMain natExpected --reduction-mode default >"$TMP_DIR/identity-read-nat.out"
grep -q '^exports-normalization-equal natMain natExpected mode=default yes$' \
	"$TMP_DIR/identity-read-nat.out"

cat >"$TMP_DIR/list-nat-match.p" <<'EOF_LIST_NAT_MATCH'
Nat := @{ zero : *; succ : * -> *; };
List := \A : @ => @{ nil : *; cons : A -> * -> *; };

headOrZero := \xs : List Nat =>
	xs @nil => Nat.zero
	   @cons x rest => x;
input := (List Nat).cons (Nat.succ Nat.zero) (List Nat).nil;
expected := Nat.succ Nat.zero;
main := headOrZero input;
EOF_LIST_NAT_MATCH

./read_file.out --check-source-exports-normalization-equal main expected \
	--reduction-mode default "$TMP_DIR/list-nat-match.p" >"$TMP_DIR/list-nat-match-source.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$TMP_DIR/list-nat-match-source.out"
./read_file.out --write-artifact "$TMP_DIR/ListNatMatch.apo" \
	"$TMP_DIR/list-nat-match.p" >"$TMP_DIR/list-nat-match.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/ListNatMatch.apo" \
	main expected --reduction-mode default >"$TMP_DIR/list-nat-match-read.out"
grep -q '^exports-normalization-equal main expected mode=default yes$' \
	"$TMP_DIR/list-nat-match-read.out"

cat >"$TMP_DIR/recursive-dependent-motive.p" <<'EOF_RECURSIVE_DEPENDENT_MOTIVE'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
BadNat := @{ zero0 : *; zero1 : *; succ : * -> *; };

fold := \n : BadNat =>
	n @zero0 => Bool.true
	  @zero1 => Nat.zero
	  @succ k => *k;
EOF_RECURSIVE_DEPENDENT_MOTIVE

./read_file.out --write-artifact "$TMP_DIR/recursive-dependent-motive.apo" \
	"$TMP_DIR/recursive-dependent-motive.p" >"$TMP_DIR/recursive-dependent-motive.out"
grep -q 'CASE(zero0 -> TYPE_VIEW(Bool' "$TMP_DIR/recursive-dependent-motive.out"
grep -q 'CASE(zero1 -> TYPE_VIEW(Nat' "$TMP_DIR/recursive-dependent-motive.out"
grep -q 'CASE(succ .* -> INDUCTION_HYPOTHESIS' "$TMP_DIR/recursive-dependent-motive.out"
./read_file.out --read-graph "$TMP_DIR/recursive-dependent-motive.apo" \
	>"$TMP_DIR/recursive-dependent-motive-read.out"
grep -q 'term_exports=4 type_exports=3 constructor_exports=7 dependencies=0' \
	"$TMP_DIR/recursive-dependent-motive-read.out"

cat >"$TMP_DIR/repl-normal-form.p" <<'EOF_REPL_NORMAL_FORM'
Nat := @{ zero : *; succ : * -> *; };

add := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => Nat.succ (*k m));
one := Nat.succ Nat.zero;
main := add one one;
EOF_REPL_NORMAL_FORM

printf ':whnf main\n:nf main\n:quit\n' |
	"$TMP_DIR/prototype-repl" "$TMP_DIR/repl-normal-form.p" >"$TMP_DIR/repl-normal-form.out"
grep -q '^prototype> whnf main := .*INDUCTION_HYPOTHESIS' "$TMP_DIR/repl-normal-form.out"
grep -q '^prototype> nf main := APP(CONSTRUCTOR(rep#0.ordinal#1), APP(CONSTRUCTOR(rep#0.ordinal#1), CONSTRUCTOR(rep#0.ordinal#0)))$' \
	"$TMP_DIR/repl-normal-form.out"

cat >"$TMP_DIR/operation-layer.p" <<'EOF_OPERATION_LAYER'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };

and := \a : Bool =>
	a @true => (\b : Bool => b)
	  @false => (\b : Bool => Bool.false);

addNat := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => Nat.succ (*k m));
EOF_OPERATION_LAYER

./read_file.out "$TMP_DIR/operation-layer.p" >"$TMP_DIR/operation-layer.out"
and_true_operation=$(awk '$1 ~ /^operation-case#/ && $NF == "label=true" { sub("body-operation#", "", $2); print $2; exit }' "$TMP_DIR/operation-layer.out")
add_nat_zero_operation=$(awk '$1 ~ /^operation-case#/ && $NF == "label=zero" { sub("body-operation#", "", $2); print $2; exit }' "$TMP_DIR/operation-layer.out")
test -n "$and_true_operation"
test -n "$add_nat_zero_operation"
test "$and_true_operation" != "$add_nat_zero_operation"
and_true_core=$(awk -v operation="$and_true_operation" '$1 == "operation#" operation { sub("core#", "", $3); print $3; exit }' "$TMP_DIR/operation-layer.out")
add_nat_zero_core=$(awk -v operation="$add_nat_zero_operation" '$1 == "operation#" operation { sub("core#", "", $3); print $3; exit }' "$TMP_DIR/operation-layer.out")
test -n "$and_true_core"
test "$and_true_core" = "$add_nat_zero_core"

cat >"$TMP_DIR/BoolProvider.p" <<'EOF_BOOL_PROVIDER'
Bool := @{ true : *; false : *; };
EOF_BOOL_PROVIDER
cat >"$TMP_DIR/TwoProvider.p" <<'EOF_TWO_PROVIDER'
Two := @{ one : *; zero : *; };
EOF_TWO_PROVIDER
cat >"$TMP_DIR/BoolUser.p" <<'EOF_BOOL_USER'
identity := \x : Bool => x;
EOF_BOOL_USER
./read_file.out --write-artifact "$TMP_DIR/Bool.apo" "$TMP_DIR/BoolProvider.p" >"$TMP_DIR/bool-provider.out"
./read_file.out --write-artifact "$TMP_DIR/Two.apo" "$TMP_DIR/TwoProvider.p" >"$TMP_DIR/two-provider.out"
./read_file.out --write-artifact "$TMP_DIR/BoolUser.apo" \
	--import-interface "$TMP_DIR/Bool.apo" "$TMP_DIR/BoolUser.p" >"$TMP_DIR/bool-user.out"
./read_file.out --link-artifacts "$TMP_DIR/BoolUser.apo" "$TMP_DIR/Two.apo" \
	--link-output "$TMP_DIR/BoolUser.with-two.apo" >"$TMP_DIR/bool-user-with-two-link.out"
./read_file.out --read-graph "$TMP_DIR/BoolUser.with-two.apo" >"$TMP_DIR/bool-user-with-two-read.out"
grep -q 'dependencies=1' "$TMP_DIR/bool-user-with-two-read.out"
grep -q 'relocation_external_terms=1 .*relocation_resolved_external_terms=0' "$TMP_DIR/bool-user-with-two-read.out"
./read_file.out --link-artifacts "$TMP_DIR/BoolUser.apo" "$TMP_DIR/Bool.apo" \
	--link-output "$TMP_DIR/BoolUser.with-bool.apo" >"$TMP_DIR/bool-user-with-bool-link.out"
./read_file.out --read-graph "$TMP_DIR/BoolUser.with-bool.apo" >"$TMP_DIR/bool-user-with-bool-read.out"
grep -q 'dependencies=0' "$TMP_DIR/bool-user-with-bool-read.out"
grep -q 'relocation_external_terms=0 .*relocation_resolved_external_terms=0' "$TMP_DIR/bool-user-with-bool-read.out"

cat >"$TMP_DIR/type-slice.p" <<'EOF_TYPE_SLICE'
Used := @{ a : *; };
Unused := @{ b : *; };
main := Used.a;
EOF_TYPE_SLICE

./read_file.out --write-artifact "$TMP_DIR/TypeSlice.apo" "$TMP_DIR/type-slice.p" >"$TMP_DIR/type-slice.out"
awk '
	$1 == "counts" && $2 == "term_slots" {
		for (i = 1; i <= NF; ++i) {
			if ($i == "type_slots") {
				type_slots = $(i + 1);
			}
			if ($i == "types") {
				types = $(i + 1);
			}
		}
		if (!(type_slots > types)) {
			exit 1;
		}
		found = 1;
	}
	END {
		if (!found) {
			exit 1;
		}
	}
' "$TMP_DIR/TypeSlice.apo"
if grep -q '^type_decl 0 #.Nat ' "$TMP_DIR/TypeSlice.apo"; then
	echo "unreached builtin type leaked into sparse graph" >&2
	exit 1
fi
if grep -q '^universe_node 0 ' "$TMP_DIR/TypeSlice.apo"; then
	echo "unreached builtin universe node leaked into sparse graph" >&2
	exit 1
fi

./read_file.out --write-artifact "$TMP_DIR/SparseList.apo" examples/standard/List.p >"$TMP_DIR/sparse-list.out"
graph_type_expr_hole=$(
	awk '
		$1 == "SECTION" && $2 == "graph" {
			in_graph = 1
			next
		}
		$1 == "SECTION" && $2 != "graph" {
			in_graph = 0
		}
		in_graph && $1 == "counts" {
			for (i = 1; i <= NF; ++i) {
				if ($i == "type_expr_slots") {
					slots = $(i + 1)
				}
			}
		}
		in_graph && $1 == "type_expr" {
			present[$2] = 1
		}
		END {
			for (i = 0; i < slots; ++i) {
				if (!(i in present)) {
					print i
					exit
				}
			}
			exit 1
		}
	' "$TMP_DIR/SparseList.apo"
)
awk -v hole="$graph_type_expr_hole" '
	$1 == "SECTION" && $2 == "graph" {
		in_graph = 1
	}
	in_graph && $1 == "type_param" && !replaced {
		$5 = hole
		replaced = 1
	}
	{ print }
' "$TMP_DIR/SparseList.apo" >"$TMP_DIR/BadSparseTypeExprRef.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadSparseTypeExprRef.apo" >"$TMP_DIR/bad-sparse-type-expr-ref.out" 2>"$TMP_DIR/bad-sparse-type-expr-ref.err"; then
	echo "sparse type expr hole reference artifact unexpectedly passed" >&2
	exit 1
fi

universe_node_hole=$(
	awk '
		$1 == "SECTION" && $2 == "universe" {
			in_universe = 1
			next
		}
		$1 == "END" && $2 == "universe" {
			in_universe = 0
		}
		in_universe && $1 == "counts" {
			for (i = 1; i <= NF; ++i) {
				if ($i == "node_slots") {
					slots = $(i + 1)
				}
			}
		}
		in_universe && $1 == "universe_node" {
			present[$2] = 1
		}
		END {
			for (i = 0; i < slots; ++i) {
				if (!(i in present)) {
					print i
					exit
				}
			}
			exit 1
		}
	' "$TMP_DIR/SparseList.apo"
)
awk -v hole="$universe_node_hole" '
	$1 == "SECTION" && $2 == "universe" {
		in_universe = 1
	}
	in_universe && $1 == "universe_edge" && !replaced {
		$4 = hole
		replaced = 1
	}
	{ print }
' "$TMP_DIR/SparseList.apo" >"$TMP_DIR/BadSparseUniverseEdgeRef.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadSparseUniverseEdgeRef.apo" >"$TMP_DIR/bad-sparse-universe-edge-ref.out" 2>"$TMP_DIR/bad-sparse-universe-edge-ref.err"; then
	echo "sparse universe node hole reference artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/multi-app.p" <<'EOF_MULTI_APP'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

identityBool :: Bool -> Bool;
identityBool := \x : Bool => x;

identityNat :: Nat -> Nat;
identityNat := \x : Nat => x;

higherBool :: (Bool -> Bool) -> (Bool -> Bool);
higherBool := \f : Bool -> Bool => f;

higherNat :: (Nat -> Nat) -> (Nat -> Nat);
higherNat := \f : Nat -> Nat => f;

useHigherBool := higherBool identityBool;
useHigherNat := higherNat identityNat;

useAscribedBool := (identityBool :: Bool -> Bool) Bool.true;
useAscribedNat := (identityNat :: Nat -> Nat) Nat.zero;
matchAscribed := ((identityBool :: Bool -> Bool) Bool.true)
        @true => (identityNat :: Nat -> Nat) Nat.zero
        @false => (identityNat :: Nat -> Nat) (Nat.succ Nat.zero);
EOF_MULTI_APP

./read_file.out "$TMP_DIR/multi-app.p" >"$TMP_DIR/multi-app.out"
multi_identity_bool_term=$(awk '/metadata label identityBool -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_identity_nat_term=$(awk '/metadata label identityNat -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_higher_bool_term=$(awk '/metadata label higherBool -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_higher_nat_term=$(awk '/metadata label higherNat -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_use_higher_bool_term=$(awk '/metadata label useHigherBool -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_use_higher_nat_term=$(awk '/metadata label useHigherNat -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_use_ascribed_bool_term=$(awk '/metadata label useAscribedBool -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_use_ascribed_nat_term=$(awk '/metadata label useAscribedNat -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
multi_match_ascribed_term=$(awk '/metadata label matchAscribed -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/multi-app.out")
test "$multi_identity_bool_term" = "$multi_identity_nat_term"
test "$multi_higher_bool_term" = "$multi_higher_nat_term"
test "$multi_use_higher_bool_term" = "$multi_use_higher_nat_term"
test -n "$multi_use_ascribed_bool_term"
test -n "$multi_use_ascribed_nat_term"
test -n "$multi_match_ascribed_term"
multi_app_elim_count=$(
	grep -F 'has-type APP(LAMBDA(_#0, VAR(_#0)), LAMBDA(_#0, VAR(_#0)))' "$TMP_DIR/multi-app.out" |
	grep -F -c '[app-elim]'
)
test "$multi_app_elim_count" -ge 2
grep -E 'metadata label matchAscribed -> operation#[0-9]+ -> term#' "$TMP_DIR/multi-app.out" >/dev/null
grep -F '[match-elim]' "$TMP_DIR/multi-app.out" >/dev/null

cat >"$TMP_DIR/type-view-sharing.p" <<'EOF_TYPE_VIEW_SHARING'
Bool := @{
	true : *;
	false : *;
};

Two := @{
	one : *;
	zero : *;
};

idBool := \x : Bool => x;
idTwo := \y : Two => y;
EOF_TYPE_VIEW_SHARING

./read_file.out "$TMP_DIR/type-view-sharing.p" >"$TMP_DIR/type-view-sharing.out"
./read_file.out --write-artifact "$TMP_DIR/type-view-sharing.apo" "$TMP_DIR/type-view-sharing.p" >"$TMP_DIR/type-view-sharing-artifact.out"
view_id_bool_term=$(awk '/metadata label idBool -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/type-view-sharing.out")
view_id_two_term=$(awk '/metadata label idTwo -> operation#[0-9]+ -> term#/ { sub("term#", "", $7); print $7 }' "$TMP_DIR/type-view-sharing.out")
test "$view_id_bool_term" = "$view_id_two_term"
view_bool_shape=$(awk '/interface type Bool / { sub("core_representation_anchor_type#", "", $5); print $5 }' "$TMP_DIR/type-view-sharing.out")
view_two_shape=$(awk '/interface type Two / { sub("core_representation_anchor_type#", "", $5); print $5 }' "$TMP_DIR/type-view-sharing.out")
view_bool_key=$(awk '/interface type Bool / { sub("representation_fingerprint=", "", $7); print $7 }' "$TMP_DIR/type-view-sharing.out")
view_two_key=$(awk '/interface type Two / { sub("representation_fingerprint=", "", $7); print $7 }' "$TMP_DIR/type-view-sharing.out")
test -n "$view_bool_shape"
test "$view_bool_shape" = "$view_two_shape"
test -n "$view_bool_key"
test "$view_bool_key" = "$view_two_key"
grep -q 'interface constructor type_export#0.true ordinal=0 fields=0 classifier_family=' "$TMP_DIR/type-view-sharing.out"
grep -q 'interface constructor type_export#1.one ordinal=0 fields=0 classifier_family=' "$TMP_DIR/type-view-sharing.out"
grep -q 'interface constructor type_export#0.false ordinal=1 fields=0 classifier_family=' "$TMP_DIR/type-view-sharing.out"
grep -q 'interface constructor type_export#1.zero ordinal=1 fields=0 classifier_family=' "$TMP_DIR/type-view-sharing.out"
grep -Eq 'term Bool := TYPE_VIEW\(Bool, core=TYPE_FORMER\(rep#[0-9]+\), source=TYPE_DECLARATION\(Bool\)\)' "$TMP_DIR/type-view-sharing.out"
grep -Eq 'term Two := TYPE_VIEW\(Two, core=TYPE_FORMER\(rep#[0-9]+\), source=TYPE_DECLARATION\(Two\)\)' "$TMP_DIR/type-view-sharing.out"
./read_file.out --check-exports-core-shape-equal "$TMP_DIR/type-view-sharing.apo" Bool Two >"$TMP_DIR/type-view-core-shape.out"
./read_file.out --check-exports-view-shape-equal "$TMP_DIR/type-view-sharing.apo" Bool Two >"$TMP_DIR/type-view-view-shape.out"
grep -q 'exports-core-shape-equal Bool Two yes' "$TMP_DIR/type-view-core-shape.out"
grep -q 'exports-view-shape-equal Bool Two no' "$TMP_DIR/type-view-view-shape.out"

cat >"$TMP_DIR/type-view-mixed-match-negative.p" <<'EOF_TYPE_VIEW_MIXED_MATCH_NEGATIVE'
Bool := @{
	true : *;
	false : *;
};

Two := @{
	one : *;
	zero : *;
};

bad := \b : Bool => b @one => Bool.true @false => Bool.false;
EOF_TYPE_VIEW_MIXED_MATCH_NEGATIVE

if ./read_file.out "$TMP_DIR/type-view-mixed-match-negative.p" >"$TMP_DIR/type-view-mixed-match-negative.out" 2>"$TMP_DIR/type-view-mixed-match-negative.err"; then
	echo "mixed Bool/Two constructor match unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/dependent-constructor-field.p" <<'EOF_DEPENDENT_CONSTRUCTOR_FIELD'
Sigma := \A : @ => \B : A -> @ => @{
	mk : (a : A) -> B a -> *;
};

first := \A : @ => \B : A -> @ => \p : Sigma A B =>
	p @mk a b => a;
EOF_DEPENDENT_CONSTRUCTOR_FIELD

./read_file.out "$TMP_DIR/dependent-constructor-field.p" >"$TMP_DIR/dependent-constructor-field.out"
grep -q 'metadata label first -> operation#[0-9][0-9]* -> term#' "$TMP_DIR/dependent-constructor-field.out"
grep -q 'interface constructor type_export#0.mk ordinal=0 fields=2 classifier_family=' "$TMP_DIR/dependent-constructor-field.out"
grep -E 'has-type VAR\(_#[0-9]+\) APP\(VAR\(_#[0-9]+\), VAR\(_#[0-9]+\)\) \[match-pattern-assumption\]' "$TMP_DIR/dependent-constructor-field.out" >/dev/null

cat >"$TMP_DIR/dependent-constructor-provider.p" <<'EOF_DEPENDENT_CONSTRUCTOR_PROVIDER'
Sigma := \A : @ => \B : A -> @ => @{
	mk : (a : A) -> B a -> *;
};
EOF_DEPENDENT_CONSTRUCTOR_PROVIDER

cat >"$TMP_DIR/dependent-constructor-import-user.p" <<'EOF_DEPENDENT_CONSTRUCTOR_IMPORT_USER'
import Sigma;

Nat := @{
	zero : *;
	succ : * -> *;
};

ConstNat := \x : Nat => Nat;
main := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
EOF_DEPENDENT_CONSTRUCTOR_IMPORT_USER

./read_file.out --write-artifact "$TMP_DIR/Sigma.apo" \
	--namespace Sigma \
	"$TMP_DIR/dependent-constructor-provider.p" >"$TMP_DIR/dependent-constructor-provider.out"
grep -Eq 'constructor \(Sigma A B\)\.mk readback_fields=2 classifier_family=[0-9]+' "$TMP_DIR/dependent-constructor-provider.out"
./read_file.out --write-artifact "$TMP_DIR/SigmaUser.apo" \
	--import-interface "$TMP_DIR/Sigma.apo" \
	"$TMP_DIR/dependent-constructor-import-user.p" >"$TMP_DIR/dependent-constructor-import-user.out"
grep -Eq 'metadata label main -> operation#[0-9]+ -> term#' "$TMP_DIR/dependent-constructor-import-user.out"
grep -Fq 'metadata external-ref Sigma -> term#' "$TMP_DIR/dependent-constructor-import-user.out"
grep -Eq 'has-type CONSTRUCTOR\(rep#[0-9]+\.ordinal#[0-9]+\) PI\(' "$TMP_DIR/dependent-constructor-import-user.out"
grep -Fq 'APP(LAMBDA(_#' "$TMP_DIR/dependent-constructor-import-user.out"
grep -Fq 'APP(APP(EXTERNAL_REF(Sigma.Sigma)' "$TMP_DIR/dependent-constructor-import-user.out"

cat >"$TMP_DIR/dependent-constructor-shape-key.p" <<'EOF_DEPENDENT_CONSTRUCTOR_SHAPE_KEY'
Sigma := \A : @ => \B : A -> @ => @{
	mk : (a : A) -> B a -> *;
};

Sigma2 := \X : @ => \Y : X -> @ => @{
	mk : (x : X) -> Y x -> *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

ConstNat := \n : Nat => Nat;
p1 := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
p2 := (Sigma2 Nat ConstNat).mk Nat.zero Nat.zero;
EOF_DEPENDENT_CONSTRUCTOR_SHAPE_KEY

./read_file.out "$TMP_DIR/dependent-constructor-shape-key.p" >"$TMP_DIR/dependent-constructor-shape-key.out"
sigma_key=$(awk '/interface type Sigma / { sub("representation_fingerprint=", "", $7); print $7 }' "$TMP_DIR/dependent-constructor-shape-key.out")
sigma2_key=$(awk '/interface type Sigma2 / { sub("representation_fingerprint=", "", $7); print $7 }' "$TMP_DIR/dependent-constructor-shape-key.out")
sigma_core=$(awk '/interface type Sigma / { sub("core_representation_anchor_type#", "", $5); print $5 }' "$TMP_DIR/dependent-constructor-shape-key.out")
sigma2_core=$(awk '/interface type Sigma2 / { sub("core_representation_anchor_type#", "", $5); print $5 }' "$TMP_DIR/dependent-constructor-shape-key.out")
test -n "$sigma_key"
test "$sigma_key" = "$sigma2_key"
test -n "$sigma_core"
test "$sigma_core" = "$sigma2_core"
grep -Eq 'constructor \(Sigma A B\)\.mk readback_fields=2 classifier_family=[0-9]+' "$TMP_DIR/dependent-constructor-shape-key.out"
grep -Eq 'constructor \(Sigma2 X Y\)\.mk readback_fields=2 classifier_family=[0-9]+' "$TMP_DIR/dependent-constructor-shape-key.out"
grep -Eq 'term Sigma2 := TYPE_VIEW\(Sigma2, core=TYPE_FORMER\(rep#[0-9]+\), source=TYPE_DECLARATION\(Sigma2\)\)' "$TMP_DIR/dependent-constructor-shape-key.out"

cat >"$TMP_DIR/bad-ascription.p" <<'EOF_BAD_ASCRIPTION'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

bad := Bool.true :: Nat;
EOF_BAD_ASCRIPTION

if ./read_file.out "$TMP_DIR/bad-ascription.p" >"$TMP_DIR/bad-ascription.out" 2>"$TMP_DIR/bad-ascription.err"; then
	echo "bad ascription unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/constructor-value.p" <<'EOF_CONSTRUCTOR_VALUE'
Bool := @{
        true : *;
        false : *;
};

main := Bool.true;
main :: Bool;
EOF_CONSTRUCTOR_VALUE

./read_file.out --write-artifact "$TMP_DIR/ConstructorValue.apo" \
	"$TMP_DIR/constructor-value.p" >"$TMP_DIR/constructor-value.out"

awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == constructor_intro_proof_kind && !bad_classifier) {
			bad_classifier = $4;
		}
		next;
	}
	$1 == "judgement" && $6 == type_formation_proof_kind && !done {
		$5 = bad_classifier;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = bad_classifier;
	}
	{ print }
' constructor_intro_proof_kind="$PROOF_KIND_CONSTRUCTOR_INTRO" \
	type_formation_proof_kind="$PROOF_KIND_TYPE_FORMATION_INTRO" \
	"$TMP_DIR/identity.apo" "$TMP_DIR/identity.apo" >"$TMP_DIR/BadTypeFormationClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadTypeFormationClassifier.apo" >"$TMP_DIR/bad-type-formation-classifier.out" 2>"$TMP_DIR/bad-type-formation-classifier.err"; then
	echo "bad type formation classifier artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == type_formation_proof_kind && !bad_classifier) {
			bad_classifier = $5;
		}
		next;
	}
	$1 == "judgement" && $6 == declaration_proof_kind && !done {
		$5 = bad_classifier;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = bad_classifier;
	}
	{ print }
' type_formation_proof_kind="$PROOF_KIND_TYPE_FORMATION_INTRO" \
	declaration_proof_kind="$PROOF_KIND_DECLARATION" \
	"$TMP_DIR/ConstructorValue.apo" "$TMP_DIR/ConstructorValue.apo" >"$TMP_DIR/BadConstructorIntroClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadConstructorIntroClassifier.apo" >"$TMP_DIR/bad-constructor-intro-classifier.out" 2>"$TMP_DIR/bad-constructor-intro-classifier.err"; then
	echo "bad constructor intro classifier artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "judgement" && $6 == binder_assumption_proof_kind && !done {
		$6 = match_pattern_assumption_proof_kind;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$3 = match_pattern_assumption_proof_kind;
	}
	{ print }
' binder_assumption_proof_kind="$PROOF_KIND_BINDER_ASSUMPTION" \
	match_pattern_assumption_proof_kind="$PROOF_KIND_MATCH_PATTERN_ASSUMPTION" \
	"$TMP_DIR/identity.apo" >"$TMP_DIR/BadPatternAssumption.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadPatternAssumption.apo" >"$TMP_DIR/bad-pattern-assumption.out" 2>"$TMP_DIR/bad-pattern-assumption.err"; then
	echo "bad pattern assumption artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 == binder_assumption_proof_kind && !done {
		$8 = 999;
		done = 1;
	}
	{ print }
' binder_assumption_proof_kind="$PROOF_KIND_BINDER_ASSUMPTION" \
	"$TMP_DIR/identity.apo" >"$TMP_DIR/BadBinderContext.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadBinderContext.apo" >"$TMP_DIR/bad-binder-context.out" 2>"$TMP_DIR/bad-binder-context.err"; then
	echo "bad binder context artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 == binder_assumption_proof_kind && !done {
		$7 = 0;
		$8 = 4294967295;
		$9 = 4294967295;
		$10 = 4294967295;
		done = 1;
	}
	{ print }
' binder_assumption_proof_kind="$PROOF_KIND_BINDER_ASSUMPTION" \
	"$TMP_DIR/identity.apo" >"$TMP_DIR/MissingBinderContext.apo"
if ./read_file.out --read-graph "$TMP_DIR/MissingBinderContext.apo" >"$TMP_DIR/missing-binder-context.out" 2>"$TMP_DIR/missing-binder-context.err"; then
	echo "missing binder context artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == binder_assumption_proof_kind && !target_proof) {
			target_proof = $7;
			bad_classifier = $4;
		}
		next;
	}
	$1 == "judgement" && $7 == target_proof {
		$5 = bad_classifier;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = bad_classifier;
	}
	{ print }
' binder_assumption_proof_kind="$PROOF_KIND_BINDER_ASSUMPTION" \
	"$TMP_DIR/identity.apo" "$TMP_DIR/identity.apo" >"$TMP_DIR/BadBinderClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadBinderClassifier.apo" >"$TMP_DIR/bad-binder-classifier.out" 2>"$TMP_DIR/bad-binder-classifier.err"; then
	echo "bad binder classifier artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 == lambda_intro_proof_kind && !done {
		$13 = $16;
		done = 1;
	}
	{ print }
' lambda_intro_proof_kind="$PROOF_KIND_LAMBDA_INTRO" \
	"$TMP_DIR/identity.apo" >"$TMP_DIR/BadLambdaIntroBinderPremise.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadLambdaIntroBinderPremise.apo" >"$TMP_DIR/bad-lambda-intro-binder-premise.out" 2>"$TMP_DIR/bad-lambda-intro-binder-premise.err"; then
	echo "bad lambda intro binder premise artifact unexpectedly passed" >&2
	exit 1
fi
./read_file.out --write-artifact "$TMP_DIR/ListInductionPattern.apo" examples/09_list_induction.p >"$TMP_DIR/list-induction-pattern.out"
if grep -q 'TELESCOPE' "$TMP_DIR/list-induction-pattern.out"; then
	echo "newly generated list induction graph unexpectedly contains TELESCOPE" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 != match_pattern_assumption_proof_kind && !done {
		$7 = 2;
		done = 1;
	}
	{ print }
' match_pattern_assumption_proof_kind="$PROOF_KIND_MATCH_PATTERN_ASSUMPTION" \
	"$TMP_DIR/ListInductionPattern.apo" >"$TMP_DIR/BadUnexpectedProofContext.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadUnexpectedProofContext.apo" >"$TMP_DIR/bad-unexpected-proof-context.out" 2>"$TMP_DIR/bad-unexpected-proof-context.err"; then
	echo "bad unexpected proof context artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/normalization_equal-match.p" <<'EOF_NORMALIZATION_EQUAL_MATCH'
Bool := @{
        true : *;
        false : *;
};

identity := \x : Bool => x;
betaMain := identity Bool.true;
betaExpected := Bool.true;
matchMain := Bool.true @true => Bool.false @false => Bool.true;
matchExpected := Bool.false;
EOF_NORMALIZATION_EQUAL_MATCH

./read_file.out --write-artifact "$TMP_DIR/NormalizationEqualMatch.apo" \
	"$TMP_DIR/normalization_equal-match.p" >"$TMP_DIR/normalization_equal-match.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualMatch.apo" \
	betaMain betaExpected --reduction-mode beta >"$TMP_DIR/normalization_equal-beta.out"
grep -q '^exports-normalization-equal betaMain betaExpected mode=beta yes$' "$TMP_DIR/normalization_equal-beta.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualMatch.apo" \
	betaMain betaExpected --reduction-mode match >"$TMP_DIR/normalization_equal-beta-match-only.out"
grep -q '^exports-normalization-equal betaMain betaExpected mode=match no$' "$TMP_DIR/normalization_equal-beta-match-only.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualMatch.apo" \
	matchMain matchExpected --reduction-mode match >"$TMP_DIR/normalization_equal-match-only.out"
grep -q '^exports-normalization-equal matchMain matchExpected mode=match yes$' "$TMP_DIR/normalization_equal-match-only.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualMatch.apo" \
	matchMain matchExpected --reduction-mode default >"$TMP_DIR/normalization_equal-match-default.out"
grep -q '^exports-normalization-equal matchMain matchExpected mode=default yes$' "$TMP_DIR/normalization_equal-match-default.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualMatch.apo" \
	matchMain matchExpected --reduction-mode beta >"$TMP_DIR/normalization_equal-match-beta-only.out"
grep -q '^exports-normalization-equal matchMain matchExpected mode=beta no$' "$TMP_DIR/normalization_equal-match-beta-only.out"

cat >"$TMP_DIR/normalization_equal-uniform-beta-match.p" <<'EOF_NORMALIZATION_EQUAL_UNIFORM_BETA_MATCH'
Bool := @{
        true : *;
        false : *;
};

identity := \x : Bool => x;
matchExpected := Bool.false;
uniformBetaMatch := Bool.true @true => (identity Bool.false) @false => (identity Bool.false);
EOF_NORMALIZATION_EQUAL_UNIFORM_BETA_MATCH

./read_file.out --write-artifact "$TMP_DIR/NormalizationEqualUniformBetaMatch.apo" \
	"$TMP_DIR/normalization_equal-uniform-beta-match.p" >"$TMP_DIR/normalization_equal-uniform-beta-match.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualUniformBetaMatch.apo" \
	uniformBetaMatch matchExpected --reduction-mode default >"$TMP_DIR/normalization_equal-uniform-beta-match-default.out"
grep -q '^exports-normalization-equal uniformBetaMatch matchExpected mode=default yes$' "$TMP_DIR/normalization_equal-uniform-beta-match-default.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualUniformBetaMatch.apo" \
	uniformBetaMatch matchExpected --reduction-mode match >"$TMP_DIR/normalization_equal-uniform-beta-match-only.out"
grep -q '^exports-normalization-equal uniformBetaMatch matchExpected mode=match no$' "$TMP_DIR/normalization_equal-uniform-beta-match-only.out"

cat >"$TMP_DIR/normalization_equal-uniform-arg-match.p" <<'EOF_NORMALIZATION_EQUAL_UNIFORM_ARG_MATCH'
Nat := @{
        zero : *;
        succ : * -> *;
};

identityNat := \x : Nat => x;
expected := Nat.succ Nat.zero;
uniformNormalizationEqualArgMatch := Nat.zero
        @zero   => Nat.succ (identityNat Nat.zero)
        @succ n => Nat.succ Nat.zero;
EOF_NORMALIZATION_EQUAL_UNIFORM_ARG_MATCH

./read_file.out --write-artifact "$TMP_DIR/NormalizationEqualUniformArgMatch.apo" \
	"$TMP_DIR/normalization_equal-uniform-arg-match.p" >"$TMP_DIR/normalization_equal-uniform-arg-match.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualUniformArgMatch.apo" \
	uniformNormalizationEqualArgMatch expected --reduction-mode default >"$TMP_DIR/normalization_equal-uniform-arg-match-default.out"
grep -q '^exports-normalization-equal uniformNormalizationEqualArgMatch expected mode=default yes$' "$TMP_DIR/normalization_equal-uniform-arg-match-default.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/NormalizationEqualUniformArgMatch.apo" \
	uniformNormalizationEqualArgMatch expected --reduction-mode match >"$TMP_DIR/normalization_equal-uniform-arg-match-only.out"
grep -q '^exports-normalization-equal uniformNormalizationEqualArgMatch expected mode=match no$' "$TMP_DIR/normalization_equal-uniform-arg-match-only.out"

cat >"$TMP_DIR/multiple-match-motives.p" <<'EOF_MULTIPLE_MATCH_MOTIVES'
Bool := @{
        true : *;
        false : *;
};

m1 := Bool.true @true => Bool.false @false => Bool.false;
m2 := Bool.false @true => Bool.false @false => Bool.false;
EOF_MULTIPLE_MATCH_MOTIVES

./read_file.out --write-artifact "$TMP_DIR/MultipleMatchMotives.apo" \
	"$TMP_DIR/multiple-match-motives.p" >"$TMP_DIR/multiple-match-motives.out"
grep -q '^term m1 ' "$TMP_DIR/MultipleMatchMotives.apo"
grep -q '^term m2 ' "$TMP_DIR/MultipleMatchMotives.apo"

cat >"$TMP_DIR/append-normalization_equal.p" <<'EOF_APPEND_NORMALIZATION_EQUAL'
Nat := @{
        zero : *;
        succ : * -> *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

append := \A : @ =>
        \xs : List A =>
                xs @nil         => (\ys : List A => ys)
                   @cons x rest => (\ys : List A => (List A).cons x (*rest ys));

n0 := Nat.zero;
n1 := Nat.succ Nat.zero;
n2 := Nat.succ n1;
n3 := Nat.succ n2;

list01 := (List Nat).cons n0 ((List Nat).cons n1 (List Nat).nil);
list23 := (List Nat).cons n2 ((List Nat).cons n3 (List Nat).nil);
list0 := (List Nat).cons n0 (List Nat).nil;
oneExpected := (List Nat).cons n0 list23;
oneMain := append Nat list0 list23;
expected := (List Nat).cons n0 ((List Nat).cons n1 ((List Nat).cons n2 ((List Nat).cons n3 (List Nat).nil)));
main := append Nat list01 list23;
EOF_APPEND_NORMALIZATION_EQUAL

./read_file.out --check-source-exports-normalization-equal oneMain oneExpected \
	--reduction-mode default "$TMP_DIR/append-normalization_equal.p" \
	>"$TMP_DIR/append-normalization_equal-one-source.out"
grep -q '^source-exports-normalization-equal oneMain oneExpected mode=default yes$' \
	"$TMP_DIR/append-normalization_equal-one-source.out"
./read_file.out --check-source-exports-normalization-equal main expected \
	--reduction-mode default "$TMP_DIR/append-normalization_equal.p" \
	>"$TMP_DIR/append-normalization_equal-source.out"
grep -q '^source-exports-normalization-equal main expected mode=default yes$' \
	"$TMP_DIR/append-normalization_equal-source.out"
./read_file.out --write-artifact "$TMP_DIR/AppendNormalizationEqual.apo" \
	"$TMP_DIR/append-normalization_equal.p" >"$TMP_DIR/append-normalization_equal.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/AppendNormalizationEqual.apo" \
	oneMain oneExpected --reduction-mode default >"$TMP_DIR/append-normalization_equal-one-default.out"
grep -q '^exports-normalization-equal oneMain oneExpected mode=default yes$' \
	"$TMP_DIR/append-normalization_equal-one-default.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/AppendNormalizationEqual.apo" \
	main expected --reduction-mode default >"$TMP_DIR/append-normalization_equal-default.out"
grep -q '^exports-normalization-equal main expected mode=default yes$' "$TMP_DIR/append-normalization_equal-default.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/AppendNormalizationEqual.apo" \
	main expected --reduction-mode beta >"$TMP_DIR/append-normalization_equal-beta.out"
grep -q '^exports-normalization-equal main expected mode=beta no$' "$TMP_DIR/append-normalization_equal-beta.out"
awk '
	$1 == "proof" && $3 == pi_formation_proof_kind && !done {
		$13 = 999;
		done = 1;
	}
	{ print }
' pi_formation_proof_kind="$PROOF_KIND_PI_FORMATION_INTRO" \
	"$TMP_DIR/AppendNormalizationEqual.apo" >"$TMP_DIR/BadPiFormationPremise.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadPiFormationPremise.apo" >"$TMP_DIR/bad-pi-formation-premise.out" 2>"$TMP_DIR/bad-pi-formation-premise.err"; then
	echo "bad pi formation premise artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/shared-namespace-a.p" <<'EOF_SHARED_NAMESPACE_A'
Nat := @{
        zero : *;
        succ : * -> *;
};
EOF_SHARED_NAMESPACE_A

cat >"$TMP_DIR/shared-namespace-b.p" <<'EOF_SHARED_NAMESPACE_B'
id := \x : Nat => x;
id :: Nat -> Nat;
EOF_SHARED_NAMESPACE_B

./read_file.out --write-artifact "$TMP_DIR/Shared.apo" \
	--namespace Shared \
	"$TMP_DIR/shared-namespace-a.p" \
	"$TMP_DIR/shared-namespace-b.p" >"$TMP_DIR/shared-namespace.out"
grep -q '^term Nat .* namespace Shared$' "$TMP_DIR/Shared.apo"
grep -q '^term id .* namespace Shared$' "$TMP_DIR/Shared.apo"
grep -q '^type Nat .* namespace Shared$' "$TMP_DIR/Shared.apo"
./read_file.out --write-artifact "$TMP_DIR/DottedNamespace.apo" \
	--namespace Shared.Core \
	"$TMP_DIR/shared-namespace-a.p" >"$TMP_DIR/dotted-namespace.out"
grep -q '^term Nat .* namespace Shared.Core$' "$TMP_DIR/DottedNamespace.apo"

cat >"$TMP_DIR/match-graph.p" <<'EOF_MATCH_GRAPH'
Bool := @{
        true : *;
        false : *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

null := \lst : List Bool =>
        lst @nil => Bool.true
            @cons x xs => Bool.false;

empty := (List Bool).nil;
one := (List Bool).cons Bool.true empty;

main := null empty;
EOF_MATCH_GRAPH

./read_file.out --write-artifact "$TMP_DIR/MatchGraph.apo" "$TMP_DIR/match-graph.p" >"$TMP_DIR/match-graph.out"
awk '
	$1 == "type_constructor" && $3 == "cons" {
		if ($9 == "4294967295") {
			exit 1;
		}
		found = 1;
	}
	END {
		if (!found) {
			exit 1;
		}
	}
' "$TMP_DIR/MatchGraph.apo"
./read_file.out --read-graph "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/match-graph-read.out"
grep -q 'terms=70 cases=12 case_binders=12 frames=1 types=3 constructors=6 type_exprs=3 judgements=38 proofs=38' "$TMP_DIR/match-graph-read.out"
grep -q 'relocation_resolved_constructor_owners=8' "$TMP_DIR/match-graph-read.out"
grep -q 'resolved constructor owner kind=2 .* ordinal=0' "$TMP_DIR/match-graph-read.out"
grep -q 'resolved constructor owner kind=2 .* ordinal=1' "$TMP_DIR/match-graph-read.out"
awk '
	$1 == "proof" && !done {
		$3 = 999;
		done = 1;
	}
	{ print }
' "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProof.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProof.apo" >"$TMP_DIR/bad-proof.out" 2>"$TMP_DIR/bad-proof.err"; then
	echo "bad proof artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "judgement" && $6 == from_kind && !done {
		$6 = to_kind;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$3 = to_kind;
	}
	{ print }
' from_kind="$PROOF_KIND_MATCH_ELIM" \
	to_kind="$PROOF_KIND_LAMBDA_INTRO" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProofShape.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofShape.apo" >"$TMP_DIR/bad-proof-shape.out" 2>"$TMP_DIR/bad-proof-shape.err"; then
	echo "bad proof shape artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $11 > 0 && !done {
		$15 = 999;
		done = 1;
	}
	{ print }
' "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProofEdge.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofEdge.apo" >"$TMP_DIR/bad-proof-edge.out" 2>"$TMP_DIR/bad-proof-edge.err"; then
	echo "bad proof edge artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "proof" && $11 > 0 && !target_proof) {
			target_proof = $2;
			expected_premise = $15;
		}
		if ($1 == "proof" && target_proof && $2 != target_proof &&
			$2 != expected_premise && !replacement_proof) {
			replacement_proof = $2;
		}
		next;
	}
	$1 == "proof" && $2 == target_proof {
		$15 = replacement_proof;
	}
	{ print }
' "$TMP_DIR/MatchGraph.apo" "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProofEdgeMismatch.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofEdgeMismatch.apo" >"$TMP_DIR/bad-proof-edge-mismatch.out" 2>"$TMP_DIR/bad-proof-edge-mismatch.err"; then
	echo "bad proof edge mismatch artifact unexpectedly passed" >&2
	exit 1
fi
./read_file.out --write-artifact "$TMP_DIR/AppPremiseKind.apo" examples/07_add.p >"$TMP_DIR/app-premise-kind-artifact.out"
awk '
	FNR == NR {
		if ($1 == "counts") {
			next_relation_id = $29;
			next_proof_id = $33;
		}
		if ($1 == "proof" && $3 == app_elim_proof_kind && $11 > 0 && !target_proof) {
			target_proof = $2;
			premise_subject = $13;
			premise_classifier = $14;
			premise_proof = $15;
		}
		next;
	}
	$1 == "counts" {
		$29 = $29 + 1;
		$31 = $31 + 1;
		$33 = $33 + 1;
		$35 = $35 + 1;
	}
	$1 == "judgements" {
		$2 = $2 + 1;
	}
	$1 == "proofs" {
		$2 = $2 + 1;
	}
	$1 == "proof" && $2 == target_proof {
		$12 = is_type_kind;
		$15 = next_proof_id;
	}
	$1 == "END" && $2 == "graph" {
		print "judgement", next_relation_id, is_type_kind, premise_subject, premise_classifier, is_type_proof_kind, next_proof_id;
		print "proof", next_proof_id, is_type_proof_kind, is_type_kind, premise_subject, premise_classifier, 0, 4294967295, 4294967295, 4294967295, 1, has_type_kind, premise_subject, premise_classifier, premise_proof;
	}
	{ print }
' app_elim_proof_kind="$PROOF_KIND_APP_ELIM" \
	is_type_kind="$JUDGEMENT_KIND_IS_TYPE" \
	is_type_proof_kind="$PROOF_KIND_IS_TYPE_FROM_HAS_TYPE" \
	has_type_kind="$JUDGEMENT_KIND_HAS_TYPE" \
	"$TMP_DIR/AppPremiseKind.apo" "$TMP_DIR/AppPremiseKind.apo" >"$TMP_DIR/BadAppPremiseKind.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadAppPremiseKind.apo" >"$TMP_DIR/bad-app-premise-kind.out" 2>"$TMP_DIR/bad-app-premise-kind.err"; then
	echo "bad app premise kind artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && !done {
		$4 = is_type_kind;
		done = 1;
	}
	{ print }
' is_type_kind="$JUDGEMENT_KIND_IS_TYPE" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProofConclusion.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofConclusion.apo" >"$TMP_DIR/bad-proof-conclusion.out" 2>"$TMP_DIR/bad-proof-conclusion.err"; then
	echo "bad proof conclusion artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 == match_type_formation_proof_kind && !done {
		print $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, 0;
		done = 1;
		next;
	}
	{ print }
' match_type_formation_proof_kind="$PROOF_KIND_MATCH_TYPE_FORMATION_INTRO" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadMotiveProof.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadMotiveProof.apo" >"$TMP_DIR/bad-motive-proof.out" 2>"$TMP_DIR/bad-motive-proof.err"; then
	echo "bad motive proof artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == match_elim_proof_kind && !match_classifier) {
			match_classifier = $5;
		}
		if ($1 == "term_node") {
			term_tag[$2] = $3;
			term_arg0[$2] = $4;
			term_arg1[$2] = $5;
		}
		next;
	}
	FNR == 1 {
		motive_lambda = term_arg0[match_classifier];
		motive_body = term_arg1[motive_lambda];
	}
	$1 == "term_node" && $2 == motive_body {
		print $1, $2, 11, 999;
		next;
	}
	{ print }
' match_elim_proof_kind="$PROOF_KIND_MATCH_ELIM" \
	"$TMP_DIR/MatchGraph.apo" "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadMatchElimMotiveShape.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadMatchElimMotiveShape.apo" >"$TMP_DIR/bad-match-elim-motive-shape.out" 2>"$TMP_DIR/bad-match-elim-motive-shape.err"; then
	echo "bad match-elim motive shape artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "counts" {
		next_relation_id = $29;
		next_proof_id = $33;
		$29 = $29 + 1;
		$31 = $31 + 1;
		$33 = $33 + 1;
		$35 = $35 + 1;
	}
	$1 == "judgements" {
		$2 = $2 + 1;
	}
	$1 == "proofs" {
		$2 = $2 + 1;
	}
	$1 == "END" && $2 == "graph" {
		print "judgement", next_relation_id, is_type_kind, 0, 1, is_type_proof_kind, next_proof_id;
		print "proof", next_proof_id, is_type_proof_kind, is_type_kind, 0, 1, 0, 4294967295, 4294967295, 4294967295, 0;
	}
	{ print }
' is_type_kind="$JUDGEMENT_KIND_IS_TYPE" \
	is_type_proof_kind="$PROOF_KIND_IS_TYPE_FROM_HAS_TYPE" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadIsTypeProof.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIsTypeProof.apo" >"$TMP_DIR/bad-is-type-proof.out" 2>"$TMP_DIR/bad-is-type-proof.err"; then
	echo "bad is-type proof artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "counts" {
		next_relation_id = $29;
		next_proof_id = $33;
		$29 = $29 + 1;
		$31 = $31 + 1;
		$33 = $33 + 1;
		$35 = $35 + 1;
	}
	$1 == "judgements" {
		$2 = $2 + 1;
	}
	$1 == "proofs" {
		$2 = $2 + 1;
	}
	$1 == "END" && $2 == "graph" {
		print "judgement", next_relation_id, is_type_kind, 2, 3, is_type_proof_kind, next_proof_id;
		print "proof", next_proof_id, is_type_proof_kind, is_type_kind, 2, 3, 0, 4294967295, 4294967295, 4294967295, 1, has_type_kind, 0, 3, 0;
	}
	{ print }
' is_type_kind="$JUDGEMENT_KIND_IS_TYPE" \
	is_type_proof_kind="$PROOF_KIND_IS_TYPE_FROM_HAS_TYPE" \
	has_type_kind="$JUDGEMENT_KIND_HAS_TYPE" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadIsTypePremiseSubject.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIsTypePremiseSubject.apo" >"$TMP_DIR/bad-is-type-premise-subject.out" 2>"$TMP_DIR/bad-is-type-premise-subject.err"; then
	echo "bad is-type premise subject artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "counts" {
		next_relation_id = $29;
		next_proof_id = $33;
		$29 = $29 + 1;
		$31 = $31 + 1;
		$33 = $33 + 1;
		$35 = $35 + 1;
	}
	$1 == "judgements" {
		$2 = $2 + 1;
	}
	$1 == "proofs" {
		$2 = $2 + 1;
	}
	$1 == "END" && $2 == "graph" {
		print "judgement", next_relation_id, has_type_kind, 0, 1, universe_cumulativity_proof_kind, next_proof_id;
		print "proof", next_proof_id, universe_cumulativity_proof_kind, has_type_kind, 0, 1, 0, 4294967295, 4294967295, 4294967295, 0;
	}
	{ print }
' has_type_kind="$JUDGEMENT_KIND_HAS_TYPE" \
	universe_cumulativity_proof_kind="$PROOF_KIND_UNIVERSE_CUMULATIVITY" \
	"$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadUniverseCumulativitySubject.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadUniverseCumulativitySubject.apo" >"$TMP_DIR/bad-universe-cumulativity-subject.out" 2>"$TMP_DIR/bad-universe-cumulativity-subject.err"; then
	echo "bad universe cumulativity subject artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "judgement" && !target_proof) {
			target_proof = $7;
		}
		next;
	}
	$1 == "judgement" && $7 == target_proof {
		$6 = declaration_proof_kind;
	}
	$1 == "proof" && $2 == target_proof {
		print $1, $2, declaration_proof_kind, $4, $5, $6, $7, $8, $9, $10, 0;
		next;
	}
	{ print }
' declaration_proof_kind="$PROOF_KIND_DECLARATION" \
	"$TMP_DIR/MatchGraph.apo" "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadDeclarationProof.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadDeclarationProof.apo" >"$TMP_DIR/bad-declaration-proof.out" 2>"$TMP_DIR/bad-declaration-proof.err"; then
	echo "bad declaration proof artifact unexpectedly passed" >&2
	exit 1
fi
cat >"$TMP_DIR/constructor-declaration.p" <<'EOF_CONSTRUCTOR_DECLARATION'
Nat := @{
        zero : *;
        succ : * -> *;
};

z := Nat.zero;
EOF_CONSTRUCTOR_DECLARATION

./read_file.out --write-artifact "$TMP_DIR/ConstructorDeclaration.apo" \
	"$TMP_DIR/constructor-declaration.p" >"$TMP_DIR/constructor-declaration.out"
awk '
	FNR == NR {
		if ($1 == "proof" && $3 == constructor_intro_proof_kind) {
			target_constructor = $5;
		}
		next;
	}
	$1 == "term_node" && $2 == target_constructor && $3 == constructor_tag {
		$4 = 999;
	}
	{ print }
' constructor_intro_proof_kind="$PROOF_KIND_CONSTRUCTOR_INTRO" \
	constructor_tag="$TERM_TAG_CONSTRUCTOR" \
	"$TMP_DIR/ConstructorDeclaration.apo" "$TMP_DIR/ConstructorDeclaration.apo" >"$TMP_DIR/BadConstructorDeclaration.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadConstructorDeclaration.apo" >"$TMP_DIR/bad-constructor-declaration.out" 2>"$TMP_DIR/bad-constructor-declaration.err"; then
	echo "bad constructor declaration artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "counts" {
		next_proof_id = $33;
		$33 = $33 + 1;
		$35 = $35 + 1;
	}
	$1 == "proofs" {
		$2 = $2 + 1;
	}
	$1 == "proof" {
		last_proof = $0;
	}
	$1 == "END" && $2 == "graph" {
		split(last_proof, fields, " ");
		fields[2] = next_proof_id;
		for (i = 1; i <= length(fields); ++i) {
			printf "%s%s", fields[i], i == length(fields) ? "\n" : " ";
		}
	}
	{ print }
' "$TMP_DIR/MatchGraph.apo" >"$TMP_DIR/BadProofOrphan.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofOrphan.apo" >"$TMP_DIR/bad-proof-orphan.out" 2>"$TMP_DIR/bad-proof-orphan.err"; then
	echo "bad orphan proof artifact unexpectedly passed" >&2
	exit 1
fi
cat >"$TMP_DIR/dependent-match.p" <<'EOF_DEPENDENT_MATCH'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

dep := \b : Bool => b @true => Nat.zero @false => Bool.true;
EOF_DEPENDENT_MATCH

./read_file.out "$TMP_DIR/dependent-match.p" >"$TMP_DIR/dependent-match.out"
grep -q 'has-type MATCH.*APP(LAMBDA.*\[match-elim\]' "$TMP_DIR/dependent-match.out"
grep -q '\[match-type-formation-intro\]' "$TMP_DIR/dependent-match.out"
grep -q 'CASE(true -> TYPE_VIEW(Nat' "$TMP_DIR/dependent-match.out"
grep -q 'CASE(false -> TYPE_VIEW(Bool' "$TMP_DIR/dependent-match.out"
./read_file.out --write-artifact "$TMP_DIR/DependentMatch.apo" "$TMP_DIR/dependent-match.p" >"$TMP_DIR/dependent-match-artifact.out"
awk '
	NR == FNR {
		if ($1 == "judgement" && $6 == match_type_formation_proof_kind && !match_term) {
			match_term = $4;
		}
		if ($1 == "term_node") {
			first_case_by_term[$2] = $5;
		}
		next;
	}
	BEGIN {
		bad_constructor = 999;
	}
	$1 == "match_case" && $2 == first_case_by_term[match_term] && !done {
		$5 = bad_constructor;
		done = 1;
	}
	{ print }
' match_type_formation_proof_kind="$PROOF_KIND_MATCH_TYPE_FORMATION_INTRO" \
	"$TMP_DIR/DependentMatch.apo" "$TMP_DIR/DependentMatch.apo" >"$TMP_DIR/BadMatchTypeFormationCase.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadMatchTypeFormationCase.apo" >"$TMP_DIR/bad-match-type-formation-case.out" 2>"$TMP_DIR/bad-match-type-formation-case.err"; then
	echo "bad match type formation case artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/ih-motive.p" <<'EOF_IH_MOTIVE'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

len := \A : @ =>
        ((\xs : List A =>
                xs @nil => Nat.zero
                   @cons x rest => Nat.succ *rest)
         :: List A -> Nat);
EOF_IH_MOTIVE

./read_file.out "$TMP_DIR/ih-motive.p" >"$TMP_DIR/ih-motive.out"
grep -q 'has-type INDUCTION_HYPOTHESIS.*TYPE_VIEW(Nat.* \[ih-elim\]' "$TMP_DIR/ih-motive.out"
grep -q 'has-type MATCH.*APP(LAMBDA.*\[match-elim\]' "$TMP_DIR/ih-motive.out"
grep -q 'has-type INDUCTION_HYPOTHESIS.*\[ih-elim\] proof#[0-9][0-9]* premises=0' "$TMP_DIR/ih-motive.out"
grep -q 'has-type APP(CONSTRUCTOR.*succ).*INDUCTION_HYPOTHESIS.*\[app-elim\] proof#[0-9][0-9]* premises=2' "$TMP_DIR/ih-motive.out"
./read_file.out --write-artifact "$TMP_DIR/IhMotive.apo" "$TMP_DIR/ih-motive.p" >"$TMP_DIR/ih-motive-artifact.out"
awk '
	$1 == "proof" && $3 == ih_elim_proof_kind && !done {
		$7 = 0;
		$8 = 4294967295;
		$9 = 4294967295;
		$10 = 4294967295;
		done = 1;
	}
	{ print }
' ih_elim_proof_kind="$PROOF_KIND_INDUCTION_HYPOTHESIS_ELIM" \
	"$TMP_DIR/IhMotive.apo" >"$TMP_DIR/BadIhContextKind.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIhContextKind.apo" >"$TMP_DIR/bad-ih-context-kind.out" 2>"$TMP_DIR/bad-ih-context-kind.err"; then
	echo "bad IH context kind artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	$1 == "proof" && $3 == ih_elim_proof_kind && !done {
		$10 = 999;
		done = 1;
	}
	{ print }
' ih_elim_proof_kind="$PROOF_KIND_INDUCTION_HYPOTHESIS_ELIM" \
	"$TMP_DIR/IhMotive.apo" >"$TMP_DIR/BadIhContextField.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIhContextField.apo" >"$TMP_DIR/bad-ih-context-field.out" 2>"$TMP_DIR/bad-ih-context-field.err"; then
	echo "bad IH context field artifact unexpectedly passed" >&2
	exit 1
fi

./read_file.out examples/07_add.p >"$TMP_DIR/add-proof.out"
if grep -q 'expects-type' "$TMP_DIR/add-proof.out"; then
	echo "expectation relation leaked into proof output" >&2
	exit 1
fi
grep -q 'has-type LAMBDA.*\[lambda-intro\] proof#[0-9][0-9]* premises=2' "$TMP_DIR/add-proof.out"

cat >"$TMP_DIR/source-view-nat-match.p" <<'EOF_SOURCE_VIEW_NAT_MATCH'
Nat := @{
	zero : *;
	succ : * -> *;
};

double := \x : Nat =>
	x @zero => Nat.zero
	@succ k => Nat.succ (Nat.succ *k);
EOF_SOURCE_VIEW_NAT_MATCH

./read_file.out "$TMP_DIR/source-view-nat-match.p" >"$TMP_DIR/source-view-nat-match.out"
grep -q 'metadata label double' "$TMP_DIR/source-view-nat-match.out"
grep -q 'INDUCTION_HYPOTHESIS.*TYPE_VIEW(Nat' "$TMP_DIR/source-view-nat-match.out"

./read_file.out --write-artifact "$TMP_DIR/AddProof.apo" examples/07_add.p >"$TMP_DIR/add-proof-artifact.out"
awk '
	$1 == "proof" && $3 == conversion_proof_kind && !done {
		$11 = 1;
		$12 = 1;
		$13 = $5;
		$14 = $6;
		$15 = $2;
		done = 1;
	}
	{ print }
' conversion_proof_kind="$PROOF_KIND_CONVERSION" \
	"$TMP_DIR/AddProof.apo" >"$TMP_DIR/BadProofCycle.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadProofCycle.apo" >"$TMP_DIR/bad-proof-cycle.out" 2>"$TMP_DIR/bad-proof-cycle.err"; then
	echo "bad proof cycle artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "term_node" && $3 == text_literal_tag && !bad_classifier) {
			bad_classifier = $2;
		}
		next;
	}
	$1 == "judgement" && $6 == conversion_proof_kind && !done_relation {
		target_proof = $7;
		$5 = bad_classifier;
		done_relation = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = bad_classifier;
	}
	{ print }
' conversion_proof_kind="$PROOF_KIND_CONVERSION" \
	text_literal_tag="$TERM_TAG_TEXT_LITERAL" \
	"$TMP_DIR/AddProof.apo" "$TMP_DIR/AddProof.apo" >"$TMP_DIR/BadConversionClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadConversionClassifier.apo" >"$TMP_DIR/bad-conversion-classifier.out" 2>"$TMP_DIR/bad-conversion-classifier.err"; then
	echo "bad conversion classifier artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/type-expect-good.p" <<'EOF_TYPE_EXPECT_GOOD'
Nat := @{
        zero : *;
        succ : * -> *;
};

main := Nat.zero;
main :: Nat;
EOF_TYPE_EXPECT_GOOD

cat >"$TMP_DIR/type-expect-bad.p" <<'EOF_TYPE_EXPECT_BAD'
Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

main := Nat.zero;
main :: Bool;
EOF_TYPE_EXPECT_BAD

./read_file.out --write-artifact "$TMP_DIR/TypeExpectGood.apo" "$TMP_DIR/type-expect-good.p" >"$TMP_DIR/type-expect-good.out"
if grep -q 'expects-type' "$TMP_DIR/type-expect-good.out"; then
	echo "type expectation emitted an expects-type relation" >&2
	exit 1
fi
grep -q 'has-type CONSTRUCTOR' "$TMP_DIR/type-expect-good.out"
if ./read_file.out --write-artifact "$TMP_DIR/TypeExpectBad.apo" "$TMP_DIR/type-expect-bad.p" >"$TMP_DIR/type-expect-bad.out" 2>"$TMP_DIR/type-expect-bad.err"; then
	echo "bad type expectation compiled successfully" >&2
	exit 1
fi
grep -q 'metadata resolve-error kind=compile name=main' "$TMP_DIR/type-expect-bad.err"

cat >"$TMP_DIR/intrinsic-nat-to-text.p" <<'EOF_INTRINSIC_NAT_TO_TEXT'
Bool := @{
        true : *;
        false : *;
};

main := #.nat_to_text;
EOF_INTRINSIC_NAT_TO_TEXT

./read_file.out --write-artifact "$TMP_DIR/IntrinsicNatToText.apo" \
	"$TMP_DIR/intrinsic-nat-to-text.p" >"$TMP_DIR/intrinsic-nat-to-text.out"
./read_file.out "$TMP_DIR/intrinsic-nat-to-text.p" >"$TMP_DIR/intrinsic-nat-to-text-print.out"
grep -q '\[intrinsic-type-intro\]' "$TMP_DIR/intrinsic-nat-to-text-print.out"
grep -q '\[host-type-intro\]' "$TMP_DIR/intrinsic-nat-to-text-print.out"
! grep -q '\[intrinsic\]' "$TMP_DIR/intrinsic-nat-to-text-print.out"
! grep -q '\[primitive\]' "$TMP_DIR/intrinsic-nat-to-text-print.out"
cat >"$TMP_DIR/text-literal.p" <<'EOF_TEXT_LITERAL'
main := #"x";
EOF_TEXT_LITERAL
./read_file.out --write-artifact "$TMP_DIR/TextLiteral.apo" \
	"$TMP_DIR/text-literal.p" >"$TMP_DIR/text-literal.out"
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == host_type_intro && !universe_classifier) {
			universe_classifier = $5;
		}
		next;
	}
	$1 == "judgement" && $6 == text_literal_intro && !done {
		$5 = universe_classifier;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = universe_classifier;
	}
	{ print }
' host_type_intro="$PROOF_KIND_HOST_TYPE_INTRO" \
	text_literal_intro="$PROOF_KIND_TEXT_LITERAL_INTRO" \
	"$TMP_DIR/TextLiteral.apo" "$TMP_DIR/TextLiteral.apo" >"$TMP_DIR/BadTextLiteralClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadTextLiteralClassifier.apo" >"$TMP_DIR/bad-text-literal-classifier.out" 2>"$TMP_DIR/bad-text-literal-classifier.err"; then
	echo "bad text literal classifier artifact unexpectedly passed" >&2
	exit 1
fi
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == text_literal_intro && !text_literal) {
			text_literal = $4;
		}
		next;
	}
	$1 == "judgement" && $6 == host_type_intro && !done {
		$4 = text_literal;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$5 = text_literal;
	}
	{ print }
' host_type_intro="$PROOF_KIND_HOST_TYPE_INTRO" \
	text_literal_intro="$PROOF_KIND_TEXT_LITERAL_INTRO" \
	"$TMP_DIR/TextLiteral.apo" "$TMP_DIR/TextLiteral.apo" >"$TMP_DIR/BadTextTypeSubject.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadTextTypeSubject.apo" >"$TMP_DIR/bad-text-type-subject.out" 2>"$TMP_DIR/bad-text-type-subject.err"; then
	echo "bad text type subject artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/int-literal.p" <<'EOF_INT_LITERAL'
main := #42;
EOF_INT_LITERAL
./read_file.out --write-artifact "$TMP_DIR/IntLiteral.apo" \
	"$TMP_DIR/int-literal.p" >"$TMP_DIR/int-literal.out"
grep -q 'has-type INT_LITERAL(42) PRIMITIVE(Int64) \[int-literal-intro\]' "$TMP_DIR/int-literal.out"
grep -q 'has-type INT_LITERAL(42) PRIMITIVE(Int) \[int-literal-intro\]' "$TMP_DIR/int-literal.out"
grep -q '\[host-type-intro\]' "$TMP_DIR/int-literal.out"
awk '
	FNR == NR {
		if ($1 == "judgement" && $6 == host_type_intro && !universe_classifier) {
			universe_classifier = $5;
		}
		next;
	}
	$1 == "judgement" && $6 == int_literal_intro && !done {
		$5 = universe_classifier;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = universe_classifier;
	}
	{ print }
' host_type_intro="$PROOF_KIND_HOST_TYPE_INTRO" \
	int_literal_intro="$PROOF_KIND_INT_LITERAL_INTRO" \
	"$TMP_DIR/IntLiteral.apo" "$TMP_DIR/IntLiteral.apo" >"$TMP_DIR/BadIntLiteralClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIntLiteralClassifier.apo" >"$TMP_DIR/bad-int-literal-classifier.out" 2>"$TMP_DIR/bad-int-literal-classifier.err"; then
	echo "bad int literal classifier artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/int64-literal.p" <<'EOF_INT64_LITERAL'
main := (#9223372036854775807 :: #.Int64);
EOF_INT64_LITERAL
./read_file.out --write-artifact "$TMP_DIR/Int64Literal.apo" \
	"$TMP_DIR/int64-literal.p" >"$TMP_DIR/int64-literal.out"
grep -q 'has-type INT_LITERAL(9223372036854775807) PRIMITIVE(Int64) \[int-literal-intro\]' "$TMP_DIR/int64-literal.out"
! grep -q 'has-type INT_LITERAL(9223372036854775807) PRIMITIVE(Int) \[int-literal-intro\]' "$TMP_DIR/int64-literal.out"

cat >"$TMP_DIR/int32-range-bad.p" <<'EOF_INT32_RANGE_BAD'
main := (#9223372036854775807 :: #.Int);
EOF_INT32_RANGE_BAD
if ./read_file.out "$TMP_DIR/int32-range-bad.p" >"$TMP_DIR/int32-range-bad.out" 2>"$TMP_DIR/int32-range-bad.err"; then
	echo "out-of-range Int literal compiled successfully" >&2
	exit 1
fi
grep -q 'metadata resolve-error kind=compile' "$TMP_DIR/int32-range-bad.err"

cat >"$TMP_DIR/int-arithmetic.p" <<'EOF_INT_ARITHMETIC'
main := #.int_add #40 #2;
EOF_INT_ARITHMETIC
./a.out "$TMP_DIR/int-arithmetic.p" >"$TMP_DIR/int-arithmetic-repl.out"
grep -q 'value main := INT_LITERAL(42)' "$TMP_DIR/int-arithmetic-repl.out"

cat >"$TMP_DIR/nested-int-arithmetic.p" <<'EOF_NESTED_INT_ARITHMETIC'
main := #.int_add (#.int_add #1 #2) #3;
EOF_NESTED_INT_ARITHMETIC
./a.out "$TMP_DIR/nested-int-arithmetic.p" >"$TMP_DIR/nested-int-arithmetic-repl.out"
grep -q 'value main := INT_LITERAL(6)' "$TMP_DIR/nested-int-arithmetic-repl.out"

cat >"$TMP_DIR/int64-arithmetic.p" <<'EOF_INT64_ARITHMETIC'
main := #.int64_add #9223372036854775800 #7;
EOF_INT64_ARITHMETIC
./a.out "$TMP_DIR/int64-arithmetic.p" >"$TMP_DIR/int64-arithmetic-repl.out"
grep -q 'value main := INT_LITERAL(9223372036854775807)' "$TMP_DIR/int64-arithmetic-repl.out"

cat >"$TMP_DIR/text-to-nat-runtime.p" <<'EOF_TEXT_TO_NAT_RUNTIME'
Nat := @{
        zero : *;
        succ : * -> *;
};

main := #.text_to_nat #"2";
EOF_TEXT_TO_NAT_RUNTIME
./a.out "$TMP_DIR/text-to-nat-runtime.p" >"$TMP_DIR/text-to-nat-runtime-repl.out"
grep -q 'value main := APP(CONSTRUCTOR(TYPE_VIEW(#.Nat, core=TYPE_FORMER(#.Nat), source=TYPE_FORMER(#.Nat)).succ)' "$TMP_DIR/text-to-nat-runtime-repl.out"
grep -q 'CONSTRUCTOR(TYPE_VIEW(#.Nat, core=TYPE_FORMER(#.Nat), source=TYPE_FORMER(#.Nat)).zero)' "$TMP_DIR/text-to-nat-runtime-repl.out"

cat >"$TMP_DIR/nat-to-text-runtime.p" <<'EOF_NAT_TO_TEXT_RUNTIME'
Nat := @{
        zero : *;
        succ : * -> *;
};

main := #.nat_to_text (Nat.succ Nat.zero);
EOF_NAT_TO_TEXT_RUNTIME
./a.out "$TMP_DIR/nat-to-text-runtime.p" >"$TMP_DIR/nat-to-text-runtime-repl.out"
grep -q 'value main := TEXT_LITERAL("1")' "$TMP_DIR/nat-to-text-runtime-repl.out"

cat >"$TMP_DIR/terminal-effect.p" <<'EOF_TERMINAL_EFFECT'
main := #.print #"hello";
EOF_TERMINAL_EFFECT
./read_file.out --write-artifact "$TMP_DIR/TerminalEffect.apo" \
	"$TMP_DIR/terminal-effect.p" >"$TMP_DIR/terminal-effect-artifact.out"
./read_file.out --read-graph "$TMP_DIR/TerminalEffect.apo" >"$TMP_DIR/terminal-effect-read-graph.out"
grep -q "^term_node .* $TERM_TAG_EFFECT_LABEL 1$" "$TMP_DIR/TerminalEffect.apo"
grep -q "^term_node .* $TERM_TAG_EFFECT_TYPE " "$TMP_DIR/TerminalEffect.apo"
./read_file.out "$TMP_DIR/terminal-effect.p" >"$TMP_DIR/terminal-effect-read.out"
grep -q 'term main := APP(INTRINSIC(print), TEXT_LITERAL("hello"))' "$TMP_DIR/terminal-effect-read.out"
! grep -q '^hello$' "$TMP_DIR/terminal-effect-read.out"
./a.out "$TMP_DIR/terminal-effect.p" >"$TMP_DIR/terminal-effect-repl.out"
grep -q '^hello$' "$TMP_DIR/terminal-effect-repl.out"
grep -q 'value main := TEXT_LITERAL("hello")' "$TMP_DIR/terminal-effect-repl.out"

awk '
	FNR == NR {
		if ($1 == "term" && $2 == "Bool") {
			bool_term = $3;
		}
		if ($1 == "judgement" && $6 == intrinsic_type_intro_proof_kind) {
			intrinsic_classifier = $5;
		}
		next;
	}
	$1 == "term" && $2 == "Bool" {
		bool_term = $3;
	}
	$1 == "judgement" && $6 == intrinsic_type_intro_proof_kind {
		intrinsic_classifier = $5;
	}
	$1 == "term_node" && $2 == intrinsic_classifier && $3 == pi_tag {
		$4 = bool_term;
	}
	{ print }
' intrinsic_type_intro_proof_kind="$PROOF_KIND_INTRINSIC_TYPE_INTRO" \
	pi_tag="$TERM_TAG_PI" \
	"$TMP_DIR/IntrinsicNatToText.apo" "$TMP_DIR/IntrinsicNatToText.apo" >"$TMP_DIR/BadIntrinsicNatToText.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadIntrinsicNatToText.apo" >"$TMP_DIR/bad-intrinsic-nat-to-text.out" 2>"$TMP_DIR/bad-intrinsic-nat-to-text.err"; then
	echo "bad intrinsic nat_to_text proof artifact unexpectedly passed" >&2
	exit 1
fi

cat >"$TMP_DIR/id-provider.p" <<'EOF_ID_PROVIDER'
Nat := @{
        zero : *;
        succ : * -> *;
};

idNat := \x : Nat => x;
EOF_ID_PROVIDER

cat >"$TMP_DIR/alias-provider.p" <<'EOF_ALIAS_PROVIDER'
Nat := @{
        zero : *;
        succ : * -> *;
};

NatAlias := Nat;
idNat := \x : Nat => x;
EOF_ALIAS_PROVIDER

cat >"$TMP_DIR/duplicate-nat-a.p" <<'EOF_DUPLICATE_NAT_A'
Nat := @{
        zero : *;
        succ : * -> *;
};

idA := \x : Nat => x;
EOF_DUPLICATE_NAT_A

cat >"$TMP_DIR/duplicate-nat-b.p" <<'EOF_DUPLICATE_NAT_B'
Nat := @{
        zero : *;
        succ : * -> *;
};

idB := \y : Nat => y;
EOF_DUPLICATE_NAT_B

cat >"$TMP_DIR/type-alias-key.p" <<'EOF_TYPE_ALIAS_KEY'
Nat := @{
        zero : *;
        succ : * -> *;
};

B := Nat;

BoxNat := @{
        box : Nat -> *;
};

BoxB := @{
        box : B -> *;
};
EOF_TYPE_ALIAS_KEY

cat >"$TMP_DIR/type-alias-key-user.p" <<'EOF_TYPE_ALIAS_KEY_USER'
main := BoxB.box Nat.zero;
main :: BoxNat;
EOF_TYPE_ALIAS_KEY_USER

cat >"$TMP_DIR/bool-two.p" <<'EOF_BOOL_TWO'
Bool := @{
        true : *;
        false : *;
};

Two := @{
        one : *;
        zero : *;
};

main := Two.one;
main :: Bool;
EOF_BOOL_TWO

cat >"$TMP_DIR/list-alias-key.p" <<'EOF_LIST_ALIAS_KEY'
Nat := @{
        zero : *;
        succ : * -> *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

ListB := \B : @ => @{
        nil  : *;
        cons : B -> * -> *;
};
EOF_LIST_ALIAS_KEY

cat >"$TMP_DIR/list-alias-key-user.p" <<'EOF_LIST_ALIAS_KEY_USER'
tail := (List Nat).nil;
main := (ListB Nat).cons Nat.zero tail;
main :: List Nat;
EOF_LIST_ALIAS_KEY_USER

cat >"$TMP_DIR/local-list-alias-key.p" <<'EOF_LOCAL_LIST_ALIAS_KEY'
Nat := @{
        zero : *;
        succ : * -> *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

ListB := \B : @ => @{
        nil  : *;
        cons : B -> * -> *;
};

tail := (List Nat).nil;
main := (ListB Nat).cons Nat.zero tail;
main :: List Nat;
EOF_LOCAL_LIST_ALIAS_KEY

cat >"$TMP_DIR/alias-classifier-user.p" <<'EOF_ALIAS_CLASSIFIER_USER'
idAlias := \x : NatAlias => x;
idAlias :: NatAlias -> NatAlias;
EOF_ALIAS_CLASSIFIER_USER

cat >"$TMP_DIR/id-user.p" <<'EOF_ID_USER'
main := idNat Nat.zero;
EOF_ID_USER

cat >"$TMP_DIR/id-user-declared.p" <<'EOF_ID_USER_DECLARED'
Nat := @{
        zero : *;
        succ : * -> *;
};

idNat : Nat -> Nat;
main := idNat Nat.zero;
main :: Nat;
EOF_ID_USER_DECLARED

cat >"$TMP_DIR/id-user-imported-classifier.p" <<'EOF_ID_USER_IMPORTED_CLASSIFIER'
Nat := @{
        zero : *;
        succ : * -> *;
};

localId := \y : Nat => y;
main := idNat Nat.zero;
main :: Nat;
EOF_ID_USER_IMPORTED_CLASSIFIER

cat >"$TMP_DIR/id-user-alias.p" <<'EOF_ID_USER_ALIAS'
Nat := @{
        zero : *;
        succ : * -> *;
};

main := idNat;
main :: Nat -> Nat;
EOF_ID_USER_ALIAS

./read_file.out --write-artifact "$TMP_DIR/IdProvider.apo" "$TMP_DIR/id-provider.p" >"$TMP_DIR/id-provider.out"
./read_file.out --check-export-normalization-equal "$TMP_DIR/IdProvider.apo" idNat >"$TMP_DIR/id-provider-normalization_equal.out"
grep -q '^export-normalization-equal idNat yes$' "$TMP_DIR/id-provider-normalization_equal.out"
./read_file.out --write-artifact "$TMP_DIR/DuplicateNatA.apo" \
	"$TMP_DIR/duplicate-nat-a.p" >"$TMP_DIR/duplicate-nat-a.out"
./read_file.out --write-artifact "$TMP_DIR/DuplicateNatB.apo" \
	"$TMP_DIR/duplicate-nat-b.p" >"$TMP_DIR/duplicate-nat-b.out"
./read_file.out --aggregate-artifact "$TMP_DIR/DuplicateNatAB.apo" \
	"$TMP_DIR/DuplicateNatA.apo" \
	"$TMP_DIR/DuplicateNatB.apo" >"$TMP_DIR/duplicate-nat-ab.out"
./read_file.out --read-graph "$TMP_DIR/DuplicateNatAB.apo" >"$TMP_DIR/duplicate-nat-ab-read.out"
grep -q 'term_exports=4 type_exports=2 constructor_exports=4 dependencies=0' "$TMP_DIR/duplicate-nat-ab-read.out"
grep -q '^term Nat .* namespace duplicate-nat-a$' "$TMP_DIR/DuplicateNatAB.apo"
grep -q '^term Nat .* namespace duplicate-nat-b$' "$TMP_DIR/DuplicateNatAB.apo"
grep -q '^type Nat .* namespace duplicate-nat-a$' "$TMP_DIR/DuplicateNatAB.apo"
grep -q '^type Nat .* namespace duplicate-nat-b$' "$TMP_DIR/DuplicateNatAB.apo"
duplicate_nat_a_term=$(awk '$1 == "term" && $2 == "idA" { print $3 }' "$TMP_DIR/DuplicateNatAB.apo")
duplicate_nat_b_term=$(awk '$1 == "term" && $2 == "idB" { print $3 }' "$TMP_DIR/DuplicateNatAB.apo")
duplicate_nat_a_classifier=$(awk '$1 == "term" && $2 == "idA" { print $4 }' "$TMP_DIR/DuplicateNatAB.apo")
duplicate_nat_b_classifier=$(awk '$1 == "term" && $2 == "idB" { print $4 }' "$TMP_DIR/DuplicateNatAB.apo")
duplicate_nat_a_key=$(awk '$1 == "term" && $2 == "idA" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/DuplicateNatAB.apo")
duplicate_nat_b_key=$(awk '$1 == "term" && $2 == "idB" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/DuplicateNatAB.apo")
test "$duplicate_nat_a_term" = "$duplicate_nat_b_term"
test "$duplicate_nat_a_classifier" != "$duplicate_nat_b_classifier"
test "$duplicate_nat_a_key" = "$duplicate_nat_b_key"
./read_file.out --write-artifact "$TMP_DIR/TypeAliasKey.apo" \
	"$TMP_DIR/type-alias-key.p" >"$TMP_DIR/type-alias-key.out"
./read_file.out --read-graph "$TMP_DIR/TypeAliasKey.apo" >"$TMP_DIR/type-alias-key-read.out"
grep -q 'term_exports=4 type_exports=3 constructor_exports=4 dependencies=0' "$TMP_DIR/type-alias-key-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0 relocation_resolved_external_type_exprs=0 relocation_external_type_formers=0 relocation_resolved_external_type_formers=0' "$TMP_DIR/type-alias-key-read.out"
nat_term=$(awk '$1 == "term" && $2 == "Nat" { print $3 }' "$TMP_DIR/TypeAliasKey.apo")
b_term=$(awk '$1 == "term" && $2 == "B" { print $3 }' "$TMP_DIR/TypeAliasKey.apo")
box_nat_key=$(awk '$1 == "type" && $2 == "BoxNat" { print $9 ":" $10 ":" $11 ":" $12 ":" $13 ":" $14 ":" $15 ":" $16 }' "$TMP_DIR/TypeAliasKey.apo")
box_b_key=$(awk '$1 == "type" && $2 == "BoxB" { print $9 ":" $10 ":" $11 ":" $12 ":" $13 ":" $14 ":" $15 ":" $16 }' "$TMP_DIR/TypeAliasKey.apo")
box_nat_term=$(awk '$1 == "term" && $2 == "BoxNat" { print $3 }' "$TMP_DIR/TypeAliasKey.apo")
box_b_term=$(awk '$1 == "term" && $2 == "BoxB" { print $3 }' "$TMP_DIR/TypeAliasKey.apo")
box_nat_term_key=$(awk '$1 == "term" && $2 == "BoxNat" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/TypeAliasKey.apo")
box_b_term_key=$(awk '$1 == "term" && $2 == "BoxB" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/TypeAliasKey.apo")
box_nat_term_hash=$(awk '$1 == "term" && $2 == "BoxNat" { print $6 }' "$TMP_DIR/TypeAliasKey.apo")
box_alias_owner_count=$(awk -v owner="owner#$box_nat_term" -v key="key=$box_nat_term_hash" '
	$0 ~ /resolved constructor owner kind=1/ && index($0, owner) && index($0, key) { count++ }
	END { print count + 0 }
' "$TMP_DIR/type-alias-key-read.out")
test "$nat_term" = "$b_term"
test "$box_nat_key" = "$box_b_key"
test "$box_nat_term" != "$box_b_term"
test "$box_nat_term_key" != "$box_b_term_key"
test "$box_alias_owner_count" -eq 0
if ./read_file.out --write-artifact "$TMP_DIR/TypeAliasKeyUser.apo" \
	--import-interface "$TMP_DIR/TypeAliasKey.apo" \
	"$TMP_DIR/type-alias-key-user.p" >"$TMP_DIR/type-alias-key-user.out" \
	2>"$TMP_DIR/type-alias-key-user.err"; then
	echo "distinct BoxNat and BoxB declarations unexpectedly unified" >&2
	exit 1
fi
grep -q 'failed to compile AST graph' "$TMP_DIR/type-alias-key-user.err"
if ./read_file.out --write-artifact "$TMP_DIR/BoolTwo.apo" \
	"$TMP_DIR/bool-two.p" >"$TMP_DIR/bool-two.out" 2>"$TMP_DIR/bool-two.err"; then
	echo "distinct Bool and Two declarations unexpectedly unified" >&2
	exit 1
fi
grep -q 'failed to compile AST graph' "$TMP_DIR/bool-two.err"
./read_file.out --write-artifact "$TMP_DIR/ListAliasKey.apo" \
	"$TMP_DIR/list-alias-key.p" >"$TMP_DIR/list-alias-key.out"
if ./read_file.out --write-artifact "$TMP_DIR/ListAliasKeyUser.apo" \
	--import-interface "$TMP_DIR/ListAliasKey.apo" \
	"$TMP_DIR/list-alias-key-user.p" >"$TMP_DIR/list-alias-key-user.out" \
	2>"$TMP_DIR/list-alias-key-user.err"; then
	echo "distinct List and ListB declarations unexpectedly unified through import" >&2
	exit 1
fi
if ./read_file.out --write-artifact "$TMP_DIR/LocalListAliasKey.apo" \
	"$TMP_DIR/local-list-alias-key.p" >"$TMP_DIR/local-list-alias-key.out" \
	2>"$TMP_DIR/local-list-alias-key.err"; then
	echo "distinct local List and ListB declarations unexpectedly unified" >&2
	exit 1
fi
./read_file.out --write-artifact "$TMP_DIR/AliasProvider.apo" \
	"$TMP_DIR/alias-provider.p" >"$TMP_DIR/alias-provider.out"
./read_file.out --write-artifact "$TMP_DIR/AliasClassifierUser.apo" \
	--import-interface "$TMP_DIR/AliasProvider.apo" \
	"$TMP_DIR/alias-classifier-user.p" >"$TMP_DIR/alias-classifier-user.out"
./read_file.out --link-artifacts "$TMP_DIR/AliasClassifierUser.apo" "$TMP_DIR/AliasProvider.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/AliasClassifierUser.linked.apo" >"$TMP_DIR/alias-classifier-user-link.out"
./read_file.out --check-export-classifiers-compatible \
	"$TMP_DIR/AliasClassifierUser.linked.apo" \
	idNat \
	idAlias >"$TMP_DIR/alias-classifier-compatible.out"
grep -q '^export-classifiers-compatible idNat idAlias yes$' "$TMP_DIR/alias-classifier-compatible.out"
./read_file.out --write-artifact "$TMP_DIR/IdUserDeclared.apo" "$TMP_DIR/id-user-declared.p" >"$TMP_DIR/id-user-declared.out"
grep -q 'has-type EXTERNAL_REF(id-user-declared.idNat) PI' "$TMP_DIR/id-user-declared.out"
grep -q 'has-type APP(EXTERNAL_REF(id-user-declared.idNat)' "$TMP_DIR/id-user-declared.out"
cat >"$TMP_DIR/id-user-declared-only.p" <<'EOF_ID_USER_DECLARED_ONLY'
Nat := @{
        zero : *;
        succ : * -> *;
};

idNat : Nat -> Nat;
EOF_ID_USER_DECLARED_ONLY
./read_file.out --write-artifact "$TMP_DIR/IdUserDeclaredOnly.apo" \
	"$TMP_DIR/id-user-declared-only.p" >"$TMP_DIR/id-user-declared-only.out"
awk '
	FNR == NR {
		if ($1 == "term_node" && $3 == constructor_tag && $5 == 0 && !bad_classifier) {
			bad_classifier = $2;
		}
		next;
	}
	$1 == "judgement" && $6 == declaration_proof_kind && !done {
		$5 = bad_classifier;
		target_proof = $7;
		done = 1;
	}
	$1 == "proof" && $2 == target_proof {
		$6 = bad_classifier;
	}
	{ print }
' constructor_tag="$TERM_TAG_CONSTRUCTOR" \
	declaration_proof_kind="$PROOF_KIND_DECLARATION" \
	"$TMP_DIR/IdUserDeclaredOnly.apo" "$TMP_DIR/IdUserDeclaredOnly.apo" >"$TMP_DIR/BadExternalDeclarationClassifier.apo"
if ./read_file.out --read-graph "$TMP_DIR/BadExternalDeclarationClassifier.apo" >"$TMP_DIR/bad-external-declaration-classifier.out" 2>"$TMP_DIR/bad-external-declaration-classifier.err"; then
	echo "bad external declaration classifier artifact unexpectedly passed" >&2
	exit 1
fi
./read_file.out --write-artifact "$TMP_DIR/IdUser.apo" \
	--import-interface "$TMP_DIR/IdProvider.apo" \
	"$TMP_DIR/id-user.p" >"$TMP_DIR/id-user.out"
grep -q 'metadata external-ref idNat' "$TMP_DIR/id-user.out"
if ./read_file.out --write-artifact "$TMP_DIR/IdUserImportedClassifier.apo" \
	--import-interface "$TMP_DIR/IdProvider.apo" \
	"$TMP_DIR/id-user-imported-classifier.p" >"$TMP_DIR/id-user-imported-classifier.out" \
	2>"$TMP_DIR/id-user-imported-classifier.err"; then
	echo "imported idNat unexpectedly accepted against a distinct local Nat" >&2
	exit 1
fi
if ./read_file.out --write-artifact "$TMP_DIR/IdUserAlias.apo" \
	--import-interface "$TMP_DIR/IdProvider.apo" \
	"$TMP_DIR/id-user-alias.p" >"$TMP_DIR/id-user-alias.out" \
	2>"$TMP_DIR/id-user-alias.err"; then
	echo "imported idNat alias unexpectedly accepted against a distinct local Nat" >&2
	exit 1
fi
./read_file.out --link-artifacts "$TMP_DIR/IdUser.apo" \
	--link-search-dir "$TMP_DIR" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/IdUser.linked.apo" >"$TMP_DIR/id-link.out"
./read_file.out --read-graph "$TMP_DIR/IdUser.linked.apo" >"$TMP_DIR/id-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/id-linked-read.out"
grep -q 'relocation_resolved_constructor_owners=' "$TMP_DIR/id-linked-read.out"
grep -q 'resolved constructor owner kind=1' "$TMP_DIR/id-linked-read.out"
if grep -q 'key=4198605800997046246' "$TMP_DIR/id-linked-read.out"; then
	echo "dead external constructor owner relocation survived" >&2
	exit 1
fi
./read_file.out --write-artifact "$TMP_DIR/IdProviderOpaque.apo" \
	--opaque-export idNat \
	"$TMP_DIR/id-provider.p" >"$TMP_DIR/id-provider-opaque.out"
./read_file.out --check-export-normalization-equal "$TMP_DIR/IdProviderOpaque.apo" idNat >"$TMP_DIR/id-provider-opaque-normalization_equal.out"
grep -q '^export-normalization-equal idNat no$' "$TMP_DIR/id-provider-opaque-normalization_equal.out"
./read_file.out --read-interface "$TMP_DIR/IdProviderOpaque.apo" >"$TMP_DIR/id-provider-opaque-interface.out"
grep -q 'interface term idNat .*transparency=opaque' "$TMP_DIR/id-provider-opaque-interface.out"
./read_file.out --link-artifacts "$TMP_DIR/IdUser.apo" "$TMP_DIR/IdProviderOpaque.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/IdUser.opaque-linked.apo" >"$TMP_DIR/id-opaque-link.out"
./read_file.out --read-graph "$TMP_DIR/IdUser.opaque-linked.apo" >"$TMP_DIR/id-opaque-linked-read.out"
grep -q 'relocation_resolved_external_terms=1' "$TMP_DIR/id-opaque-linked-read.out"
grep -q 'resolved external term term#.*idNat' "$TMP_DIR/id-opaque-linked-read.out"
grep -q "term_node .* $TERM_TAG_EXTERNAL_REF id-provider idNat" "$TMP_DIR/IdUser.opaque-linked.apo"

./read_file.out --write-artifact "$TMP_DIR/AliasProviderOpaque.apo" \
	--opaque-export NatAlias \
	"$TMP_DIR/alias-provider.p" >"$TMP_DIR/alias-provider-opaque.out"
./read_file.out --link-artifacts "$TMP_DIR/AliasClassifierUser.apo" "$TMP_DIR/AliasProviderOpaque.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/AliasClassifierUser.opaque-linked.apo" >"$TMP_DIR/alias-classifier-user-opaque-link.out"
./read_file.out --check-export-classifiers-compatible \
	"$TMP_DIR/AliasClassifierUser.opaque-linked.apo" \
	idNat \
	idAlias >"$TMP_DIR/alias-classifier-opaque-compatible.out"
grep -q '^export-classifiers-compatible idNat idAlias no$' "$TMP_DIR/alias-classifier-opaque-compatible.out"

cat >"$TMP_DIR/id-target-local.p" <<'EOF_ID_TARGET_LOCAL'
Nat := @{
        zero : *;
        succ : * -> *;
};

main := idNat Nat.zero;
localId := \y : Nat => y;
EOF_ID_TARGET_LOCAL

./read_file.out --write-artifact "$TMP_DIR/IdTargetLocal.apo" "$TMP_DIR/id-target-local.p" >"$TMP_DIR/id-target-local.out"
./read_file.out --link-artifacts "$TMP_DIR/IdTargetLocal.apo" \
	"$TMP_DIR/IdProvider.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/IdTargetLocal.linked.apo" >"$TMP_DIR/id-target-local-link.out"
idnat_term=$(awk '$1 == "term" && $2 == "idNat" { print $3 }' "$TMP_DIR/IdTargetLocal.linked.apo")
localid_term=$(awk '$1 == "term" && $2 == "localId" { print $3 }' "$TMP_DIR/IdTargetLocal.linked.apo")
idnat_classifier=$(awk '$1 == "term" && $2 == "idNat" { print $4 }' "$TMP_DIR/IdTargetLocal.linked.apo")
localid_classifier=$(awk '$1 == "term" && $2 == "localId" { print $4 }' "$TMP_DIR/IdTargetLocal.linked.apo")
idnat_key=$(awk '$1 == "term" && $2 == "idNat" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/IdTargetLocal.linked.apo")
localid_key=$(awk '$1 == "term" && $2 == "localId" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/IdTargetLocal.linked.apo")
idnat_classifier_key=$(awk '$1 == "term" && $2 == "idNat" { print $14 ":" $15 ":" $16 ":" $17 ":" $18 ":" $19 ":" $20 ":" $21 }' "$TMP_DIR/IdTargetLocal.linked.apo")
localid_classifier_key=$(awk '$1 == "term" && $2 == "localId" { print $14 ":" $15 ":" $16 ":" $17 ":" $18 ":" $19 ":" $20 ":" $21 }' "$TMP_DIR/IdTargetLocal.linked.apo")
test "$idnat_term" = "$localid_term"
test "$idnat_classifier" != "$localid_classifier"
test "$idnat_key" = "$localid_key"
test "$idnat_classifier_key" != "$localid_classifier_key"
if ./read_file.out --link-artifacts "$TMP_DIR/IdTargetLocal.apo" \
	--link-search-dir "$TMP_DIR" \
	--link-output "$TMP_DIR/IdTargetLocal.ambiguous.apo" \
	>"$TMP_DIR/id-target-local-ambiguous.out" \
	2>"$TMP_DIR/id-target-local-ambiguous.err"; then
	echo "ambiguous bare idNat provider unexpectedly linked" >&2
	exit 1
fi
grep -q 'failed to search provider artifacts' "$TMP_DIR/id-target-local-ambiguous.err"

cat >"$TMP_DIR/nat.p" <<'EOF_NAT'
Nat := @{
        zero : *;
        succ : * -> *;
};
EOF_NAT

cat >"$TMP_DIR/list.p" <<'EOF_LIST'
List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};
EOF_LIST

cat >"$TMP_DIR/box.p" <<'EOF_BOX'
Box := \A : @ => @{
        box : A -> *;
};
EOF_BOX

cat >"$TMP_DIR/wrap.p" <<'EOF_WRAP'
Wrap := \B : @ => @{
        wrap : B -> *;
};
EOF_WRAP

cat >"$TMP_DIR/user.p" <<'EOF_USER'
main := (List Nat).nil;
EOF_USER

cat >"$TMP_DIR/type-expr-user.p" <<'EOF_TYPE_EXPR_USER'
import Nat;

BoxNat := @{
        box : Nat -> *;
};
EOF_TYPE_EXPR_USER

cat >"$TMP_DIR/owner-alias-user.p" <<'EOF_OWNER_ALIAS_USER'
import Nat;
import NatAlias;
import List;

mainAlias := (List NatAlias).nil;
mainNat := (List Nat).nil;
EOF_OWNER_ALIAS_USER

cat >"$TMP_DIR/source-import-user.p" <<'EOF_SOURCE_IMPORT_USER'
import Nat;
import List;

main := (List Nat).nil;
EOF_SOURCE_IMPORT_USER

cat >"$TMP_DIR/source-import-only.p" <<'EOF_SOURCE_IMPORT_ONLY'
import Nat;
EOF_SOURCE_IMPORT_ONLY

cat >"$TMP_DIR/alias-user.p" <<'EOF_ALIAS_USER'
B := Nat;
main := (List B).nil;
main :: List Nat;
EOF_ALIAS_USER

cat >"$TMP_DIR/bad-cons-user.p" <<'EOF_BAD_CONS_USER'
Bool := @{
        true : *;
        false : *;
};

tail := (List Nat).nil;
main := (List Nat).cons Bool.true tail;
main :: List Nat;
EOF_BAD_CONS_USER

cat >"$TMP_DIR/good-cons-user.p" <<'EOF_GOOD_CONS_USER'
tail := (List Nat).nil;
main := (List Nat).cons Nat.zero tail;
main :: List Nat;
EOF_GOOD_CONS_USER

./read_file.out --write-artifact "$TMP_DIR/Nat.apo" "$TMP_DIR/nat.p" >"$TMP_DIR/nat.out"
./read_file.out --write-artifact "$TMP_DIR/List.apo" "$TMP_DIR/list.p" >"$TMP_DIR/list.out"
./read_file.out --write-artifact "$TMP_DIR/Box.apo" "$TMP_DIR/box.p" >"$TMP_DIR/box.out"
./read_file.out --write-artifact "$TMP_DIR/Wrap.apo" "$TMP_DIR/wrap.p" >"$TMP_DIR/wrap.out"
./read_file.out --aggregate-artifact "$TMP_DIR/Prelude.apo" \
	"$TMP_DIR/Nat.apo" \
	"$TMP_DIR/List.apo" >"$TMP_DIR/prelude.out"
./read_file.out --aggregate-artifact "$TMP_DIR/BoxWrap.apo" \
	"$TMP_DIR/Box.apo" \
	"$TMP_DIR/Wrap.apo" >"$TMP_DIR/box-wrap.out"
interface_universe_vars=$(awk '
	$1 == "SECTION" && $2 == "interface" { in_interface = 1; next }
	$1 == "END" && $2 == "interface" { in_interface = 0; next }
	in_interface && $1 == "type_expr" && $3 == "2" { print $4 }
' "$TMP_DIR/BoxWrap.apo" | sort -u | wc -l)
test "$interface_universe_vars" -eq 2
grep -q 'interface constructor type_export#0.cons ordinal=1 fields=2' "$TMP_DIR/list.out"
awk '
	/interface constructor type_export#0\.cons ordinal=1 fields=2 classifier_family=/ {
		split($0, parts, "classifier_family=");
		if (parts[2] == "4294967295" || parts[2] == "") {
			exit 1;
		}
		found = 1;
	}
	END {
		if (!found) {
			exit 1;
		}
	}
' "$TMP_DIR/list.out"
./read_file.out --write-artifact "$TMP_DIR/TypeExprUser.apo" \
	--import-interface "$TMP_DIR/Nat.apo" \
	"$TMP_DIR/type-expr-user.p" >"$TMP_DIR/type-expr-user.out"
./read_file.out --read-graph "$TMP_DIR/TypeExprUser.apo" >"$TMP_DIR/type-expr-user-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0 relocation_resolved_external_type_exprs=1 relocation_external_type_formers=0 relocation_resolved_external_type_formers=1' "$TMP_DIR/type-expr-user-read.out"
grep -q 'resolved external type former type_expr#.*Nat' "$TMP_DIR/type-expr-user-read.out"
grep -q '^dependency Nat namespace -$' "$TMP_DIR/TypeExprUser.apo"
./read_file.out --link-artifacts "$TMP_DIR/TypeExprUser.apo" "$TMP_DIR/Nat.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/TypeExprUser.linked.apo" >"$TMP_DIR/type-expr-user-link.out"
./read_file.out --read-graph "$TMP_DIR/TypeExprUser.linked.apo" >"$TMP_DIR/type-expr-user-linked-read.out"
grep -q 'term_exports=2 type_exports=2 constructor_exports=3 dependencies=0' "$TMP_DIR/type-expr-user-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0 relocation_resolved_external_type_exprs=1 relocation_external_type_formers=0 relocation_resolved_external_type_formers=1' "$TMP_DIR/type-expr-user-linked-read.out"
grep -q 'resolved external type expr type_expr#.*Nat' "$TMP_DIR/type-expr-user-linked-read.out"
grep -q 'resolved external type former type_expr#.*Nat' "$TMP_DIR/type-expr-user-linked-read.out"
./read_file.out --write-artifact "$TMP_DIR/OwnerAliasUser.apo" \
	--import-interface "$TMP_DIR/AliasProvider.apo" \
	--import-interface "$TMP_DIR/List.apo" \
	"$TMP_DIR/owner-alias-user.p" >"$TMP_DIR/owner-alias-user.out"
./read_file.out --link-artifacts "$TMP_DIR/OwnerAliasUser.apo" "$TMP_DIR/AliasProvider.apo" \
	--link-provider "$TMP_DIR/List.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/OwnerAliasUser.linked.apo" >"$TMP_DIR/owner-alias-link.out"
owner_alias_term=$(awk '$1 == "term" && $2 == "mainAlias" { print $3 }' "$TMP_DIR/OwnerAliasUser.linked.apo")
owner_nat_term=$(awk '$1 == "term" && $2 == "mainNat" { print $3 }' "$TMP_DIR/OwnerAliasUser.linked.apo")
owner_alias_key=$(awk '$1 == "term" && $2 == "mainAlias" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/OwnerAliasUser.linked.apo")
owner_nat_key=$(awk '$1 == "term" && $2 == "mainNat" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/OwnerAliasUser.linked.apo")
test "$owner_alias_term" = "$owner_nat_term"
test "$owner_alias_key" = "$owner_nat_key"
./read_file.out --check-export-classifiers-compatible \
	"$TMP_DIR/OwnerAliasUser.linked.apo" \
	mainNat \
	mainAlias >"$TMP_DIR/owner-alias-compatible.out"
grep -q '^export-classifiers-compatible mainNat mainAlias yes$' "$TMP_DIR/owner-alias-compatible.out"
./read_file.out --link-artifacts "$TMP_DIR/OwnerAliasUser.apo" "$TMP_DIR/AliasProviderOpaque.apo" \
	--link-provider "$TMP_DIR/List.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/OwnerAliasUser.opaque-linked.apo" >"$TMP_DIR/owner-alias-opaque-link.out"
owner_alias_opaque_term=$(awk '$1 == "term" && $2 == "mainAlias" { print $3 }' "$TMP_DIR/OwnerAliasUser.opaque-linked.apo")
owner_nat_opaque_term=$(awk '$1 == "term" && $2 == "mainNat" { print $3 }' "$TMP_DIR/OwnerAliasUser.opaque-linked.apo")
owner_alias_opaque_key=$(awk '$1 == "term" && $2 == "mainAlias" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/OwnerAliasUser.opaque-linked.apo")
owner_nat_opaque_key=$(awk '$1 == "term" && $2 == "mainNat" { print $6 ":" $7 ":" $8 ":" $9 ":" $10 ":" $11 ":" $12 ":" $13 }' "$TMP_DIR/OwnerAliasUser.opaque-linked.apo")
test "$owner_alias_opaque_term" != "$owner_nat_opaque_term"
test "$owner_alias_opaque_key" != "$owner_nat_opaque_key"
./read_file.out --check-export-classifiers-compatible \
	"$TMP_DIR/OwnerAliasUser.opaque-linked.apo" \
	mainNat \
	mainAlias >"$TMP_DIR/owner-alias-opaque-compatible.out"
grep -q '^export-classifiers-compatible mainNat mainAlias no$' "$TMP_DIR/owner-alias-opaque-compatible.out"
./read_file.out --write-artifact "$TMP_DIR/User.apo" \
	--import-interface "$TMP_DIR/List.apo" \
	--import-interface "$TMP_DIR/Nat.apo" \
	"$TMP_DIR/user.p" >"$TMP_DIR/user.out"
mkdir "$TMP_DIR/split-imports"
cp "$TMP_DIR/Nat.apo" "$TMP_DIR/split-imports/Nat.apo"
cp "$TMP_DIR/List.apo" "$TMP_DIR/split-imports/List.apo"
./read_file.out --write-artifact "$TMP_DIR/SourceImportOnly.apo" \
	--import-search-dir "$TMP_DIR/split-imports" \
	"$TMP_DIR/source-import-only.p" >"$TMP_DIR/source-import-only.out"
grep -q '^dependency Nat namespace -$' "$TMP_DIR/SourceImportOnly.apo"
./read_file.out --write-artifact "$TMP_DIR/SourceImportUser.apo" \
	--import-search-dir "$TMP_DIR/split-imports" \
	"$TMP_DIR/source-import-user.p" >"$TMP_DIR/source-import-user.out"
grep -q 'metadata external-ref List' "$TMP_DIR/source-import-user.out"
grep -q 'metadata external-ref Nat' "$TMP_DIR/source-import-user.out"
grep -q '^dependency List namespace -$' "$TMP_DIR/SourceImportUser.apo"
grep -q '^dependency Nat namespace -$' "$TMP_DIR/SourceImportUser.apo"
./read_file.out --link-artifacts "$TMP_DIR/SourceImportUser.apo" \
	--link-search-dir "$TMP_DIR/split-imports" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/SourceImportUser.linked.apo" >"$TMP_DIR/source-import-user-link.out"
./read_file.out --read-graph "$TMP_DIR/SourceImportUser.linked.apo" >"$TMP_DIR/source-import-user-linked-read.out"
grep -q 'term_exports=3 type_exports=2 constructor_exports=4 dependencies=0' "$TMP_DIR/source-import-user-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/source-import-user-linked-read.out"
mkdir "$TMP_DIR/prelude-only"
cp "$TMP_DIR/Prelude.apo" "$TMP_DIR/prelude-only/Prelude.apo"
./read_file.out --write-artifact "$TMP_DIR/SourceImportPreludeUser.apo" \
	--import-search-dir "$TMP_DIR/prelude-only" \
	"$TMP_DIR/source-import-user.p" >"$TMP_DIR/source-import-prelude-user.out"
grep -q 'metadata external-ref List' "$TMP_DIR/source-import-prelude-user.out"
grep -q 'metadata external-ref Nat' "$TMP_DIR/source-import-prelude-user.out"
grep -q '^dependency List namespace -$' "$TMP_DIR/SourceImportPreludeUser.apo"
grep -q '^dependency Nat namespace -$' "$TMP_DIR/SourceImportPreludeUser.apo"
./read_file.out --link-artifacts "$TMP_DIR/SourceImportPreludeUser.apo" "$TMP_DIR/Prelude.apo" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/SourceImportPreludeUser.linked.apo" >"$TMP_DIR/source-import-prelude-user-link.out"
./read_file.out --read-graph "$TMP_DIR/SourceImportPreludeUser.linked.apo" >"$TMP_DIR/source-import-prelude-user-linked-read.out"
grep -q 'term_exports=3 type_exports=2 constructor_exports=4 dependencies=0' "$TMP_DIR/source-import-prelude-user-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/source-import-prelude-user-linked-read.out"
./read_file.out --link-artifacts "$TMP_DIR/User.apo" \
	--link-search-dir "$TMP_DIR/split-imports" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/User.linked.apo" >"$TMP_DIR/user-link.out"
./read_file.out --read-graph "$TMP_DIR/User.linked.apo" >"$TMP_DIR/user-linked-read.out"
grep -q 'term_exports=3 type_exports=2 constructor_exports=4 dependencies=0' "$TMP_DIR/user-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/user-linked-read.out"
grep -q 'debug_term_names=3 debug_type_names=2 debug_constructor_names=4' "$TMP_DIR/user-linked-read.out"
grep -q 'resolved constructor owner kind=1 .* ordinal=0' "$TMP_DIR/user-linked-read.out"

./read_file.out --write-artifact "$TMP_DIR/AliasUser.apo" \
	--import-interface "$TMP_DIR/List.apo" \
	--import-interface "$TMP_DIR/Nat.apo" \
	"$TMP_DIR/alias-user.p" >"$TMP_DIR/alias-user.out"
./read_file.out --write-artifact "$TMP_DIR/GoodConsUser.apo" \
	--import-interface "$TMP_DIR/List.apo" \
	--import-interface "$TMP_DIR/Nat.apo" \
	"$TMP_DIR/good-cons-user.p" >"$TMP_DIR/good-cons-user.out"
grep -Eq 'has-type APP\(APP\(CONSTRUCTOR\(rep#[0-9]+\.ordinal#[0-9]+\), CONSTRUCTOR\(rep#[0-9]+\.ordinal#[0-9]+\)\), CONSTRUCTOR\(rep#[0-9]+\.ordinal#[0-9]+\)\) APP\(EXTERNAL_REF\(List\), EXTERNAL_REF\(Nat\)\) \[app-elim\]' "$TMP_DIR/good-cons-user.out"
if ./read_file.out --write-artifact "$TMP_DIR/BadConsUser.apo" \
	--import-interface "$TMP_DIR/List.apo" \
	--import-interface "$TMP_DIR/Nat.apo" \
	"$TMP_DIR/bad-cons-user.p" >"$TMP_DIR/bad-cons-user.out" 2>"$TMP_DIR/bad-cons-user.err"; then
	echo "bad imported constructor application compiled successfully" >&2
	exit 1
fi
./read_file.out --link-artifacts "$TMP_DIR/AliasUser.apo" \
	--link-search-dir "$TMP_DIR/split-imports" \
	--link-reexport-providers \
	--link-output "$TMP_DIR/AliasUser.linked.apo" >"$TMP_DIR/alias-link.out"
./read_file.out --read-graph "$TMP_DIR/AliasUser.linked.apo" >"$TMP_DIR/alias-linked-read.out"
grep -q 'term_exports=4 type_exports=2 constructor_exports=4 dependencies=0' "$TMP_DIR/alias-linked-read.out"
grep -q 'relocation_external_terms=0 .*relocation_external_type_exprs=0' "$TMP_DIR/alias-linked-read.out"
grep -q 'resolved constructor owner kind=1 .* ordinal=0' "$TMP_DIR/alias-linked-read.out"

cc -std=c11 -Wall -Wextra -Werror -I src/prototype \
	src/prototype/core_view_representation_check.c \
	src/prototype/ast.c src/prototype/ast_inspect.c src/prototype/reader.c \
	src/prototype/term.c src/prototype/type_declaration.c src/prototype/typing.c \
	src/prototype/universe.c src/prototype/symbol.c \
	-o "$TMP_DIR/core-view-representation-check"
"$TMP_DIR/core-view-representation-check"

echo "artifact flow tests passed"
