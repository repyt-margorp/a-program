// Legacy Main 63b00eb accepts access : Acc Nat LT zero, but observed equals
// wrong (one), not expected (zero). The fixed-right-index LT IH is invalid.
Nat := @{ zero : *; succ : * -> *; };
LT := @\left : Nat => @\right : Nat => {
	step : (n : Nat) -> * n (Nat.succ n);
	weakenRight : (m : Nat) -> (n : Nat) -> * m n -> * m (Nat.succ n);
};
Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

natAccessible := \n : Nat =>
	n @zero => (Acc Nat LT).acc Nat.zero
		&(\y : Nat => \edge : LT y Nat.zero =>
			edge @step k => Nat.zero
			     @weakenRight m k prior => Nat.zero)
	  @succ k => (Acc Nat LT).acc (Nat.succ k)
		&(\y : Nat => \edge : LT y (Nat.succ k) =>
			edge @step n => *k
			     @weakenRight m n prior => *prior);
natAccessible :: (n : Nat) -> Acc Nat LT n;

one := Nat.succ Nat.zero;
two := Nat.succ one;
lower := LT.weakenRight Nat.zero one (LT.step Nat.zero);
descend := \n : Nat => \p : Acc Nat LT n => \m : Nat => \edge : LT m n =>
	p @acc x down => down m edge;
access := descend two (natAccessible two) Nat.zero lower;
access :: Acc Nat LT Nat.zero;
index := \n : Nat => \p : Acc Nat LT n => p @acc x down => x;
observed := index Nat.zero access;
expected := { Nat.zero; };
wrong := { one; };
