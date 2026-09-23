Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	step : (n:Nat) -> * n (Nat.succ n);
	weakenRight : (m:Nat) -> (n:Nat) -> * m n -> * m (Nat.succ n);
};
ltLift := \m:Nat => \n:Nat => \proof:LT m n =>
	proof @step k => LT.step (Nat.succ k)
		@weakenRight lower upper prior =>
			LT.weakenRight (Nat.succ lower) (Nat.succ upper) *prior;
ltLift :: (m:Nat) -> (n:Nat) -> LT m n -> LT (Nat.succ m) (Nat.succ n);
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
main := ltLift zero one (LT.step zero);
expected := LT.step one;
recursiveMain := ltLift zero two (LT.weakenRight zero one (LT.step zero));
recursiveExpected := LT.weakenRight one two (LT.step one);
