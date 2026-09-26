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

map := \A : @ => \B : @ => \transform : A -> B =>
	\n : Nat => \xs : Vec A n =>
		xs @nil => (Vec B).nil
		   @cons k value rest =>
			(Vec B).cons k (transform value) *rest;
map :: (A : @) -> (B : @) -> (A -> B) ->
	(n : Nat) -> Vec A n -> Vec B n;

successor := \value : Nat => Nat.succ value;
sample := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := map Nat Nat &successor (Nat.succ Nat.zero) sample;
