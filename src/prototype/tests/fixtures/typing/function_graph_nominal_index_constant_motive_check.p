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

certified := {
	packet := *copyTwo one;
	packet @returned output graph =>
		constantNatUnary one output graph;
};

certifiedOne := {
	packet := *copyOne One.unit;
	packet @returned output graph =>
		constantOneNatUnary One.unit output graph;
};

main := copyTwo one;
expected := { Two.succ Two.zero; };
