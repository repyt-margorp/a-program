Nat := @{
	zero : *;
	succ : * -> *;
};

double := \n : Nat =>
	n @zero => Nat.zero
	  @succ k => Nat.succ (Nat.succ *k);

add := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => Nat.succ (*k m));

one := Nat.succ Nat.zero;
main := add one one;
