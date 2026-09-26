// The tail proof must move together with its constructor's size.
Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(n:Nat)->Nat->* n->* (Nat.succ n);};
Tag := @\n:Nat => @\xs:Vec n => {tag:(n:Nat)->(xs:Vec n)->Nat->* n xs;};
Marked := @\n:Nat => @\xs:Vec n => {
	mark:(n:Nat)->(h:Nat)->(t:Vec n)->Tag n t->* (Nat.succ n) (Vec.cons n h t);
};
extract := \n:Nat => \h:Nat => \t:Vec n =>
	\p:Marked (Nat.succ n) (Vec.cons n h t) => p @mark k x xs evidence => evidence;
extract :: (n:Nat)->(h:Nat)->(t:Vec n)->Marked (Nat.succ n) (Vec.cons n h t)->Tag n t;
observe := \p:Tag Nat.zero Vec.nil => p @tag n xs value => value;
one := Nat.succ Nat.zero;
main := observe (extract Nat.zero Nat.zero Vec.nil (Marked.mark Nat.zero Nat.zero Vec.nil (Tag.tag Nat.zero Vec.nil one)));
expected := one;
