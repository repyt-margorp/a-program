Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
length := \A:@ => \xs:List A => xs
	@nil => Nat.zero
	@cons head tail => Nat.succ *tail;
lengthNat := length Nat;
Size := @\xs:List Nat => @\n:Nat => {
	nil:* (List Nat).nil Nat.zero;
	cons:(head:Nat)->(tail:List Nat)->(n:Nat)->* tail n->* ((List Nat).cons head tail) (Nat.succ n);
};
correct := \xs:List Nat => \n:Nat => \proof:@lengthNat xs n => proof
	@nil => Size.nil
	@cons head tail count prior => Size.cons head tail count *prior;
correct :: (xs:List Nat)->(n:Nat)->@lengthNat xs n->Size xs n;
read := \xs:List Nat => \n:Nat => \proof:Size xs n => proof
	@nil => Nat.zero
	@cons head tail count prior => Nat.succ *prior;
one := Nat.succ Nat.zero;
sample := (List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil);
main := *lengthNat sample @n => read sample n (correct sample n @n);
expected := Nat.succ one;
alias := lengthNat;
aliasMain := *alias sample @n => read sample n (correct sample n @n);
