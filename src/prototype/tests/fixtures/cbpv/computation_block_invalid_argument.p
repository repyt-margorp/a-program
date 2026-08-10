Nat := @{
	zero : *;
	succ : * -> *;
};

identity := \n : Nat => n;

apply := \f : Nat -> Nat => f Nat.zero;

main := apply identity;
