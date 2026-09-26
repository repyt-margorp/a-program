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

PairLists := \A : @ => @{
	pair : List A -> List A -> *;
};

lessOrEqual := \left : Nat =>
	left @zero => (\right : Nat => Bool.true)
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero => Bool.false
					@succ rightPredecessor =>
						*leftPredecessor rightPredecessor);

lessOrEqual :: Nat -> Nat -> Bool;

partition := \pivot : Nat => \xs : List Nat =>
	xs @nil => (PairLists Nat).pair (List Nat).nil (List Nat).nil
		@cons head tail => {
			partitionedTail := *tail;
			partitionedTail @pair lower upper =>
				(lessOrEqual head pivot
					@true => (PairLists Nat).pair
						((List Nat).cons head lower) upper
					@false => (PairLists Nat).pair lower
						((List Nat).cons head upper));
		};

partition :: Nat -> List Nat -> PairLists Nat;

append := \left : List Nat =>
	left @nil => (\right : List Nat => right)
		@cons head tail =>
			(\right : List Nat => (List Nat).cons head (*tail right));

append :: List Nat -> List Nat -> List Nat;

quickSortWithFuel := \fuel : Nat =>
	fuel @zero => (\xs : List Nat => xs)
		@succ remaining =>
			(\xs : List Nat =>
				xs @nil => (List Nat).nil
					@cons pivot tail => {
						partitioned := partition pivot tail;
						partitioned @pair lower upper => {
							lowerSorted := *remaining lower;
							upperSorted := *remaining upper;
							append lowerSorted
								((List Nat).cons pivot upperSorted);
						};
					});

quickSortWithFuel :: Nat -> List Nat -> List Nat;

one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons one (List Nat).nil);
main := quickSortWithFuel four sample;
expected := {
	(List Nat).cons one
		((List Nat).cons two (List Nat).nil);
};
