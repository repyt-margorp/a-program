Nat := @{zero:*; succ:*->*;};
Bad := @\n:Nat => {step:* (Nat.succ n)->* n;};
main := Nat.zero;
