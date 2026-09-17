Nat := @{ zero : *; succ : * -> *; };
f := \x : Nat => x @zero => Nat.zero
	@succ n => (n @zero => Nat.zero @succ k => Nat.succ k);
// Neither zero leaf may silently win the alias.
wrong := (@f).zero;
