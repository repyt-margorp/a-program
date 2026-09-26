// A transported dependent field cannot acquire an unrelated tail index.
Nat := @{zero:*; succ:*->*;};
Vec := @\n:Nat => {nil:* Nat.zero; cons:(n:Nat)->Nat->* n->* (Nat.succ n);};
Tag := @\n:Nat => @\xs:Vec n => {tag:(n:Nat)->(xs:Vec n)->Nat->* n xs;};
Marked := @\n:Nat => @\xs:Vec n => {
	mark:(n:Nat)->(h:Nat)->(t:Vec n)->Tag n t->* (Nat.succ n) (Vec.cons n h t);
};
extract := \n:Nat => \h:Nat => \t:Vec n => \unrelated:Vec n =>
	\p:Marked (Nat.succ n) (Vec.cons n h t) => p @mark k x xs evidence => evidence;
extract :: (n:Nat)->(h:Nat)->(t:Vec n)->(unrelated:Vec n)->
	Marked (Nat.succ n) (Vec.cons n h t)->Tag n unrelated;
