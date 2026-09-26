Nat := @{ zero : *; succ : * -> *; };
Vec := \A : @ => @\n : Nat => {
	nil : * Nat.zero;
	cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
};

// The indexed motive generalizes the ambient start argument.
count := \A : @ => \n : Nat => \xs : Vec A n => \start : Nat => xs
	@nil => start
	@cons k head tail => Nat.succ (*tail start);

// Ordinary Match uses the same ambient generalization without an IH.
select := \A : @ => \n : Nat => \xs : Vec A n => \start : Nat => xs
	@nil => start
	@cons k head tail => start;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
sample := (Vec Nat).cons one one ((Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil);
main := count Nat two sample one;
emptyMain := count Nat Nat.zero (Vec Nat).nil one;
selected := select Nat two sample one;
