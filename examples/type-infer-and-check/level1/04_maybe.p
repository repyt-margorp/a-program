/* Level 1: Non-recursive data type - Maybe (Option) */

Maybe := \A : @ => @{
  nothing : *;
  just : A -> *;
};

/* Maybe eliminator */
maybe_elim : (A : @) -> (B : @) ->
             B -> (A -> B) -> Maybe A -> B :=
  \A : @ => \B : @ =>
  \default : B => \f : (A -> B) => \m : Maybe A =>
    m @nothing => default
      @just a => f a;

Nat := @{
  zero : *;
  succ : * -> *;
};

/* Maybe is used for partial functions */
none_example : Maybe Nat := (Maybe Nat).nothing;
some_example : Maybe Nat := (Maybe Nat).just Nat.zero;

main := some_example;
