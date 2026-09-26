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
copy_graph := (@copy Nat).cons one one ((Vec Nat).cons Nat.zero (Vec Nat).nil)
	((Vec Nat).cons Nat.zero (Vec Nat).nil)
	((@copy Nat).cons Nat.zero Nat.zero (Vec Nat).nil (Vec Nat).nil (@copy Nat).nil);
copy_graph :: @copy Nat two sample (copy Nat two sample);
read_graph := \n:Nat => \xs:Vec Nat n => \ys:Vec Nat n => \proof:@copy Nat n xs ys => ys;
graphMain := read_graph two sample (copy Nat two sample) copy_graph;
