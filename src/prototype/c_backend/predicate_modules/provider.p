Nat := @{zero : *; succ : * -> *;};
Flag := @{true : *; false : *;};
keep := \n : Nat => n @zero => Flag.false @succ prior => Flag.true;
choose := \left : Nat => left
	@zero => (\right : Nat => Flag.true)
	@succ prior => (\right : Nat => keep right);
choose :: Nat -> Nat -> Flag;
