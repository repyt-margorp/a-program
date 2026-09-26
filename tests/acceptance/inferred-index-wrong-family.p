Nat := @{zero:*; succ:*->*;};
Left := @\n:Nat => {nil:* Nat.zero;};
Right := @\n:Nat => {nil:* Nat.zero;};
D := @\n:Nat => {mk:Left n->* n;};
main := D.mk Right.nil;
