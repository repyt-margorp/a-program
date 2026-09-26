Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

length := \xs : NatList =>
	xs
		@nil => Nat.zero
		@cons head tail => Nat.succ *tail;

length :: NatList -> Nat;

Unary := @\value : Nat => {
	zero : * Nat.zero;
	succ : (predecessor : Nat) -> * predecessor ->
		* (Nat.succ predecessor);
};

lengthOutputUnary := \input : NatList => \output : Nat =>
	\graph : @length input output =>
		graph
			@nil => Unary.zero
			@cons head tail tailLength tailGraph =>
				Unary.succ tailLength *tailGraph;

one := NatList.cons Nat.zero NatList.nil;

length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons h t => (@length).cons h t (length t) *t;
length_graph :: (xs:NatList)->@length xs (length xs);
certified := lengthOutputUnary one (length one) (length_graph one);

main := length one;
expected := { Nat.succ Nat.zero; };
