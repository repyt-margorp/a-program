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
		@cons head tail => {
			tailLength := *tail;
			Nat.succ tailLength;
		};

length :: NatList -> Nat;

one := NatList.cons Nat.zero NatList.nil;

main := length one;

expected := {
	Nat.succ Nat.zero;
};

length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons h t => (@length).cons h t (length t) *t;
length_graph :: (xs:NatList)->@length xs (length xs);
certified := length_graph one;
