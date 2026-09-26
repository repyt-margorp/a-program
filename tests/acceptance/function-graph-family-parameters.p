Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->A->* k->* (Nat.succ k);};
length := \n:Nat => \xs:Vec Nat n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
length_graph := \n:Nat => \xs:Vec Nat n => xs @(size self => @length size self (length size self))
	@nil => (@length).nil
	@cons k head tail => (@length).cons k head tail (length k tail) *tail;
length_graph :: (n:Nat)->(xs:Vec Nat n)->@length n xs (length n xs);
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one one ((Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil);
main := length two sample;
expected := two;

Diagonal := @\left:Nat => @\right:Nat => {same:(n:Nat)->* n n;};
Related := \A:@ => \R:A->A->@ => @\x:A => {at:(y:A)->R y y->* y;};
read := \n:Nat => \v:Related Nat Diagonal n => v @at y relation => y;
read_graph := \n:Nat => \v:Related Nat Diagonal n => v @(index self => @read index self (read index self))
	@at y relation => (@read).at y relation;
read_graph :: (n:Nat)->(v:Related Nat Diagonal n)->@read n v (read n v);
relatedMain := read two ((Related Nat Diagonal).at two (Diagonal.same two));
