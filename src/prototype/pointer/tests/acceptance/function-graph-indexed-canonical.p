Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->Nat->* k->* (Nat.succ k);
};
length := \n:Nat => \xs:Vec n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
graph := @length;
witness := *length;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := Vec.cons one one (Vec.cons Nat.zero Nat.zero Vec.nil);
main := *length two sample @output => output;
expected := two;
emptyMain := *length Nat.zero Vec.nil @output => output;
emptyExpected := Nat.zero;
