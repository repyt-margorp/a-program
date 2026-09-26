Nat := @{zero:*; succ:*->*;};
Witness := @\n:Nat => {at:(k:Nat)->* k;};
Indexed := @\n:Nat => {
	zero : * Nat.zero;
	succ : (k:Nat) -> * (Nat.succ k);
};
use := \n:Nat => \f:Witness n -> Nat => \v:Indexed n =>
	v @zero => f (Witness.at (Nat.succ Nat.zero))
	  @succ k => f (Witness.at (Nat.succ k));
