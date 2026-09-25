Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
length_graph := \xs:List => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:List)->@length xs (length xs);
Size := @\xs:List => @\n:Nat => {
	nil:* List.nil Nat.zero;
	cons:(head:Nat)->(tail:List)->(n:Nat)->* tail n->* (List.cons head tail) (Nat.succ n);
};
lengthCorrect := \xs:List => \n:Nat => \g:@length xs n => g
	@nil => Size.nil
	@cons head tail n rest => Size.cons head tail n *rest;
TailSize := @\xs:List => @\n:Nat => {
	nil:* List.nil Nat.zero;
	cons:(head:Nat)->(tail:List)->(n:Nat)->Size tail n->* (List.cons head tail) n;
};
tailLength := \xs:List => xs @nil => Nat.zero @cons head tail => length tail;
tail_graph := \xs:List => xs @(self => @tailLength self (tailLength self))
	@nil => (@tailLength).nil
	@cons head tail => (@tailLength).cons head tail (length tail) (length_graph tail);
tail_graph :: (xs:List)->@tailLength xs (tailLength xs);
correct := \xs:List => \n:Nat => \g:@tailLength xs n => g
	@nil => TailSize.nil
	@cons head tail n trace => TailSize.cons head tail n (lengthCorrect tail n trace);
correct :: (xs:List)->(n:Nat)->@tailLength xs n->TailSize xs n;
readSize := \xs:List => \n:Nat => \p:Size xs n => p
	@nil => Nat.zero @cons head tail count rest => Nat.succ *rest;
readTail := \xs:List => \n:Nat => \p:TailSize xs n => p
	@nil => Nat.zero @cons head tail count size => readSize tail count size;
one := Nat.succ Nat.zero;
sample := List.cons Nat.zero (List.cons one List.nil);
main := tailLength sample;
proofMain := readTail sample (tailLength sample) (correct sample (tailLength sample) (tail_graph sample));

twice := \xs:List => xs @nil => Nat.zero @cons head tail => {
	first := length tail;
	again := length tail;
	Nat.succ again;
};
twice_graph := \xs:List => xs @(self => @twice self (twice self))
	@nil => (@twice).nil
	@cons head tail => (@twice).cons head tail (length tail) (length_graph tail) (length tail) (length_graph tail);
twice_graph :: (xs:List)->@twice xs (twice xs);
twiceMain := twice sample;
twiceRead := \xs:List => \n:Nat => \g:@twice xs n => g
	@nil => Nat.zero
	@cons head tail first firstGraph again againGraph =>
		Nat.succ (readSize tail again (lengthCorrect tail again againGraph));
twiceProofMain := twiceRead sample (twice sample) (twice_graph sample);
two := Nat.succ one;

append := \xs:List => xs @nil => (\ys:List => ys)
	@cons head tail => (\ys:List => List.cons head (*tail ys));
dropAppend := \xs:List => xs @nil => (\ys:List => ys)
	@cons head tail => (\ys:List => append tail ys);
appendMain := dropAppend sample (List.cons Nat.zero List.nil);
appendExpected := List.cons one (List.cons Nat.zero List.nil);
useAppend := \head:Nat => \xs:List => \ys:List => \zs:List => \p:@append xs ys zs =>
	(@dropAppend).cons head xs ys zs p;
useAppend :: (head:Nat)->(xs:List)->(ys:List)->(zs:List)->@append xs ys zs->@dropAppend (List.cons head xs) ys zs;

GenericList := \A:@ => @{nil:*; cons:A->*->*;};
genericLength := \A:@ => \xs:GenericList A => xs
	@nil => Nat.zero @cons head tail => Nat.succ *tail;
genericTail := \A:@ => \xs:GenericList A => xs
	@nil => Nat.zero @cons head tail => genericLength A tail;
genericSample := (GenericList Nat).cons one ((GenericList Nat).cons Nat.zero (GenericList Nat).nil);
genericMain := genericTail Nat genericSample;
useGeneric := \A:@ => \head:A => \tail:GenericList A => \n:Nat => \p:@genericLength A tail n =>
	(@genericTail A).cons head tail n p;

lengthAgain := \xs:List => {
	count := length xs;
	count @zero => Nat.zero @succ k => Nat.succ k;
};
again_graph := \xs:List => xs @(self => @lengthAgain self (lengthAgain self))
	@nil => (@lengthAgain).zero List.nil (@length).nil
	@cons head tail => (@lengthAgain).succ (List.cons head tail) (length tail)
		((@length).cons head tail (length tail) (length_graph tail));
again_graph :: (xs:List)->@lengthAgain xs (lengthAgain xs);
againCorrect := \xs:List => \n:Nat => \g:@lengthAgain xs n => g
	@zero original trace => lengthCorrect original Nat.zero trace
	@succ original k trace => lengthCorrect original (Nat.succ k) trace;
againCorrect :: (xs:List) -> (n:Nat) -> @lengthAgain xs n -> Size xs n;
againMain := readSize sample (lengthAgain sample) (againCorrect sample (lengthAgain sample) (again_graph sample));
againEmpty := readSize List.nil (lengthAgain List.nil) (againCorrect List.nil (lengthAgain List.nil) (again_graph List.nil));
zero := Nat.zero;
