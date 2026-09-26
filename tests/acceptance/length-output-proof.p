Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList =>
	xs @nil => Nat.zero
	   @cons head tail => Nat.succ *tail;
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);
Unary := @\n:Nat => {
	zero : * Nat.zero;
	succ : (k:Nat) -> * k -> * (Nat.succ k);
};
property := \input:NatList => \output:Nat => \graph:@length input output =>
	graph @cons head tail tailLength tailGraph => Unary.succ tailLength *tailGraph
	      @nil => Unary.zero;
countProof := \n:Nat => \proof:Unary n =>
	proof @zero => Nat.zero
	      @succ k rest => Nat.succ *rest;
two := Nat.succ (Nat.succ Nat.zero);
input := NatList.cons two (NatList.cons Nat.zero NatList.nil);
main := countProof (length input) (property input (length input) (length_graph input));
emptyMain := countProof (length NatList.nil) (property NatList.nil (length NatList.nil) (length_graph NatList.nil));
expected := two;
emptyExpected := Nat.zero;

LengthOf := @\xs:NatList => @\n:Nat => {
	nil : * NatList.nil Nat.zero;
	cons : (head:Nat) -> (tail:NatList) -> (k:Nat) -> * tail k ->
		* (NatList.cons head tail) (Nat.succ k);
};
lengthCorrect := \xs:NatList => \n:Nat => \graph:@length xs n => graph
	@nil => LengthOf.nil
	@cons head tail tailLength tailGraph => LengthOf.cons head tail tailLength *tailGraph;
lengthCorrect :: (xs:NatList) -> (n:Nat) -> (@length xs n) -> LengthOf xs n;
readSpec := \xs:NatList => \n:Nat => \proof:LengthOf xs n => proof
	@nil => Nat.zero
	@cons head tail k rest => Nat.succ *rest;
specMain := readSpec input (length input) (lengthCorrect input (length input) (length_graph input));
emptySpecMain := readSpec NatList.nil (length NatList.nil)
	(lengthCorrect NatList.nil (length NatList.nil) (length_graph NatList.nil));
