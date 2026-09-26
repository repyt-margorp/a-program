Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:A->* n->* (Nat.succ n);};
Tag := @\n:Nat => @\k:Nat => {mark:Vec Nat n -> * n k;};
main := Nat.zero;
