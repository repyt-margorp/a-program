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

sort := \xs : List Nat =>
	xs @nil => (List Nat).nil
		@cons head tail => {
			sortedTail := *tail;
			insert head sortedTail;
		};

sort :: List Nat -> List Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
sample := (List Nat).cons three
	((List Nat).cons one ((List Nat).cons two (List Nat).nil));
main := sort sample;
expected := {
	(List Nat).cons one
		((List Nat).cons two ((List Nat).cons three (List Nat).nil));
};
