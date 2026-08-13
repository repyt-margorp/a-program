Nat := @{
	zero : *;
	succ : * -> *;
};

Vec :=
	\A : @ =>
	@\n : Nat =>
	{
		nil : * Nat.zero;
		cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
	};

tail := \A : @ => \n : Nat => \xs : Vec A (Nat.succ n) =>
	xs @cons k value rest => rest;
tail :: (A : @) -> (n : Nat) -> Vec A (Nat.succ n) -> Vec A n;

sample := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := tail Nat Nat.zero sample;
