Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:(k:Nat)->A->* k->* (Nat.succ k);
};
length := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => Nat.zero
	@cons k head tail => Nat.succ *tail;
graph := @length;
length_graph := \A:@ => \n:Nat => \xs:Vec A n =>
	xs @(size self => @length A size self (length A size self))
	@nil => (@length A).nil
	@cons k head tail => (@length A).cons k head tail (length A k tail) *tail;
length_graph :: (A:@)->(n:Nat)->(xs:Vec A n)->@length A n xs (length A n xs);
one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (Vec Nat).cons one one ((Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil);
main := length Nat two sample;
expected := two;
emptyMain := length Nat Nat.zero (Vec Nat).nil;
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
specMain := readProof two (length Nat two sample)
	(lengthCorrect Nat two sample (length Nat two sample) (length_graph Nat two sample));
emptySpecMain := readProof Nat.zero (length Nat Nat.zero (Vec Nat).nil)
	(lengthCorrect Nat Nat.zero (Vec Nat).nil (length Nat Nat.zero (Vec Nat).nil)
		(length_graph Nat Nat.zero (Vec Nat).nil));

copy := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => (Vec A).nil
	@cons k head tail => (Vec A).cons k head *tail;
copyGraph := @copy;
copyMain := copy Nat two sample;

countFrom := \A:@ => \n:Nat => \xs:Vec A n => xs
	@nil => (\start:Nat => start)
	@cons k head tail => (\start:Nat => *tail (Nat.succ start));
countGraph := @countFrom;
countMain := countFrom Nat two sample Nat.zero;

Point := @\A:@ => @\x:A => {at:(B:@)->(y:B)->* B y;};
constant := \A:@ => \x:A => \p:Point A x => p @at B y => Nat.succ Nat.zero;
pointGraph := @constant;
pointMain := constant Nat two (Point.at Nat two);
