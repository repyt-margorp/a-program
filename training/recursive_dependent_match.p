Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

BadNat := @{
	zero0 : *;
	zero1 : *;
	succ : * -> *;
};

fold := \n : BadNat =>
	n @zero0 => Bool.true
	  @zero1 => Nat.zero
	  @succ k => *k;
