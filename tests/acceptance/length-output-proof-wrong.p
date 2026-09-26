Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
LengthOf := @\xs:NatList => @\n:Nat => {
	nil : * NatList.nil Nat.zero;
	cons : (head:Nat) -> (tail:NatList) -> (k:Nat) -> * tail k ->
		* (NatList.cons head tail) (Nat.succ k);
};
lengthCorrect := \xs:NatList => \n:Nat => \graph:@length xs n => graph
	@nil => LengthOf.nil
	@cons head tail tailLength tailGraph => LengthOf.cons head tail tailLength *tailGraph;
lengthCorrect :: (xs:NatList) -> (n:Nat) -> (@length xs n) -> LengthOf xs Nat.zero;
