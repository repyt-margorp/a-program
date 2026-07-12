/* Level 3: Indexed inductive family - Finite sets */
/* NOTE: This currently fails type checking */

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Fin n represents numbers less than n */
/* Fin 0 is empty, Fin 1 has one element, etc. */
Fin := \n : Nat => @{
  fzero : Fin (Nat.succ n);
  fsucc : Fin n -> Fin (Nat.succ n);
};

/* Examples: */
/* zero_lt_one : Fin (Nat.succ Nat.zero) := */
/*   (Fin (Nat.succ Nat.zero)).fzero; */

/* one_lt_two : Fin (Nat.succ (Nat.succ Nat.zero)) := */
/*   (Fin (Nat.succ (Nat.succ Nat.zero))).fsucc zero_lt_one; */

/* Fin can be used for safe array indexing */
main := Fin;
