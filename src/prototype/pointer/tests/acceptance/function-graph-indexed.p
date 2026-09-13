Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->* (Nat.succ k);
};
length := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
graph := @length;
witness := *length;
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one one ((Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil);
main := *length Nat two sample @output => output;
expected := two;
emptyMain := *length Nat Nat.zero (Vec Nat).nil @output => output;
emptyExpected := Nat.zero;

SameNat := @\left:Nat => @\right:Nat => {
	zero:* Nat.zero Nat.zero;
	succ:(left:Nat)->(right:Nat)->* left right->* (Nat.succ left) (Nat.succ right);
};
lengthCorrect := \A:@ => \n:Nat => \xs:Vec A n => \out:Nat => \g:@length A n xs out => g
	@nil => SameNat.zero
	@cons k head tail tailLength tailGraph => SameNat.succ k tailLength *tailGraph;
lengthCorrect :: (A:@)->(n:Nat)->(xs:Vec A n)->(out:Nat)->(@length A n xs out)->SameNat n out;
readProof := \n:Nat => \out:Nat => \p:SameNat n out => p
	@zero => Nat.zero
	@succ left right rest => Nat.succ *rest;
specMain := *length Nat two sample @output =>
	readProof two output (lengthCorrect Nat two sample output @output);
emptySpecMain := *length Nat Nat.zero (Vec Nat).nil @output =>
	readProof Nat.zero output (lengthCorrect Nat Nat.zero (Vec Nat).nil output @output);

copy := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => (Vec A).nil
	@cons k head tail => (Vec A).cons k head *tail;
copyMain := *copy Nat two sample @output => output;

countFrom := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => (\start:Nat => start)
	@cons k head tail => (\start:Nat => *tail (Nat.succ start));
countMain := *countFrom Nat two sample Nat.zero @output => output;

Point := @\A:@ => @\x:A => {at:(B:@)->(y:B)->* B y;};
constant := \A:@ => \x:A => \p:Point A x => p @at B y => Nat.succ Nat.zero;
pointMain := *constant Nat two (Point.at Nat two) @output => output;
