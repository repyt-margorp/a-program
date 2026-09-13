Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList =>
	xs @nil => Nat.zero
	   @cons head tail => Nat.succ *tail;
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
main := *length input @output => countProof output (property input output @output);
emptyMain := *length NatList.nil @output => countProof output (property NatList.nil output @output);
expected := two;
emptyExpected := Nat.zero;
