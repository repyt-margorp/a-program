Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

lessOrEqual := \left : Nat =>
	left @zero => (\right : Nat => Bool.true)
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero => Bool.false
					@succ rightPredecessor =>
						*leftPredecessor rightPredecessor);

lessOrEqual :: Nat -> Nat -> Bool;

insert := \value : Nat => \xs : List Nat =>
	xs @nil => (List Nat).cons value (List Nat).nil
		@cons head tail =>
			(lessOrEqual value head
				@true => (List Nat).cons value xs
				@false => {
					insertedTail := *tail;
					(List Nat).cons head insertedTail;
				});

insert :: Nat -> List Nat -> List Nat;

insertByDecision := \decision : Bool => \value : Nat => \xs : List Nat =>
	xs @nil => (List Nat).cons value (List Nat).nil
		@cons head tail =>
			(decision
				@true => (List Nat).cons value xs
				@false => {
					insertedTail := *tail;
					(List Nat).cons head insertedTail;
				});

insertByDecision :: Bool -> Nat -> List Nat -> List Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
sortedOuter := (List Nat).cons one
	((List Nat).cons three (List Nat).nil);
earlyInput := (List Nat).cons two
	((List Nat).cons three (List Nat).nil);
earlyMain := insert one earlyInput;
earlyExpected := {
	(List Nat).cons one earlyInput;
};
traceEarly := insertByDecision Bool.true one earlyInput;
traceEarlyExpected := {
	(List Nat).cons one earlyInput;
};
traceRecursive := insertByDecision Bool.false one earlyInput;
traceRecursiveExpected := {
	(List Nat).cons two
		((List Nat).cons three ((List Nat).cons one (List Nat).nil));
};
main := insert two sortedOuter;
expected := {
	(List Nat).cons one
		((List Nat).cons two ((List Nat).cons three (List Nat).nil));
};
