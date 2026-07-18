Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

add := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => {
		result := *k m;
		Nat.succ result
	  });
add :: Nat -> Nat -> Nat;

two := Nat.succ (Nat.succ Nat.zero);
three := Nat.succ two;

main := (add two) three;
