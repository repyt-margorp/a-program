Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	under : (n:Nat) -> * n (Nat.succ n);
};
Acc := \A:@ => \R:A->A->@ => @\subject:A => {
	acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
Sized := \A:@ => @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> A -> * k -> * (Nat.succ k);
};
run := \A:@ => \n:Nat => \access:Acc Nat LT n =>
	access @acc current down =>
		(\input:Sized A current =>
			input @nil => Nat.zero
			  @cons k head tail => *down k (LT.under k) tail);
run :: (A:@) -> (n:Nat) -> Acc Nat LT n -> Sized A n -> Nat;
