Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
One := @\n:Nat => {at:(k:Nat)->* (Nat.succ k);};
bad := \n:Nat => \m:Nat => \f:Witness n -> Nat => \v:One (Nat.succ m) =>
	v @at k => f (Witness.at k);
