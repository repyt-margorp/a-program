// Foundational list-sorting algorithms for the pointer-core prototype.
//
// Every algorithm takes the same explicit Boolean comparison. `quickSort`
// uses Acc evidence to justify recursion on a strictly smaller measured list.

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

natLessOrEqual := \left : Nat =>
	left @zero => (\right : Nat => Bool.true)
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero => Bool.false
					@succ rightPredecessor =>
						*leftPredecessor rightPredecessor);

natLessOrEqual :: Nat -> Nat -> Bool;

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

insertionSortBy := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	xs @nil => (List A).nil
		@cons head tail => {
			sortedTail := *tail;
			insertBy A le head sortedTail;
		};

insertionSortBy :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

insertNat := insertBy Nat natLessOrEqual;
insertNat :: Nat -> List Nat -> List Nat;

insertionSort := insertionSortBy Nat natLessOrEqual;
insertionSort :: List Nat -> List Nat;

LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
		lift : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};

Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

accessibleSucc := \n : Nat => \proof : Acc Nat LT n =>
	proof @acc current down =>
		(Acc Nat LT).acc (Nat.succ current)
			&(\y : Nat => \edge : LT y (Nat.succ current) =>
				edge @step k => proof
					@weakenRight m k prior => down m prior
					@lift m k prior => *down m prior);

accessibleSucc :: (n : Nat) -> Acc Nat LT n -> Acc Nat LT (Nat.succ n);

natAccessible := \n : Nat =>
	n @zero =>
		(Acc Nat LT).acc Nat.zero
				&(\y : Nat => \edge : LT y Nat.zero =>
					edge @step k => Nat.zero
						@weakenRight m k prior => Nat.zero
						@lift m k prior => Nat.zero)
		@succ k => accessibleSucc k *k;

natAccessible :: (n : Nat) -> Acc Nat LT n;

SizedList := \A : @ => @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> A -> * n -> * (Nat.succ n);
};

Measured := \A : @ => @{
	measured : (n : Nat) -> SizedList A n -> *;
};

Partition := \A : @ => \bound : Nat => @{
	parts :
		(lowerSize : Nat) ->
		(lower : SizedList A lowerSize) ->
		(upperSize : Nat) ->
		(upper : SizedList A upperSize) ->
		LT lowerSize (Nat.succ bound) ->
		LT upperSize (Nat.succ bound) -> *;
};

partitionLower := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					(Nat.succ lowerSize)
					((SizedList A).cons lowerSize head lower)
					upperSize upper
					(LT.lift lowerSize (Nat.succ size) lowerBound)
					(LT.weakenRight upperSize (Nat.succ size) upperBound);

partitionLower :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionUpper := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					lowerSize lower
					(Nat.succ upperSize)
					((SizedList A).cons upperSize head upper)
					(LT.weakenRight lowerSize (Nat.succ size) lowerBound)
					(LT.lift upperSize (Nat.succ size) upperBound);

partitionUpper :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionByDecision := \A : @ => \head : A => \size : Nat =>
	\decision : Bool => \partitioned : Partition A size =>
		decision
			@true => partitionLower A head size partitioned
			@false => partitionUpper A head size partitioned;

partitionByDecision :: (A : @) -> A -> (size : Nat) -> Bool ->
	Partition A size -> Partition A (Nat.succ size);

partition := \A : @ => \le : A -> A -> Bool =>
	\pivot : A => \size : Nat => \xs : SizedList A size =>
		xs @nil =>
			(Partition A Nat.zero).parts Nat.zero (SizedList A).nil
					Nat.zero (SizedList A).nil
					(LT.step Nat.zero) (LT.step Nat.zero)
		@cons tailSize head tail => {
			decision := le head pivot;
			partitionByDecision A head tailSize decision *tail;
		};

partition :: (A : @) -> (A -> A -> Bool) -> (pivot : A) ->
	(size : Nat) -> SizedList A size -> Partition A size;

measure := \A : @ => \xs : List A =>
	xs @nil => (Measured A).measured Nat.zero (SizedList A).nil
		@cons head tail =>
			(*tail @measured n values =>
				(Measured A).measured (Nat.succ n)
					((SizedList A).cons n head values));

measure :: (A : @) -> List A -> Measured A;

append := \A : @ => \left : List A =>
	left @nil => (\right : List A => right)
		@cons head tail =>
			(\right : List A => (List A).cons head (*tail right));

append :: (A : @) -> List A -> List A -> List A;

// TreeSort demonstrates a second structural recursion scheme. The tree is
// deliberately unbalanced: the supplied comparison controls its shape.
Tree := \A : @ => @{
	empty : *;
	node : A -> * -> * -> *;
};

treeInsert := \A : @ => \le : A -> A -> Bool => \value : A =>
	\tree : Tree A =>
		tree @empty => (Tree A).node value (Tree A).empty (Tree A).empty
			@node root lower upper =>
				(le value root
					@true => {
						insertedLower := *lower;
						(Tree A).node root insertedLower upper;
					}
					@false => {
						insertedUpper := *upper;
						(Tree A).node root lower insertedUpper;
					});

treeInsert :: (A : @) -> (A -> A -> Bool) -> A -> Tree A -> Tree A;

treeBuild := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	xs @nil => (Tree A).empty
		@cons head tail => {
			builtTail := *tail;
			treeInsert A le head builtTail;
		};

treeBuild :: (A : @) -> (A -> A -> Bool) -> List A -> Tree A;

treeToList := \A : @ => \tree : Tree A =>
	tree @empty => (List A).nil
		@node value lower upper => {
			lowerValues := *lower;
			upperValues := *upper;
			append A lowerValues ((List A).cons value upperValues);
		};

treeToList :: (A : @) -> Tree A -> List A;

treeSort := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	treeToList A (treeBuild A le xs);

treeSort :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

// Alternating split gives each recursive MergeSort call approximately half of
// its input. The result is intentionally unindexed; the recursion below is
// justified by an explicit decreasing fuel value.
Halves := \A : @ => @{
	halves : List A -> List A -> *;
};

splitAlternating := \A : @ => \xs : List A =>
	xs @nil => (Halves A).halves (List A).nil (List A).nil
		@cons head tail =>
			(*tail @halves left right =>
				(Halves A).halves ((List A).cons head right) left);

splitAlternating :: (A : @) -> List A -> Halves A;

// `right` is captured by the Match. Therefore `*tail` already has type
// `List A`; it must not be applied to `right` again.
mergeBy := \A : @ => \le : A -> A -> Bool => \left : List A =>
	\right : List A =>
		left @nil => right
			@cons head tail => {
				mergedTail := *tail;
				insertBy A le head mergedTail;
			};

mergeBy :: (A : @) -> (A -> A -> Bool) -> List A -> List A -> List A;

mergeSortFuel := \le : Nat -> Nat -> Bool => \fuel : Nat =>
	fuel @zero => (\xs : List Nat => xs)
		@succ remaining =>
			(\xs : List Nat =>
				xs @nil => (List Nat).nil
					@cons head tail => {
						splitResult := splitAlternating Nat xs;
						splitResult @halves left right => {
							leftSorted := *remaining left;
							rightSorted := *remaining right;
							mergeBy Nat le leftSorted rightSorted;
						};
					});

mergeSortFuel :: (Nat -> Nat -> Bool) -> Nat -> List Nat -> List Nat;

mergeSort := \le : Nat -> Nat -> Bool => \xs : List Nat =>
	measure Nat xs @measured size ignored => mergeSortFuel le size xs;

mergeSort :: (Nat -> Nat -> Bool) -> List Nat -> List Nat;

quickSortAcc := \A : @ => \le : A -> A -> Bool =>
	\size : Nat => \access : Acc Nat LT size =>
		access @acc current down =>
			(\input : SizedList A current =>
					input @nil => (List A).nil
						@cons tailSize pivot tail => {
							partitioned := partition A le pivot tailSize tail;
							partitioned
								@parts lowerSize lower upperSize upper lowerBound upperBound =>
									{
										lowerResult := *down lowerSize lowerBound lower;
										upperResult := *down upperSize upperBound upper;
										append A lowerResult
											((List A).cons pivot upperResult);
									};
				});

quickSortAcc :: (A : @) -> (A -> A -> Bool) ->
	(size : Nat) -> Acc Nat LT size -> SizedList A size -> List A;

quickSort := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	measure A xs @measured size values =>
		quickSortAcc A le size (natAccessible size) values;

quickSort :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

quickSortTerminates := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	#.terminates (&(quickSort A &le xs));
