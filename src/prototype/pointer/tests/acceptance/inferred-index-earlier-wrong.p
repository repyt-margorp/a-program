Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:A->* n->* (Nat.succ n);};
D := @\n:Nat => {mk:Vec Nat (Nat.succ n)->Vec Nat n->* n;};
nil := (Vec Nat).nil;
main := D.mk nil nil;
