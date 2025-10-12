Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

pred := \n : Nat =>
	n @zero => Nat.zero
	  @succ k => k;

main := pred (Nat.succ Nat.zero);
