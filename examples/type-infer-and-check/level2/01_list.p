(* Level 2: Simple inductive type - Lists *)

Nat := @{
  zero : *;
  succ : * -> *;
};

List := \A : @ => @{
  nil : *;
  cons : A -> * -> *;
};

(* Length with induction hypothesis *)
length : (A : @) -> List A -> Nat := \A : @ => \lst : List A =>
  lst @nil => Nat.zero
      @cons x xs => Nat.succ ( *xs);

(* Append with induction hypothesis *)
append : (A : @) -> List A -> List A -> List A :=
  \A : @ => \xs : List A => \ys : List A =>
    xs @nil => ys
       @cons x rest => (List A).cons x ( *rest ys);

(* Map with induction hypothesis *)
map : (A : @) -> (B : @) -> (A -> B) -> List A -> List B :=
  \A : @ => \B : @ => \f : (A -> B) => \lst : List A =>
    lst @nil => (List B).nil
        @cons x xs => (List B).cons (f x) ( *xs);

sample := (List Nat).cons Nat.zero (List Nat).nil;

main := length Nat sample;
