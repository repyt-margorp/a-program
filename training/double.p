Nat := @{
	zero : *;
	succ : * -> *;
};

double := \x : Nat =>
	x @zero => Nat.zero
	@succ k => Nat.succ (Nat.succ *k);
