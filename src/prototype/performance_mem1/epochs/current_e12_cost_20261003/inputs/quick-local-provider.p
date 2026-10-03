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

// Shared predicates for the graph theorem and the ordinary-result theorem.
general_all_from := \A:@ => \r:A->A->@ => \head:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(next:A)->(tail:List A)->r head next->* tail->* ((List A).cons next tail);
};
general_sorted := \A:@ => \r:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->general_all_from A r head tail->* tail->* ((List A).cons head tail);
};
general_decision := \A:@ => \r:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:r x y->* Bool.true;
	no:r y x->* Bool.false;
};

// This alias preserves the existing nominal family and its strong contract.
general_strongly_sorted := general_sorted;

general_locally_sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	one:(x:A)->* ((List A).cons x (List A).nil);
	cons:(x:A)->(y:A)->(ys:List A)->R x y->
		* ((List A).cons y ys)->* ((List A).cons x ((List A).cons y ys));
};

sorted_bound_head := \A:@ => \R:A->A->@ => \x:A => \y:A => \ys:List A =>
	\p:general_all_from A R x ((List A).cons y ys) => p @cons h t ph pt => ph;
sorted_strong_bound := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	\p:general_strongly_sorted A R ((List A).cons x xs) => p @cons h t ph pt => ph;

sorted_local_prepend := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	xs @(self => general_all_from A R x self->general_locally_sorted A R self->
		general_locally_sorted A R ((List A).cons x self))
	@nil => (\bound:general_all_from A R x (List A).nil =>
		\tail:general_locally_sorted A R (List A).nil => (general_locally_sorted A R).one x)
	@cons y ys => (\bound:general_all_from A R x ((List A).cons y ys) =>
		\tail:general_locally_sorted A R ((List A).cons y ys) =>
		(general_locally_sorted A R).cons x y ys (sorted_bound_head A R x y ys bound) tail);
sorted_local_prepend :: (A:@)->(R:A->A->@)->(x:A)->(xs:List A)->
	general_all_from A R x xs->general_locally_sorted A R xs->
	general_locally_sorted A R ((List A).cons x xs);

strongly_sorted_to_locally_sorted := \A:@ => \R:A->A->@ => \xs:List A =>
	\p:general_strongly_sorted A R xs => p
	@nil => (general_locally_sorted A R).nil
	@cons h t ph pt => sorted_local_prepend A R h t ph *pt;
strongly_sorted_to_locally_sorted :: (A:@)->(R:A->A->@)->(xs:List A)->
	general_strongly_sorted A R xs->general_locally_sorted A R xs;

sorted_bound_trans := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\y:A => \ys:List A => \p:general_all_from A R y ys => p
	@nil => (\x:A => \xy:R x y => (general_all_from A R x).nil)
	@cons h t ph pt => (\x:A => \xy:R x y =>
		(general_all_from A R x).cons h t (trans x y xy h ph) (*pt x xy));
sorted_bound_trans :: (A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(y:A)->(ys:List A)->general_all_from A R y ys->(x:A)->R x y->general_all_from A R x ys;

locally_sorted_to_strongly_sorted := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\xs:List A => \p:general_locally_sorted A R xs => p
	@nil => (general_strongly_sorted A R).nil
	@one x => (general_strongly_sorted A R).cons x (List A).nil
		(general_all_from A R x).nil (general_strongly_sorted A R).nil
	@cons x y ys xy tail => (general_strongly_sorted A R).cons x ((List A).cons y ys)
		((general_all_from A R x).cons y ys xy
			(sorted_bound_trans A R trans y ys (sorted_strong_bound A R y ys *tail) x xy)) *tail;
locally_sorted_to_strongly_sorted :: (A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(xs:List A)->
	general_locally_sorted A R xs->general_strongly_sorted A R xs;

decision_diagonal := \A:@ => \R:A->A->@ => \x:A => \answer:Bool =>
	\proof:general_decision A R x x answer => proof
	@yes evidence => evidence
	@no evidence => evidence;
decision_diagonal :: (A:@)->(R:A->A->@)->(x:A)->(answer:Bool)->
	general_decision A R x x answer->R x x;
// Shared partition and All-preservation lemmas; no ordering laws are assumed.

qsr_All := \A:@ => \P:A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->P h->* t->* ((List A).cons h t);
};
qsr_SizedAll := \A:@ => \P:A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(n:Nat)->(h:A)->(t:SizedList A n)->P h->* n t->
		* (Nat.succ n) ((SizedList A).cons n h t);
};
qsr_PartAll := \A:@ => \P:A->@ => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		qsr_SizedAll A P l left->qsr_SizedAll A P r right->
		* ((Partition A n).parts l left r right lb rb);
};
qsr_sized_head := \A:@ => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:qsr_SizedAll A P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b ph pt => ph;
qsr_sized_tail := \A:@ => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:qsr_SizedAll A P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b ph pt => pt;

qsr_lower_all := \A:@ => \P:A->@ => \h:A => \n:Nat => \ph:P h => \parts:Partition A n =>
	\prior:qsr_PartAll A P n parts => prior
	@(value self => qsr_PartAll A P (Nat.succ n) (partitionLower A h n value))
	@parts l left r right lb rb pl pr =>
		(qsr_PartAll A P (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			((qsr_SizedAll A P).cons l h left ph pl) pr;
qsr_lower_all :: (A:@)->(P:A->@)->(h:A)->(n:Nat)->P h->(parts:Partition A n)->
	qsr_PartAll A P n parts->qsr_PartAll A P (Nat.succ n) (partitionLower A h n parts);

qsr_upper_all := \A:@ => \P:A->@ => \h:A => \n:Nat => \ph:P h => \parts:Partition A n =>
	\prior:qsr_PartAll A P n parts => prior
	@(value self => qsr_PartAll A P (Nat.succ n) (partitionUpper A h n value))
	@parts l left r right lb rb pl pr =>
		(qsr_PartAll A P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			pl ((qsr_SizedAll A P).cons r h right ph pr);
qsr_upper_all :: (A:@)->(P:A->@)->(h:A)->(n:Nat)->P h->(parts:Partition A n)->
	qsr_PartAll A P n parts->qsr_PartAll A P (Nat.succ n) (partitionUpper A h n parts);

qsr_decision_all := \A:@ => \P:A->@ => \h:A => \n:Nat => \d:Bool => \parts:Partition A n =>
	\ph:P h => \prior:qsr_PartAll A P n parts =>
	d @(self => qsr_PartAll A P (Nat.succ n) (partitionByDecision A h n self parts))
	@true => qsr_lower_all A P h n ph parts prior
	@false => qsr_upper_all A P h n ph parts prior;
qsr_decision_all :: (A:@)->(P:A->@)->(h:A)->(n:Nat)->(d:Bool)->(parts:Partition A n)->
	P h->qsr_PartAll A P n parts->qsr_PartAll A P (Nat.succ n) (partitionByDecision A h n d parts);

qsr_partition_all := \A:@ => \P:A->@ => \le:A->A->Bool => \pivot:A => \n:Nat => \xs:SizedList A n =>
	xs @(size self => (prior:qsr_SizedAll A P size self)->qsr_PartAll A P size (partition A (&le) pivot size self))
	@nil => (\prior:qsr_SizedAll A P Nat.zero (SizedList A).nil =>
		(qsr_PartAll A P Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
			(LT.step Nat.zero) (LT.step Nat.zero) (qsr_SizedAll A P).nil (qsr_SizedAll A P).nil)
	@cons k h t => (\prior:qsr_SizedAll A P (Nat.succ k) ((SizedList A).cons k h t) =>
		qsr_decision_all A P h k (le h pivot) (partition A (&le) pivot k t)
			(qsr_sized_head A P k h t prior) (*t (qsr_sized_tail A P k h t prior)));
qsr_partition_all :: (A:@)->(P:A->@)->(le:A->A->Bool)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->
	qsr_SizedAll A P n xs->qsr_PartAll A P n (partition A (&le) pivot n xs);

qsr_Decision := \A:@ => \R:A->A->@ => \x:A => \y:A => @\answer:Bool => {
	yes:R x y->* Bool.true;
	no:R y x->* Bool.false;
};
qsr_PartOrdered := \A:@ => \R:A->A->@ => \pivot:A => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		qsr_SizedAll A (&(\h:A => R h pivot)) l left->qsr_SizedAll A (&(\h:A => R pivot h)) r right->
		* ((Partition A n).parts l left r right lb rb);
};
qsr_lower_ordered := \A:@ => \R:A->A->@ => \pivot:A => \h:A => \n:Nat => \hp:R h pivot =>
	\parts:Partition A n => \prior:qsr_PartOrdered A R pivot n parts => prior
	@(value self => qsr_PartOrdered A R pivot (Nat.succ n) (partitionLower A h n value))
	@parts l left r right lb rb pl pr =>
		(qsr_PartOrdered A R pivot (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			((qsr_SizedAll A (&(\x:A => R x pivot))).cons l h left hp pl) pr;
qsr_upper_ordered := \A:@ => \R:A->A->@ => \pivot:A => \h:A => \n:Nat => \ph:R pivot h =>
	\parts:Partition A n => \prior:qsr_PartOrdered A R pivot n parts => prior
	@(value self => qsr_PartOrdered A R pivot (Nat.succ n) (partitionUpper A h n value))
	@parts l left r right lb rb pl pr =>
		(qsr_PartOrdered A R pivot (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			pl ((qsr_SizedAll A (&(\x:A => R pivot x))).cons r h right ph pr);
qsr_decision_ordered := \A:@ => \R:A->A->@ => \pivot:A => \h:A => \n:Nat =>
	\parts:Partition A n => \prior:qsr_PartOrdered A R pivot n parts => \d:Bool => \pd:qsr_Decision A R h pivot d =>
	pd @(answer self => qsr_PartOrdered A R pivot (Nat.succ n) (partitionByDecision A h n answer parts))
	@yes hp => qsr_lower_ordered A R pivot h n hp parts prior
	@no ph => qsr_upper_ordered A R pivot h n ph parts prior;
qsr_decision_ordered :: (A:@)->(R:A->A->@)->(pivot:A)->(h:A)->(n:Nat)->
	(parts:Partition A n)->qsr_PartOrdered A R pivot n parts->(d:Bool)->qsr_Decision A R h pivot d->
	qsr_PartOrdered A R pivot (Nat.succ n) (partitionByDecision A h n d parts);
qsr_partition_ordered := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n =>
	xs @(size self => qsr_PartOrdered A R pivot size (partition A (&le) pivot size self))
	@nil => (qsr_PartOrdered A R pivot Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
		(LT.step Nat.zero) (LT.step Nat.zero)
		(qsr_SizedAll A (&(\h:A => R h pivot))).nil (qsr_SizedAll A (&(\h:A => R pivot h))).nil
	@cons k h t => qsr_decision_ordered A R pivot h k (partition A (&le) pivot k t) *t (le h pivot) (decide h pivot);
qsr_partition_ordered :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y))->(pivot:A)->(n:Nat)->(xs:SizedList A n)->
	qsr_PartOrdered A R pivot n (partition A (&le) pivot n xs);

qsr_all_head := \A:@ => \P:A->@ => \h:A => \t:List A => \p:qsr_All A P ((List A).cons h t) =>
	p @cons x xs ph pt => ph;
qsr_all_tail := \A:@ => \P:A->@ => \h:A => \t:List A => \p:qsr_All A P ((List A).cons h t) =>
	p @cons x xs ph pt => pt;
qsr_append_all := \A:@ => \P:A->@ => \xs:List A =>
	xs @(self => (ys:List A)->qsr_All A P self->qsr_All A P ys->qsr_All A P (append A self ys))
	@nil => (\ys:List A => \pl:qsr_All A P (List A).nil => \pr:qsr_All A P ys => pr)
	@cons h t => (\ys:List A => \pl:qsr_All A P ((List A).cons h t) => \pr:qsr_All A P ys =>
		(qsr_All A P).cons h (append A t ys) (qsr_all_head A P h t pl) (*t ys (qsr_all_tail A P h t pl) pr));
qsr_append_all :: (A:@)->(P:A->@)->(xs:List A)->(ys:List A)->qsr_All A P xs->qsr_All A P ys->qsr_All A P (append A xs ys);

// This name only abbreviates the existing recursive branch in theorem types.
// The final post-check targets the imported quickSort, not this abbreviation.
qsr_join_values := \A:@ => \le:A->A->Bool => \k:Nat => \pivot:A =>
	\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m => \parts:Partition A k =>
	parts @parts l lower r upper lb rb =>
		append A (quickSortAcc A (&le) l (down l lb) lower)
			((List A).cons pivot (quickSortAcc A (&le) r (down r rb) upper));
qsr_join_all := \A:@ => \P:A->@ => \le:A->A->Bool => \k:Nat => \pivot:A =>
	\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
	\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(xs:SizedList A m)->qsr_SizedAll A P m xs->
		qsr_All A P (quickSortAcc A (&le) m (down m bound) xs) =>
	\ph:P pivot => \parts:Partition A k => \prior:qsr_PartAll A P k parts =>
	prior @(value self => qsr_All A P (qsr_join_values A (&le) k pivot (&down) value))
	@parts l lower r upper lb rb pl pr =>
		qsr_append_all A P (quickSortAcc A (&le) l (down l lb) lower)
			((List A).cons pivot (quickSortAcc A (&le) r (down r rb) upper))
			(ih l lb lower pl)
			((qsr_All A P).cons pivot (quickSortAcc A (&le) r (down r rb) upper) ph (ih r rb upper pr));
qsr_join_all :: (A:@)->(P:A->@)->(le:A->A->Bool)->(k:Nat)->(pivot:A)->
	(down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m)->
	(ih:(m:Nat)->(bound:LT m (Nat.succ k))->(xs:SizedList A m)->qsr_SizedAll A P m xs->
		qsr_All A P (quickSortAcc A (&le) m (down m bound) xs))->
	P pivot->(parts:Partition A k)->qsr_PartAll A P k parts->qsr_All A P (qsr_join_values A (&le) k pivot (&down) parts);

qsr_quick_all_step := \A:@ => \P:A->@ => \le:A->A->Bool => \n:Nat => \xs:SizedList A n =>
	xs @(size self => (down:(m:Nat)->LT m size->Acc Nat LT m)->
		(ih:(m:Nat)->(bound:LT m size)->(values:SizedList A m)->qsr_SizedAll A P m values->
			qsr_All A P (quickSortAcc A (&le) m (down m bound) values))->
		qsr_SizedAll A P size self->qsr_All A P (quickSortAcc A (&le) size ((Acc Nat LT).acc size (&down)) self))
	@nil => (\down:(m:Nat)->LT m Nat.zero->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m Nat.zero)->(values:SizedList A m)->qsr_SizedAll A P m values->
			qsr_All A P (quickSortAcc A (&le) m (down m bound) values) =>
		\prior:qsr_SizedAll A P Nat.zero (SizedList A).nil => (qsr_All A P).nil)
	@cons k h t => (\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(values:SizedList A m)->qsr_SizedAll A P m values->
			qsr_All A P (quickSortAcc A (&le) m (down m bound) values) =>
		\prior:qsr_SizedAll A P (Nat.succ k) ((SizedList A).cons k h t) =>
		qsr_join_all A P (&le) k h (&down) (&ih) (qsr_sized_head A P k h t prior)
			(partition A (&le) h k t) (qsr_partition_all A P (&le) h k t (qsr_sized_tail A P k h t prior)));
qsr_quick_all_step :: (A:@)->(P:A->@)->(le:A->A->Bool)->(n:Nat)->(xs:SizedList A n)->
	(down:(m:Nat)->LT m n->Acc Nat LT m)->
	(ih:(m:Nat)->(bound:LT m n)->(values:SizedList A m)->qsr_SizedAll A P m values->
		qsr_All A P (quickSortAcc A (&le) m (down m bound) values))->
	qsr_SizedAll A P n xs->qsr_All A P (quickSortAcc A (&le) n ((Acc Nat LT).acc n (&down)) xs);

qsr_quick_all := \A:@ => \P:A->@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n =>
	access @(size self => (xs:SizedList A size)->qsr_SizedAll A P size xs->qsr_All A P (quickSortAcc A (&le) size self xs))
	@acc size down => (\xs:SizedList A size => qsr_quick_all_step A P (&le) size xs (&down) &*down);
qsr_quick_all :: (A:@)->(P:A->@)->(le:A->A->Bool)->(n:Nat)->(access:Acc Nat LT n)->(xs:SizedList A n)->
	qsr_SizedAll A P n xs->qsr_All A P (quickSortAcc A (&le) n access xs);

qsr_all_connection := \A:@ => \R:A->A->@ => \head:A => \xs:List A =>
	\p:qsr_All A (&(\x:A => R head x)) xs => p
	@nil => (general_all_from A R head).nil
	@cons h t ph pt => (general_all_from A R head).cons h t ph *pt;
qsr_all_connection :: (A:@)->(R:A->A->@)->(head:A)->(xs:List A)->
	qsr_All A (&(\x:A => R head x)) xs->general_all_from A R head xs;

qsr_decision_connection := \A:@ => \R:A->A->@ => \x:A => \y:A => \answer:Bool =>
	\p:general_decision A R x y answer => p
	@yes proof => (qsr_Decision A R x y).yes proof
	@no proof => (qsr_Decision A R x y).no proof;
qsr_decision_connection :: (A:@)->(R:A->A->@)->(x:A)->(y:A)->(answer:Bool)->
	general_decision A R x y answer->qsr_Decision A R x y answer;
