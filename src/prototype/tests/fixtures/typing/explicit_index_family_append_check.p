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

add := \left : Nat =>
	left @zero => (\right : Nat => right)
	     @succ predecessor =>
		(\right : Nat => Nat.succ (*predecessor right));

append := \A : @ => \m : Nat => \xs : Vec A m =>
	xs @nil => (\n : Nat => \ys : Vec A n => ys)
	   @cons k value rest =>
		(\n : Nat => \ys : Vec A n =>
			(Vec A).cons (add k n) value (*rest n ys));

append :: (A : @) -> (m : Nat) -> Vec A m ->
	(n : Nat) -> Vec A n -> Vec A (add m n);

left := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
right := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
main := append Nat (Nat.succ Nat.zero) left
	(Nat.succ Nat.zero) right;
