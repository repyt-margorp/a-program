Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	step : (n:Nat) -> * n (Nat.succ n);
};
AccessibleFoo := \A:@ => \R:A->A->@ => @\subject:A => {
	accessible_node : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
rebuild := \n:Nat => \proof:AccessibleFoo Nat LT n =>
	proof @(index self => AccessibleFoo Nat LT index)
	@accessible_node current down =>
		(AccessibleFoo Nat LT).accessible_node current
			&(\y:Nat => \edge:LT y current => *down y edge);
