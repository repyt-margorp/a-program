Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->A->* k->* (Nat.succ k);};
length := \n:Nat => \xs:Vec Nat n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one one ((Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil);
main := *length two sample @output => output;
expected := two;

Diagonal := @\left:Nat => @\right:Nat => {same:(n:Nat)->* n n;};
Related := \A:@ => \R:A->A->@ => @\x:A => {at:(y:A)->R y y->* y;};
read := \n:Nat => \v:Related Nat Diagonal n => v @at y relation => y;
relatedMain := *read two ((Related Nat Diagonal).at two (Diagonal.same two)) @output => output;
