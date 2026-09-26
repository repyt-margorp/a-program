// Issue #28: the posted reproducer applies an already computed List as a function.
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

structuralMerge := \A : @ => \le : A -> A -> Bool => \left : List A =>
	\right : List A =>
		left @nil => right
		  @cons head tail => {
			mergedTail := *tail right;
			insertBy A le head mergedTail;
		  };
structuralMerge :: (A : @) -> (A -> A -> Bool) -> List A -> List A -> List A;

repeatWithFuel := \le : Nat -> Nat -> Bool => \fuel : Nat =>
	fuel @zero => (\xs : List Nat => xs)
	  @succ remaining =>
		(\xs : List Nat =>
			xs @nil => (List Nat).nil
			  @cons head tail => *remaining xs);
repeatWithFuel :: (Nat -> Nat -> Bool) -> Nat -> List Nat -> List Nat;
