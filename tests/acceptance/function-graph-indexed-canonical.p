Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->Nat->* k->* (Nat.succ k);
};
length := \n:Nat => \xs:Vec n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
graph := @length;
length_graph := \n:Nat => \xs:Vec n => xs @(size self => @length size self (length size self))
	@nil => (@length).nil
	@cons k head tail => (@length).cons k head tail (length k tail) *tail;
length_graph :: (n:Nat)->(xs:Vec n)->@length n xs (length n xs);
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := Vec.cons one one (Vec.cons Nat.zero Nat.zero Vec.nil);
main := length two sample;
expected := two;
emptyMain := length Nat.zero Vec.nil;
emptyExpected := Nat.zero;
