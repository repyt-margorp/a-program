Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Nat->*->*;};

length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => (Bool.true
		@false => (\n:Nat => n)
		@true => (\n:Nat => Nat.succ *tail)) Nat.zero) xs;
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

Box := @{mk:Nat->*;};
	unpack := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => ((Box.mk head) @mk value => Nat.succ *tail)) xs;
unpackMain := *unpack sample @output => output;

skip := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => (Bool.false
		@false => Nat.zero
		@true => Nat.succ *tail)) xs;
skipMain := *skip sample @output => output;
zero := Nat.zero;
