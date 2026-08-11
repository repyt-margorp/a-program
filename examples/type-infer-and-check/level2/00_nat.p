/* Level 2: Simple inductive type - Natural numbers */

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Addition with induction hypothesis */
add := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat => Nat.succ (*k m));
add :: Nat -> Nat -> Nat;

/* Multiplication with induction hypothesis */
mul := \n : Nat =>
  n @zero => (\m : Nat => Nat.zero)
    @succ k => (\m : Nat => add m (*k m));
mul :: Nat -> Nat -> Nat;

/* Predecessor */
pred := \n : Nat =>
  n @zero => Nat.zero
    @succ k => k;
pred :: Nat -> Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;

main := add two three;
