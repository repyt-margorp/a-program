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
lengthAgain := \xs:List => {
	count := length xs;
	count @zero => Nat.zero @succ k => Nat.succ k;
};
againCorrect := \xs:List => \n:Nat => \g:@lengthAgain xs n => g
	@zero original trace => lengthCorrect original Nat.zero trace
	@succ original k trace => lengthCorrect original (Nat.succ k) trace;
againCorrect :: (xs:List) -> (n:Nat) -> @lengthAgain xs n -> Size xs n;
