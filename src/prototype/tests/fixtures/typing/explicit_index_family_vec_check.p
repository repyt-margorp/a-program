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

empty := (Vec Nat).nil;
empty :: Vec Nat Nat.zero;

single := (Vec Nat).cons Nat.zero Nat.zero empty;
single :: Vec Nat (Nat.succ Nat.zero);
