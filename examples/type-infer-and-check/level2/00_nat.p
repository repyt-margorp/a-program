(* Level 2: Simple inductive type - Natural numbers *)

Nat := @{
  zero : *;
  succ : * -> *;
};

(* Addition with induction hypothesis *)
add : Nat -> Nat -> Nat := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat => Nat.succ ( *k m));

(* Multiplication with induction hypothesis *)
mul : Nat -> Nat -> Nat := \n : Nat =>
  n @zero => (\m : Nat => Nat.zero)
    @succ k => (\m : Nat => add m ( *k m));

(* Predecessor *)
pred : Nat -> Nat := \n : Nat =>
  n @zero => Nat.zero
    @succ k => k;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;

main := add two three;
