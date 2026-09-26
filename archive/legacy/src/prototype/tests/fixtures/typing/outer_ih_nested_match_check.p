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

chooseTail := \xs : List Nat =>
	xs @nil => (List Nat).nil
		@cons head tail =>
			(Bool.true
				@true => *tail
				@false => (List Nat).cons head (List Nat).nil);

chooseTail :: List Nat -> List Nat;

applyOuter := \xs : List Nat =>
	xs @nil => (\value : Nat => value)
		@cons head tail =>
			(\value : Nat =>
				Bool.true
					@true => *tail value
					@false => head);

applyOuter :: List Nat -> Nat -> Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
sample := (List Nat).cons one ((List Nat).cons two (List Nat).nil);
tailResult := chooseTail sample;
tailExpected := { (List Nat).nil; };
functionResult := applyOuter sample two;
functionExpected := { two; };
