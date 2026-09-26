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
sample := (List Nat).cons two ((List Nat).cons one (List Nat).nil);
main := quickSort Nat &lessOrEqual sample;
expected := {
	(List Nat).cons one
		((List Nat).cons two (List Nat).nil);
};

emptyInput := (List Nat).nil;
emptyMain := quickSort Nat &lessOrEqual emptyInput;
emptyExpected := {
	(List Nat).nil;
};

singletonInput := (List Nat).cons two (List Nat).nil;
singletonMain := quickSort Nat &lessOrEqual singletonInput;
singletonExpected := {
	(List Nat).cons two (List Nat).nil;
};

ascendingInput := (List Nat).cons one
	((List Nat).cons two ((List Nat).cons three (List Nat).nil));
ascendingMain := quickSort Nat &lessOrEqual ascendingInput;
ascendingExpected := {
	(List Nat).cons one
		((List Nat).cons two ((List Nat).cons three (List Nat).nil));
};

descendingInput := (List Nat).cons three
	((List Nat).cons two ((List Nat).cons one (List Nat).nil));
descendingMain := quickSort Nat &lessOrEqual descendingInput;
descendingExpected := {
	(List Nat).cons one
		((List Nat).cons two ((List Nat).cons three (List Nat).nil));
};

duplicateInput := (List Nat).cons two
	((List Nat).cons one ((List Nat).cons two (List Nat).nil));
duplicateMain := quickSort Nat &lessOrEqual duplicateInput;
duplicateExpected := {
	(List Nat).cons one
		((List Nat).cons two ((List Nat).cons two (List Nat).nil));
};
