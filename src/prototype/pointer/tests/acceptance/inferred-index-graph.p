Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
length := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one ((Vec Nat).cons Nat.zero (Vec Nat).nil);
SameNat := @\left:Nat => @\right:Nat => {
	zero:* Nat.zero Nat.zero;
	succ:(a:Nat)->(b:Nat)->* a b->* (Nat.succ a) (Nat.succ b);
};
// Generated graph fields stay explicit even when source Match hides an index.
lengthCorrect := \A:@ => \n:Nat => \xs:Vec A n => \out:Nat => \g:@length A n xs out => g
	@nil => SameNat.zero
	@cons k head tail tailLength tailGraph => SameNat.succ k tailLength *tailGraph;
lengthCorrect :: (A:@)->(n:Nat)->(xs:Vec A n)->(out:Nat)->(@length A n xs out)->SameNat n out;
readProof := \n:Nat => \out:Nat => \p:SameNat n out => p
	@zero => Nat.zero
	@succ left right rest => Nat.succ *rest;
main := *length Nat two sample @output =>
	readProof two output (lengthCorrect Nat two sample output @output);
expected := two;
