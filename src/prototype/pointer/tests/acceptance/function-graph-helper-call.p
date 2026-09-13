Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
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
main := *tailLength sample @output => output;
proofMain := *tailLength sample @output => readTail sample output (correct sample output @output);

twice := \xs:List => xs @nil => Nat.zero @cons head tail => {
	first := length tail;
	again := length tail;
	Nat.succ again;
};
twiceMain := *twice sample @output => output;
twiceRead := \xs:List => \n:Nat => \g:@twice xs n => g
	@nil => Nat.zero
	@cons head tail first firstGraph again againGraph =>
		Nat.succ (readSize tail again (lengthCorrect tail again againGraph));
twiceProofMain := *twice sample @output => twiceRead sample output @output;
two := Nat.succ one;

append := \xs:List => xs @nil => (\ys:List => ys)
	@cons head tail => (\ys:List => List.cons head (*tail ys));
dropAppend := \xs:List => xs @nil => (\ys:List => ys)
	@cons head tail => (\ys:List => append tail ys);
appendMain := *dropAppend sample (List.cons Nat.zero List.nil) @output => output;
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
genericMain := *genericTail Nat genericSample @output => output;
useGeneric := \A:@ => \head:A => \tail:GenericList A => \n:Nat => \p:@genericLength A tail n =>
	(@genericTail A).cons head tail n p;

lengthAgain := \xs:List => {
	count := length xs;
	count @zero => Nat.zero @succ k => Nat.succ k;
};
againCorrect := \xs:List => \n:Nat => \g:@lengthAgain xs n => g
	@zero original trace => lengthCorrect original Nat.zero trace
	@succ original k trace => lengthCorrect original (Nat.succ k) trace;
againCorrect :: (xs:List) -> (n:Nat) -> @lengthAgain xs n -> Size xs n;
againMain := *lengthAgain sample @output =>
	readSize sample output (againCorrect sample output @output);
againEmpty := *lengthAgain List.nil @output =>
	readSize List.nil output (againCorrect List.nil output @output);
zero := Nat.zero;
