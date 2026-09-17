Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:A->* n->* (Nat.succ n);};
copy := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => (Vec A).nil
	@cons head tail => (Vec A).cons head *tail;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one ((Vec Nat).cons Nat.zero (Vec Nat).nil);
main := copy Nat two sample;
expected := sample;
graphMain := *copy Nat two sample @output => output;
