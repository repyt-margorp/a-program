Nat := @{ zero : *; succ : * -> *; };
f := \x : Nat => x @zero => Nat.zero
	@succ n => (n @zero => Nat.zero @succ k => Nat.succ k);
wrong := (@f).case1;
wrong :: @f Nat.zero Nat.zero;
