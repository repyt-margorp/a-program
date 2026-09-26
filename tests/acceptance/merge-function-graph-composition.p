Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
List := \A : @ => @{ nil : *; cons : A -> * -> *; };

insertBy := \A : @ => \le : A -> A -> Bool => \value : A => \xs : List A =>
	xs @nil => (List A).cons value (List A).nil
	  @cons head tail =>
		(le value head
			@true => (List A).cons value xs
			@false => {
				insertedTail := *tail;
				(List A).cons head insertedTail;
			});
insertBy :: (A : @) -> (A -> A -> Bool) -> A -> List A -> List A;

// Captured right: the recursive result already has type List A.
structuralMerge := \A : @ => \le : A -> A -> Bool => \left : List A =>
	\right : List A =>
		left @nil => right
		  @cons head tail => {
			mergedTail := *tail;
			insertBy A le head mergedTail;
		  };
structuralMerge :: (A : @) -> (A -> A -> Bool) -> List A -> List A -> List A;

// Branch-local right: the recursive result is a function receiving right.
curriedMerge := \A : @ => \le : A -> A -> Bool => \left : List A =>
	left @nil => (\right : List A => right)
	  @cons head tail => (\right : List A => {
		mergedTail := *tail right;
		insertBy A le head mergedTail;
	  });
curriedMerge :: (A : @) -> (A -> A -> Bool) -> List A -> List A -> List A;

repeatWithFuel := \le : Nat -> Nat -> Bool => \fuel : Nat =>
	fuel @zero => (\xs : List Nat => xs)
	  @succ remaining =>
		(\xs : List Nat =>
			xs @nil => (List Nat).nil
			  @cons head tail => *remaining xs);
repeatWithFuel :: (Nat -> Nat -> Bool) -> Nat -> List Nat -> List Nat;

lessEqual := \n : Nat => n
	@zero => (\m : Nat => Bool.true)
	@succ k => (\m : Nat => m @zero => Bool.false @succ j => *k j);
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
nil := (List Nat).nil;
left := (List Nat).cons zero ((List Nat).cons two nil);
right := (List Nat).cons one nil;
expected := (List Nat).cons zero ((List Nat).cons one ((List Nat).cons two nil));
main := structuralMerge Nat &lessEqual left right;
curriedMain := curriedMerge Nat &lessEqual left right;
fuelMain := repeatWithFuel &lessEqual two main;
emptyMain := structuralMerge Nat &lessEqual nil right;
emptyExpected := right;
