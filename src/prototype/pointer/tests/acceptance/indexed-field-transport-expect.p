Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
One := @\n:Nat => {at:(k:Nat)->* (Nat.succ k);};
bad := \n:Nat => \v:One (Nat.succ n) =>
	v @at k => (Witness.at k :: Witness n);
