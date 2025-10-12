(* Level 3: Indexed inductive family - Propositional equality *)
(* NOTE: This currently fails type checking *)

(* Equality type: Eq A x y is inhabited iff x = y *)
Eq := \A : @ => \x : A => \y : A => @{
  refl : Eq A x x;
};

(* Only reflexivity constructor exists *)
(* This means Eq A x y is only constructible when x = y *)

Nat := @{
  zero : *;
  succ : * -> *;
};

(* Example: proving zero = zero *)
(* zero_eq_zero : Eq Nat Nat.zero Nat.zero := *)
(*   (Eq Nat Nat.zero Nat.zero).refl; *)

(* Cannot prove: zero = succ zero *)
(* zero_eq_one : Eq Nat Nat.zero (Nat.succ Nat.zero) := *)
(*   ??? -- impossible! *)

(* Symmetry of equality *)
(* sym : (A : @) -> (x : A) -> (y : A) -> *)
(*       Eq A x y -> Eq A y x := *)
(*   \A : @ => \x : A => \y : A => \p : Eq A x y => *)
(*     p @refl => (Eq A x x).refl; *)
(*       -- when p is refl, we know x = y, so can construct refl *)

main := Eq;
