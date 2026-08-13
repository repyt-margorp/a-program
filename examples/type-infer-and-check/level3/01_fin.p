/* Level 3: Indexed inductive family - Finite sets */
Nat := @{
  zero : *;
  succ : * -> *;
};

/* Fin n represents numbers less than n */
/* Fin 0 is empty, Fin 1 has one element, etc. */
Fin := @\n : Nat => {
  fzero : (k : Nat) -> * (Nat.succ k);
  fsucc : (k : Nat) -> * k -> * (Nat.succ k);
};

/* Examples: */
/* zero_lt_one : Fin (Nat.succ Nat.zero) := */
/*   (Fin (Nat.succ Nat.zero)).fzero; */

/* one_lt_two : Fin (Nat.succ (Nat.succ Nat.zero)) := */
/*   (Fin (Nat.succ (Nat.succ Nat.zero))).fsucc zero_lt_one; */

/* Fin can be used for safe array indexing */
zero_lt_one := Fin.fzero Nat.zero;
zero_lt_one :: Fin (Nat.succ Nat.zero);

one_lt_two := Fin.fsucc (Nat.succ Nat.zero) zero_lt_one;
one_lt_two :: Fin (Nat.succ (Nat.succ Nat.zero));

main := one_lt_two;
