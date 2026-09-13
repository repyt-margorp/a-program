Nat := @{zero:*; succ:*->*;};
Chain := @\n:Nat => {zero:* Nat.zero; succ:(k:Nat)->* k->* (Nat.succ k);};
bad := \n:Nat => \p:Chain n =>
	p @zero => p
		@succ k rest => (\unused:Chain (Nat.succ k) => p) *rest;
