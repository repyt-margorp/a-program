/* Level 1: Non-recursive data type - Maybe (Option) */

Maybe := \A : @ => @{
  nothing : *;
  just : A -> *;
};

/* Maybe eliminator */
maybe_elim := \A : @ => \B : @ =>
  \default : B => \f : (A -> B) => \m : Maybe A =>
    m @nothing => default
      @just a => f a;
maybe_elim :: (A : @) -> (B : @) -> B -> (A -> B) -> Maybe A -> B;

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Maybe is used for partial functions */
none_example := (Maybe Nat).nothing;
none_example :: Maybe Nat;
some_example := (Maybe Nat).just Nat.zero;
some_example :: Maybe Nat;

main := some_example;
