Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:A->* n->* (Nat.succ n);};
D := @\n:Nat => {mk:Vec Nat (Nat.succ n)->Vec Nat n->* n;};
nil := (Vec Nat).nil;
one := (Vec Nat).cons Nat.zero nil;
partial := D.mk one;
// A separately named partial call does not inherit its caller's argument.
main := partial nil;
