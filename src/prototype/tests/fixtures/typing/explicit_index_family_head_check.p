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

head_or := \A : @ => \default : A => \n : Nat => \xs : Vec A n =>
	xs @nil => default
		@cons k value tail => value;
head_or :: (A : @) -> A -> (n : Nat) -> Vec A n -> A;

sample := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := head_or Nat Nat.zero (Nat.succ Nat.zero) sample;
