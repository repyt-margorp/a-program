Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

add : Nat -> Nat -> Nat := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => Nat.succ ( *k m));

two := Nat.succ (Nat.succ Nat.zero);
three := Nat.succ two;

main := (add two) three;
