Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};

length := \xs : NatList =>
	xs @nil => Nat.zero
	   @cons head tail => Nat.succ *tail;
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);

graphOutput := \input : NatList => \output : Nat =>
	\graph : @length input output => output;

alias := length;
aliasOutput := \input : NatList => \output : Nat =>
	\graph : @alias input output => graphOutput input output graph;

graphDepth := \input : NatList => \output : Nat =>
	\graph : @length input output => graph
		@cons head tail tailLength tailGraph => Nat.succ *tailGraph
		@nil => Nat.zero;

baseGraph := (@length).nil;
baseMain := graphDepth NatList.nil Nat.zero baseGraph;
baseExpected := Nat.zero;

one := NatList.cons Nat.zero NatList.nil;
main := length one;
certifiedMain := graphOutput one (length one) (length_graph one);
aliasMain := aliasOutput one (alias one) (length_graph one);
proofMain := graphDepth one (length one) (length_graph one);
expected := Nat.succ Nat.zero;
directMain := length one;
directProof := graphDepth one (length one) (length_graph one);
shadowMain := { output := length one; (\output : Nat => output) Nat.zero; };
nestedProof := { output := length one;
	{ output := length NatList.nil; graphDepth NatList.nil (length NatList.nil) (length_graph NatList.nil); };
};
