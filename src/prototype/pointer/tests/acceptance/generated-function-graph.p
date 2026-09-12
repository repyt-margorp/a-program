Nat := @{zero : *; succ : * -> *;};
NatList := @{nil : *; cons : Nat -> * -> *;};

length := \xs : NatList =>
	xs @nil => Nat.zero
	   @cons head tail => Nat.succ *tail;

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
certifiedMain := {
	packet := *length one;
	packet @returned output graph => graphOutput one output graph;
};
aliasMain := {
	packet := *alias one;
	packet @returned output graph => aliasOutput one output graph;
};
proofMain := {
	packet := *length one;
	packet @returned output graph => graphDepth one output graph;
};
expected := Nat.succ Nat.zero;
