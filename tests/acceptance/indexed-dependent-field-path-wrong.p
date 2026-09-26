// Equal schema shape does not justify changing the selected field index.
Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(n:Nat)->Nat->* n->* (Nat.succ n);};
Marked := \P:Nat->@ => @\n:Nat => @\xs:Vec n => {
	mark:(n:Nat)->(h:Nat)->(t:Vec n)->P h->* (Nat.succ n) (Vec.cons n h t);
};
extract := \P:Nat->@ => \n:Nat => \h:Nat => \t:Vec n =>
	\p:Marked P (Nat.succ n) (Vec.cons n h t) => p @mark k x xs evidence => evidence;
constant := \n:Nat => Nat;
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := extract &constant Nat.zero two Vec.nil ((Marked &constant).mark Nat.zero one Vec.nil two);
