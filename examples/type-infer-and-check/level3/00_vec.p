/* Level 3: Indexed inductive family - Length-indexed vectors */

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Vector indexed by length */
Vec :=
  \A : @ =>
  @\n : Nat =>
  {
    nil  : * Nat.zero;
    cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
  };

/* Head - requires non-empty vector */
/* head : (A : @) -> (n : Nat) -> Vec A (Nat.succ n) -> A := */
/*   \A : @ => \n : Nat => \v : Vec A (Nat.succ n) => */
/*     v @nil => ( impossible! zero ≠ succ n ) */
/*       @cons x xs => x; */

/* Safe example construction */
empty := \A : @ => (Vec A).nil;
empty :: (A : @) -> Vec A Nat.zero;

singleton := \A : @ => \x : A =>
  (Vec A).cons Nat.zero x (empty A);
singleton :: (A : @) -> A -> Vec A (Nat.succ Nat.zero);

main := singleton Nat Nat.zero;
