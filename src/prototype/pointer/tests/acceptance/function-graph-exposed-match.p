Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};

length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail) xs;
sample := List.cons Nat.zero (List.cons Nat.zero List.nil);
expected := Nat.succ (Nat.succ Nat.zero);
main := *length sample @output => output;

Size := @\xs:List => @\n:Nat => {
	nil:* List.nil Nat.zero;
	cons:(head:Nat)->(tail:List)->(n:Nat)->* tail n->* (List.cons head tail) (Nat.succ n);
};
correct := \xs:List => \n:Nat => \g:@length xs n => g
	@nil => Size.nil
	@cons head tail count trace => Size.cons head tail count *trace;
correct :: (xs:List)->(n:Nat)->@length xs n->Size xs n;
read := \xs:List => \n:Nat => \p:Size xs n => p
	@nil => Nat.zero
	@cons head tail count trace => Nat.succ *trace;
proofMain := *length sample @output => read sample output (correct sample output @output);

quoted := \xs:List => (&(\ys:List => ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail)) xs;
quotedMain := *quoted sample @output => output;
sequenced := \xs:List => {ys:=xs; ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;};
sequencedMain := *sequenced sample @output => output;

Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->A->* k->* (Nat.succ k);};
indexed := \A:@ => \n:Nat => \v:Vec A n =>
	(\i:Nat => \xs:Vec A i => xs
		@nil => Nat.zero
		@cons k head tail => Nat.succ *tail) n v;
vector := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
indexedMain := *indexed Nat (Nat.succ Nat.zero) vector @output => Nat.succ output;
