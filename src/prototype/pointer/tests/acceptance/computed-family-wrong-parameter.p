Nat := @{zero:*; succ:*->*;};
Box := \A:@ => \bound:Nat => @{mk:A->*;};
main := (Box Nat {first:=Nat.succ Nat.zero; Nat.succ first;}).mk Nat.zero;
main :: Box Nat Nat.zero;
