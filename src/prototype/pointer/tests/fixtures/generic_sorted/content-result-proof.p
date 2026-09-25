// Append after content-proof.p. Prove preservation for the ordinary call,
// independently of a generated graph packet and without comparator assumptions.
import natAccessible;

append_graph := \A:@ => \xs:List A => xs
	@(self => (ys:List A)->@append A self ys (append A self ys))
	@nil => (\ys:List A => (@append A).case0 ys)
	@cons h t => (\ys:List A => (@append A).case1 h t ys (append A t ys) (*t ys));
append_graph :: (A:@)->(xs:List A)->(ys:List A)->@append A xs ys (append A xs ys);

lower_content_result := \A:@ => \h:A => \n:Nat => \original:List A => \parts:Partition A n =>
	parts @(self => partition_result_contents A original n self->
		partition_result_contents A ((List A).cons h original) (Nat.succ n) (partitionLower A h n self))
	@parts l left r right lb rb => (\prior:partition_contents A original l left r right =>
		partition_keep_lower A h l left r right original prior);
upper_content_result := \A:@ => \h:A => \n:Nat => \original:List A => \parts:Partition A n =>
	parts @(self => partition_result_contents A original n self->
		partition_result_contents A ((List A).cons h original) (Nat.succ n) (partitionUpper A h n self))
	@parts l left r right lb rb => (\prior:partition_contents A original l left r right =>
		partition_keep_upper A h l left r right original prior);
decision_content_result := \A:@ => \h:A => \n:Nat => \original:List A =>
	\parts:Partition A n => \prior:partition_result_contents A original n parts => \d:Bool =>
	d @(self => partition_result_contents A ((List A).cons h original) (Nat.succ n)
		(partitionByDecision A h n self parts))
	@true => lower_content_result A h n original parts prior
	@false => upper_content_result A h n original parts prior;
partition_content_result := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat =>
	\input:SizedList A n => \original:List A => \representation:sized_contents A n input original =>
	representation @(size values source self =>
		partition_result_contents A source size (partition A &le pivot size values))
	@nil => (partition_contents A (List A).nil Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil).parts
		(List A).nil (List A).nil (sized_contents A).nil (sized_contents A).nil (permutation A).nil
	@cons k h t xs rest => decision_content_result A h k xs (partition A &le pivot k t) *rest (le h pivot);
partition_content_result :: (A:@)->(le:A->A->Bool)->(pivot:A)->(n:Nat)->(input:SizedList A n)->
	(original:List A)->sized_contents A n input original->
	partition_result_contents A original n (partition A &le pivot n input);

quick_join_result := \A:@ => \le:A->A->Bool => \k:Nat => \pivot:A =>
	\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m => \parts:Partition A k =>
	parts @parts l lower r upper lb rb =>
		append A (quickSortAcc A &le l (down l lb) lower)
			((List A).cons pivot (quickSortAcc A &le r (down r rb) upper));
join_content_result := \A:@ => \le:A->A->Bool => \k:Nat => \pivot:A => \original:List A =>
	\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
	\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(input:SizedList A m)->(source:List A)->
		sized_contents A m input source->permutation A source (quickSortAcc A &le m (down m bound) input) =>
	\parts:Partition A k => parts @(self => partition_result_contents A original k self->
		permutation A ((List A).cons pivot original) (quick_join_result A &le k pivot &down self))
	@parts l lower r upper lb rb => (\prior:partition_contents A original l lower r upper =>
		quick_partition_content A pivot original l lower r upper
			(quickSortAcc A &le l (down l lb) lower) (quickSortAcc A &le r (down r rb) upper)
			(append A (quickSortAcc A &le l (down l lb) lower)
				((List A).cons pivot (quickSortAcc A &le r (down r rb) upper)))
			(append_graph A (quickSortAcc A &le l (down l lb) lower)
				((List A).cons pivot (quickSortAcc A &le r (down r rb) upper)))
			&(ih l lb lower) &(ih r rb upper) prior);

quick_content_step := \A:@ => \le:A->A->Bool => \n:Nat => \input:SizedList A n =>
	\original:List A => \representation:sized_contents A n input original => representation
	@(size values source self => (down:(m:Nat)->LT m size->Acc Nat LT m)->
		(ih:(m:Nat)->(bound:LT m size)->(xs:SizedList A m)->(ys:List A)->
			sized_contents A m xs ys->permutation A ys (quickSortAcc A &le m (down m bound) xs))->
		permutation A source (quickSortAcc A &le size ((Acc Nat LT).acc size &down) values))
	@nil => (\down:(m:Nat)->LT m Nat.zero->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m Nat.zero)->(xs:SizedList A m)->(ys:List A)->
			sized_contents A m xs ys->permutation A ys (quickSortAcc A &le m (down m bound) xs) =>
		(permutation A).nil)
	@cons k h t xs rest => (\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(values:SizedList A m)->(source:List A)->
			sized_contents A m values source->permutation A source (quickSortAcc A &le m (down m bound) values) =>
		join_content_result A &le k h xs &down &ih (partition A &le h k t)
			(partition_content_result A &le h k t xs rest));
quick_sized_content_result := \A:@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n =>
	access @(size self => (input:SizedList A size)->(original:List A)->sized_contents A size input original->
		permutation A original (quickSortAcc A &le size self input))
	@acc size down => (\input:SizedList A size => \original:List A => \representation:sized_contents A size input original =>
		quick_content_step A &le size input original representation &down &*down);
quick_sized_content_result :: (A:@)->(le:A->A->Bool)->(n:Nat)->(access:Acc Nat LT n)->
	(input:SizedList A n)->(original:List A)->sized_contents A n input original->
	permutation A original (quickSortAcc A &le n access input);

measure_content_cons := \A:@ => \h:A => \t:List A => \output:Measured A =>
	\prior:measurement_contents A t output => prior
	@(value self => measurement_contents A ((List A).cons h t)
		(value @measured size values => (Measured A).measured (Nat.succ size) ((SizedList A).cons size h values)))
	@measured size values representation => (measurement_contents A ((List A).cons h t)).measured
		(Nat.succ size) ((SizedList A).cons size h values) ((sized_contents A).cons size h values t representation);
measure_content_result := \A:@ => \xs:List A => xs
	@(self => measurement_contents A self (measure A self))
	@nil => (measurement_contents A (List A).nil).measured Nat.zero (SizedList A).nil (sized_contents A).nil
	@cons h t => measure_content_cons A h t (measure A t) *t;
measure_content_result :: (A:@)->(xs:List A)->measurement_contents A xs (measure A xs);
quick_measured_content_result := \A:@ => \le:A->A->Bool => \xs:List A => \output:Measured A =>
	\representation:measurement_contents A xs output => representation
	@(value self => permutation A xs
		(value @measured size values => quickSortAcc A &le size (natAccessible size) values))
	@measured size values representation =>
		quick_sized_content_result A &le size (natAccessible size) values xs representation;
quick_content_result := \A:@ => \le:A->A->Bool => \xs:List A =>
	quick_measured_content_result A &le xs (measure A xs) (measure_content_result A xs);
quick_content_result :: (A:@)->(le:A->A->Bool)->(xs:List A)->permutation A xs (quickSort A &le xs);
