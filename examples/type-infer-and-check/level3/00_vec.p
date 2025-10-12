(* Level 3: Indexed inductive family - Length-indexed vectors *)
(* NOTE: This currently fails type checking - syntax is accepted but *)
(* the type checker doesn't handle index refinement *)

Nat := @{
  zero : *;
  succ : * -> *;
};

(* Vector indexed by length *)
Vec := \A : @ => \n : Nat => @{
  nil  : Vec A Nat.zero;
  cons : A -> Vec A n -> Vec A (Nat.succ n);
};

(* Head - requires non-empty vector *)
(* head : (A : @) -> (n : Nat) -> Vec A (Nat.succ n) -> A := *)
(*   \A : @ => \n : Nat => \v : Vec A (Nat.succ n) => *)
(*     v @nil => (* impossible! zero ≠ succ n *) *)
(*       @cons x xs => x; *)

(* Safe example construction *)
empty : (A : @) -> Vec A Nat.zero :=
  \A : @ => (Vec A Nat.zero).nil;

singleton : (A : @) -> A -> Vec A (Nat.succ Nat.zero) :=
  \A : @ => \x : A =>
    (Vec A (Nat.succ Nat.zero)).cons x (empty A);

main := Vec;
