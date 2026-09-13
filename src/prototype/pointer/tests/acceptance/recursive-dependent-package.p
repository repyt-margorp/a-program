Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
SizedList := \A:@ => @\size:Nat => {
	nil:* Nat.zero;
	cons:(n:Nat)->A->* n->* (Nat.succ n);
};
Measured := \A:@ => @{measured:(n:Nat)->SizedList A n->*;};
measure := \A:@ => \xs:List A =>
	xs @nil => (Measured A).measured Nat.zero (SizedList A).nil
		@cons head tail =>
			(*tail @measured n values =>
				(Measured A).measured (Nat.succ n)
					((SizedList A).cons n head values));
measure :: (A:@)->List A->Measured A;
main := measure Nat ((List Nat).cons Nat.zero (List Nat).nil);
expected := (Measured Nat).measured (Nat.succ Nat.zero)
	((SizedList Nat).cons Nat.zero Nat.zero (SizedList Nat).nil);
