Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
Chain := @\n:Nat => {zero:* Nat.zero; succ:(k:Nat)->* k->* (Nat.succ k);};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
next := \k:Nat => \decision:Bool => \proof:Witness k => Witness.at (Nat.succ k);
make := \n:Nat => \chain:Chain n =>
	chain @zero => Witness.at Nat.zero
		@succ k rest => {
			decision := Bool.true;
			next k decision *rest;
			*rest :: Witness (Nat.succ k);
		};
