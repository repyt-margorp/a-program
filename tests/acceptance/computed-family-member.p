Nat := @{zero:*; succ:*->*;};
Box := \A:@ => \bound:Nat => @{mk:A->*;};
make := \n:Nat => (Box Nat (Nat.succ n)).mk n;
make :: (n:Nat) -> Box Nat (Nat.succ n);
main := make Nat.zero;
expected := (Box Nat (Nat.succ Nat.zero)).mk Nat.zero;
nested := (Box Nat {first:=Nat.succ Nat.zero; Nat.succ first;}).mk Nat.zero;
nestedExpected := (Box Nat (Nat.succ (Nat.succ Nat.zero))).mk Nat.zero;
