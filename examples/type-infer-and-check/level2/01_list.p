/* Level 2: Simple inductive type - Lists */

Nat := @{
  zero : *;
  succ : * -> *;
};

List := \A : @ => @{
  nil : *;
  cons : A -> * -> *;
};

/* Length with induction hypothesis */
length := \A : @ => \lst : List A =>
  lst @nil => Nat.zero
      @cons x xs => Nat.succ (*xs);
length :: (A : @) -> List A -> Nat;

/* Append with induction hypothesis */
append := \A : @ => \xs : List A =>
    xs @nil => (\ys : List A => ys)
       @cons x rest => (\ys : List A => (List A).cons x (*rest ys));
append :: (A : @) -> List A -> List A -> List A;

/* Map with induction hypothesis */
map := \A : @ => \B : @ => \f : (A -> B) => \lst : List A =>
    lst @nil => (List B).nil
        @cons x xs => (List B).cons (f x) (*xs);
map :: (A : @) -> (B : @) -> (A -> B) -> List A -> List B;

sample := (List Nat).cons Nat.zero (List Nat).nil;

main := length Nat sample;
