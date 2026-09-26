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

head := \A : @ => \n : Nat => \xs : Vec A (Nat.succ n) =>
	xs @cons k value tail => value;
head :: (A : @) -> (n : Nat) -> Vec A (Nat.succ n) -> A;

sample := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := head Nat Nat.zero sample;
