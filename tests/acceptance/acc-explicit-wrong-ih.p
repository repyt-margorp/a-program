Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	step : (n:Nat) -> * n (Nat.succ n);
};
AccessibleFoo := \A:@ => \R:A->A->@ => @\subject:A => {
	accessible_node : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
bad := \proof:AccessibleFoo Nat LT Nat.zero =>
	proof @(index self => AccessibleFoo Nat LT index)
	@accessible_node current down => *down current (LT.step current);
