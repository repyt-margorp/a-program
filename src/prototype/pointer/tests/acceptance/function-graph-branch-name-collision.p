Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Bool->Bool->*->*;};
length := \xs:List => xs @nil => Nat.zero
	@cons first second tail => (first
		@false => (second @false => Nat.succ *tail @true => Nat.succ *tail)
		@true => (second @false => Nat.succ *tail @true => Nat.succ *tail));
relation := @length;
Size := @\xs:List => @\n:Nat => {
	nil:* List.nil Nat.zero;
	cons:(first:Bool)->(second:Bool)->(tail:List)->(n:Nat)->* tail n->
		* (List.cons first second tail) (Nat.succ n);
};
correct := \xs:List => \n:Nat => \g:relation xs n => g
	@case0 => Size.nil
	@case1 tail n trace => Size.cons Bool.false Bool.false tail n *trace
	@case2 tail n trace => Size.cons Bool.false Bool.true tail n *trace
	@case3 tail n trace => Size.cons Bool.true Bool.false tail n *trace
	@case4 tail n trace => Size.cons Bool.true Bool.true tail n *trace;
correct :: (xs:List)->(n:Nat)->relation xs n->Size xs n;
read := \xs:List => \n:Nat => \p:Size xs n => p
	@nil => Nat.zero @cons first second tail n rest => Nat.succ *rest;
sample := List.cons Bool.false Bool.false
	(List.cons Bool.false Bool.true
	(List.cons Bool.true Bool.false
	(List.cons Bool.true Bool.true List.nil)));
main := *length sample @output => read sample output (correct sample output @output);
expected := Nat.succ (Nat.succ (Nat.succ (Nat.succ Nat.zero)));
