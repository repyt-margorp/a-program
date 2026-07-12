Bool := @{
        true : *;
        false : *;
};

Nat := @{
        zero : *;
        succ : * -> *;
};

List := \A : @ => @{
        nil  : *;
        cons : A -> * -> *;
};

len := \A : @ =>
        ((\lst : List A =>
                (lst
                        @nil => Nat.zero
                        @cons x xs => Nat.succ *xs))
         :: List A -> Nat);

sample_tail2 := ((List Nat).nil) :: List Nat;
sample_tail2 :: List Nat;

sample_tail1 := ((List Nat).cons (Nat.succ (Nat.succ Nat.zero)) sample_tail2) :: List Nat;
sample_tail1 :: List Nat;

sample_tail0 := ((List Nat).cons (Nat.succ Nat.zero) sample_tail1) :: List Nat;
sample_tail0 :: List Nat;

sample := ((List Nat).cons Nat.zero sample_tail0) :: List Nat;
sample :: List Nat;

main := len Nat sample;
