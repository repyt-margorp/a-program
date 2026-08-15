Nat := @{
	zero : *;
	succ : * -> *;
};

LE :=
	@\left : Nat =>
	@\right : Nat =>
	{
		zeroLe : (n : Nat) -> * Nat.zero n;
		succLe : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};

LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
	};

Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

leRefl := \n : Nat =>
	n @zero => LE.zeroLe Nat.zero
		@succ k => LE.succLe k k *k;

leRefl :: (n : Nat) -> LE n n;

natAccessible := \n : Nat =>
	n @zero =>
		(Acc Nat LT).acc Nat.zero
			&(\y : Nat => \edge : LT y Nat.zero =>
				edge @step k => Nat.zero
					@weakenRight m k prior => Nat.zero)
		@succ k =>
			(Acc Nat LT).acc (Nat.succ k)
				&(\y : Nat => \edge : LT y (Nat.succ k) =>
					edge @step n => *k
						@weakenRight m n prior => *prior);

natAccessible :: (n : Nat) -> Acc Nat LT n;

one := Nat.succ Nat.zero;
two := Nat.succ one;
main := natAccessible two;
