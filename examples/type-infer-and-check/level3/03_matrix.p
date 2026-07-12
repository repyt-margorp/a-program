/* Level 3: Multiple indices - Matrix type */
/* NOTE: This currently fails type checking */

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Matrix indexed by rows and columns */
Matrix := \A : @ => \rows : Nat => \cols : Nat => @{
  empty : Matrix A Nat.zero Nat.zero;
  row : Vec A cols -> Matrix A rows cols -> Matrix A (Nat.succ rows) cols;
};

/* Vec is needed for row type */
Vec := \A : @ => \n : Nat => @{
  nil  : Vec A Nat.zero;
  cons : A -> Vec A n -> Vec A (Nat.succ n);
};

/* Examples: */
/* empty_matrix : (A : @) -> Matrix A Nat.zero Nat.zero := */
/*   \A : @ => (Matrix A Nat.zero Nat.zero).empty; */

/* 1x1 matrix: */
/* one_by_one : (A : @) -> A -> Matrix A (Nat.succ Nat.zero) (Nat.succ Nat.zero) := */
/*   \A : @ => \x : A => */
/*     let vec = (Vec A (Nat.succ Nat.zero)).cons x (Vec A Nat.zero).nil in */
/*     (Matrix A (Nat.succ Nat.zero) (Nat.succ Nat.zero)).row */
/*       vec */
/*       (empty_matrix A); */

main := Matrix;
