Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};

length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail) xs;
length_graph := \xs:List => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:List)->@length xs (length xs);
sample := List.cons Nat.zero (List.cons Nat.zero List.nil);
expected := Nat.succ (Nat.succ Nat.zero);
main := length sample;

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
proofMain := read sample (length sample) (correct sample (length sample) (length_graph sample));

quoted := \xs:List => (&(\ys:List => ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail)) xs;
quotedGraph := @quoted;
quotedMain := quoted sample;
sequenced := \xs:List => {ys:=xs; ys
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;};
sequencedGraph := @sequenced;
sequencedMain := sequenced sample;

Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:(k:Nat)->A->* k->* (Nat.succ k);};
indexed := \A:@ => \n:Nat => \v:Vec A n =>
	(\i:Nat => \xs:Vec A i => xs
		@nil => Nat.zero
		@cons k head tail => Nat.succ *tail) n v;
vector := (Vec Nat).cons Nat.zero Nat.zero (Vec Nat).nil;
indexedGraph := @indexed;
indexedMain := Nat.succ (indexed Nat (Nat.succ Nat.zero) vector);
