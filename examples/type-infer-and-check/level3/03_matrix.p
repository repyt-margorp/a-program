/* Level 3: Multiple indices - Matrix type */
Nat := @{
  zero : *;
  succ : * -> *;
};

/* Vec is needed for row type */
Vec := \A : @ => @\n : Nat => {
  nil  : * Nat.zero;
  cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
};

/* Matrix indexed by rows and columns */
Matrix :=
  \A : @ =>
  @\rows : Nat =>
  @\cols : Nat =>
  {
    empty : * Nat.zero Nat.zero;
    row : (r : Nat) -> (c : Nat) ->
      Vec A c -> * r c -> * (Nat.succ r) c;
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

empty_matrix := \A : @ => (Matrix A).empty;
empty_matrix :: (A : @) -> Matrix A Nat.zero Nat.zero;

one_vec := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
one_vec :: Vec Nat (Nat.succ Nat.zero);

main := one_vec;
