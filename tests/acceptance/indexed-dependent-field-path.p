// Recover an independent field across a path in an indexed family.
Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(n:Nat)->Nat->* n->* (Nat.succ n);};
Marked := \P:Nat->@ => @\n:Nat => @\xs:Vec n => {
	mark:(n:Nat)->(h:Nat)->(t:Vec n)->P h->* (Nat.succ n) (Vec.cons n h t);
};
extract := \P:Nat->@ => \n:Nat => \h:Nat => \t:Vec n =>
	\p:Marked P (Nat.succ n) (Vec.cons n h t) => p @mark k x xs evidence => evidence;
extract :: (P:Nat->@)->(n:Nat)->(h:Nat)->(t:Vec n)->
	Marked P (Nat.succ n) (Vec.cons n h t)->P h;
constant := \n:Nat => Nat;
one := Nat.succ Nat.zero;
two := Nat.succ one;
main := extract &constant Nat.zero one Vec.nil ((Marked &constant).mark Nat.zero one Vec.nil two);
expected := two;
// The boundary also retains a prefix with a dependent index, not just Nat.
Carrier := @\n:Nat => @\xs:Vec n => {pack:(n:Nat)->(xs:Vec n)->Nat->* n xs;};
MarkedCarrier := \P:Nat->@ => @\n:Nat => @\xs:Vec n => @\v:Carrier n xs => {
	mark:(n:Nat)->(xs:Vec n)->(h:Nat)->P h->* n xs (Carrier.pack n xs h);
};
extract_carrier := \P:Nat->@ => \n:Nat => \xs:Vec n => \h:Nat =>
	\p:MarkedCarrier P n xs (Carrier.pack n xs h) => p @mark k ys x evidence => evidence;
extract_carrier :: (P:Nat->@)->(n:Nat)->(xs:Vec n)->(h:Nat)->
	MarkedCarrier P n xs (Carrier.pack n xs h)->P h;
dependent := extract_carrier &constant Nat.zero Vec.nil one
	((MarkedCarrier &constant).mark Nat.zero Vec.nil one two);
