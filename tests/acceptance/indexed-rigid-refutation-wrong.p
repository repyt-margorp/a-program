Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> Nat -> * k -> * (Nat.succ k);
};
bad := \n:Nat => \v:Vec (Nat.succ n) =>
	v @nil => Nat.zero
	  @cons k x rest => Nat.succ Bool.true;
