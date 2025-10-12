(* Level 1: Non-recursive data type - Product (Pair) *)

Prod := \A : @ => \B : @ => @{
  pair : A -> B -> *;
};

(* Projections *)
fst : (A : @) -> (B : @) -> Prod A B -> A :=
  \A : @ => \B : @ => \p : Prod A B =>
    p @pair a b => a;

snd : (A : @) -> (B : @) -> Prod A B -> B :=
  \A : @ => \B : @ => \p : Prod A B =>
    p @pair a b => b;

Bool := @{
  true : *;
  false : *;
};

Nat := @{
  zero : *;
  succ : * -> *;
};

(* Example: Bool × Nat *)
example_pair : Prod Bool Nat :=
  (Prod Bool Nat).pair Bool.true Nat.zero;

main := fst Bool Nat example_pair;
