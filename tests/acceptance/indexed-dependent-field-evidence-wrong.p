// A constructor field is not evidence for an arbitrary predicate of it.
Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(n:Nat)->Nat->* n->* (Nat.succ n);};
Marked := \P:Nat->@ => @\n:Nat => @\xs:Vec n => {
	mark:(n:Nat)->(h:Nat)->(t:Vec n)->P h->* (Nat.succ n) (Vec.cons n h t);
};
extract := \P:Nat->@ => \n:Nat => \h:Nat => \t:Vec n =>
	\p:Marked P (Nat.succ n) (Vec.cons n h t) => p @mark k x xs evidence => x;
extract :: (P:Nat->@)->(n:Nat)->(h:Nat)->(t:Vec n)->
	Marked P (Nat.succ n) (Vec.cons n h t)->P h;
