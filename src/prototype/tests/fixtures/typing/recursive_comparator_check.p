Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

lessOrEqual := \left : Nat =>
	left @zero => (\right : Nat => Bool.true)
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero => Bool.false
					@succ rightPredecessor =>
						*leftPredecessor rightPredecessor);

lessOrEqual :: Nat -> Nat -> Bool;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;

lessResult := lessOrEqual two three;
lessExpected := { Bool.true; };
greaterResult := lessOrEqual three two;
greaterExpected := { Bool.false; };

main := lessResult;
