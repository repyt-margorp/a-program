// The accessibleSucc/natAccessible definitions and post-checks from legacy IF8.
// Computing accessibility is not proof that the whole QuickSort fixture works.
Nat := @{
	zero : *;
	succ : * -> *;
};
LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
		lift : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};
Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};
accessibleSucc := \n : Nat => \proof : Acc Nat LT n =>
	proof @acc current down =>
		(Acc Nat LT).acc (Nat.succ current)
			&(\y : Nat => \edge : LT y (Nat.succ current) =>
				edge @step k => proof
					@weakenRight m k prior => down m prior
					@lift m k prior => *down m prior);
accessibleSucc :: (n : Nat) -> Acc Nat LT n -> Acc Nat LT (Nat.succ n);

natAccessible := \n : Nat =>
	n @zero =>
		(Acc Nat LT).acc Nat.zero
				&(\y : Nat => \edge : LT y Nat.zero =>
					edge @step k => Nat.zero
						@weakenRight m k prior => Nat.zero
						@lift m k prior => Nat.zero)
		@succ k => accessibleSucc k *k;
natAccessible :: (n : Nat) -> Acc Nat LT n;

readIndex := \n : Nat => \proof : Acc Nat LT n =>
	proof @acc current down => current;
expected := Nat.succ (Nat.succ Nat.zero);
main := readIndex expected (natAccessible expected);
