/* Level 1: Non-recursive data type - Product (Pair) */

Prod := \A : @ => \B : @ => @{
  pair : A -> B -> *;
};

/* Projections */
fst := \A : @ => \B : @ => \p : Prod A B =>
    p @pair a b => a;
fst :: (A : @) -> (B : @) -> Prod A B -> A;

snd := \A : @ => \B : @ => \p : Prod A B =>
    p @pair a b => b;
snd :: (A : @) -> (B : @) -> Prod A B -> B;

Bool := @{
  true : *;
  false : *;
};

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Example: Bool × Nat */
example_pair := (Prod Bool Nat).pair Bool.true Nat.zero;
example_pair :: Prod Bool Nat;

main := fst Bool Nat example_pair;
