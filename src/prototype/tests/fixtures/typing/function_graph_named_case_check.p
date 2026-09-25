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

inspect := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@nil => Nat.zero
		@cons {
			tailLength;
		} => Nat.succ *tailLength;

selectGraph := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@cons {
			tailLength := recursive;
		} => @recursive;

length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons h t => (@length).cons h t (length t) *t;
length_graph :: (xs:NatList)->@length xs (length xs);
package := length_graph one;

directValue := length one;

proof := inspect one (length one) (length_graph one);
