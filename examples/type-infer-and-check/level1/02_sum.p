/* Level 1: Non-recursive data type - Sum (Either) */

Sum := \A : @ => \B : @ => @{
  inl : A -> *;
  inr : B -> *;
};

Bool := @{
  true : *;
  false : *;
};

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Sum eliminator */
sum_elim := \A : @ => \B : @ => \C : @ =>
  \f : (A -> C) => \g : (B -> C) => \s : Sum A B =>
    s @inl a => f a
      @inr b => g b;
sum_elim :: (A : @) -> (B : @) -> (C : @) ->
            (A -> C) -> (B -> C) -> Sum A B -> C;

/* Example: Bool or Nat */
left_example := (Sum Bool Nat).inl Bool.true;
left_example :: Sum Bool Nat;
right_example := (Sum Bool Nat).inr Nat.zero;
right_example :: Sum Bool Nat;

main := left_example;
