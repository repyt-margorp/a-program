Nat := @{
	zero : *;
	succ : * -> *;
};

Two := @{
	zero : *;
	succ : * -> *;
};

One := @{
	unit : *;
};

copyTwo := \value : Two =>
	value
		@zero => Two.zero
		@succ predecessor => Two.succ *predecessor;

copyTwo :: Two -> Two;

copyOne := \value : One =>
	value @unit => One.unit;

copyOne :: One -> One;

Unary := @\value : Nat => {
	zero : * Nat.zero;
	succ : (predecessor : Nat) -> * predecessor ->
		* (Nat.succ predecessor);
};

constantNatUnary := \input : Two => \output : Two =>
	\graph : @copyTwo input output =>
		graph
			@zero => Unary.zero
			@succ predecessor predecessorOutput predecessorGraph =>
				Unary.zero;

constantOneNatUnary := \input : One => \output : One =>
	\graph : @copyOne input output =>
		graph @unit => Unary.zero;

constantOneNatUnary ::
	(input : One) -> (output : One) ->
	@copyOne input output -> Unary Nat.zero;

one := Two.succ Two.zero;

copy_graph := \n:Two => n @(self => @copyTwo self (copyTwo self))
	@zero => (@copyTwo).zero
	@succ k => (@copyTwo).succ k (copyTwo k) *k;
copy_graph :: (n:Two)->@copyTwo n (copyTwo n);
certified := constantNatUnary one (copyTwo one) (copy_graph one);
certifiedOne := constantOneNatUnary One.unit One.unit (@copyOne).unit;

main := copyTwo one;
expected := { Two.succ Two.zero; };
