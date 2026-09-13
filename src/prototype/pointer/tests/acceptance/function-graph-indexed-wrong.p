Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->Nat->* k->* (Nat.succ k);};
length := \n:Nat => \xs:Vec n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
Graph := @length;
one := Nat.succ Nat.zero;
tail := Vec.cons Nat.zero Nat.zero Vec.nil;
// The tail is in Vec one; a nil graph certifies a different fiber and input.
wrong := Graph.cons one Nat.zero tail Nat.zero Graph.nil;
