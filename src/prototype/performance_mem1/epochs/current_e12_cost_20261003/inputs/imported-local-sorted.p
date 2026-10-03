import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Measured;
import Acc;
import LT;
import measure;
import natAccessible;
import partition;
import quickSortAcc;
import quickSort;
import append;
import general_decision;
import general_locally_sorted;
import sorted_local_prepend;
import qsr_All;
import qsr_PartOrdered;
import qsr_partition_ordered;
import qsr_all_head;
import qsr_all_tail;
import qsr_all_connection;
import qsr_decision_connection;
import qsr_join_values;
import qsr_quick_all;

qsl_prepend := \A:@ => \R:A->A->@ => \pivot:A => \ys:List A =>
	\bound:qsr_All A (&(\y:A => R pivot y)) ys => \local:general_locally_sorted A R ys =>
	sorted_local_prepend A R pivot ys (qsr_all_connection A R pivot ys bound) local;

// The pivot connects the two chains; no lower-to-upper relation is constructed.
qsl_append := \A:@ => \R:A->A->@ => \pivot:A => \xs:List A =>
	\local:general_locally_sorted A R xs => local
	@(values self => (ys:List A)->qsr_All A (&(\x:A => R x pivot)) values->
		general_locally_sorted A R ys->qsr_All A (&(\y:A => R pivot y)) ys->
		general_locally_sorted A R (append A values ((List A).cons pivot ys)))
	@nil => (\ys:List A => \lower:qsr_All A (&(\x:A => R x pivot)) (List A).nil =>
		\upper:general_locally_sorted A R ys => \bound:qsr_All A (&(\y:A => R pivot y)) ys =>
		qsl_prepend A R pivot ys bound upper)
	@one x => (\ys:List A => \lower:qsr_All A (&(\x:A => R x pivot)) ((List A).cons x (List A).nil) =>
		\upper:general_locally_sorted A R ys => \bound:qsr_All A (&(\y:A => R pivot y)) ys =>
		(general_locally_sorted A R).cons x pivot ys
			(qsr_all_head A (&(\x:A => R x pivot)) x (List A).nil lower) (qsl_prepend A R pivot ys bound upper))
	@cons x y tail edge rest => (\ys:List A =>
		\lower:qsr_All A (&(\x:A => R x pivot)) ((List A).cons x ((List A).cons y tail)) =>
		\upper:general_locally_sorted A R ys => \bound:qsr_All A (&(\y:A => R pivot y)) ys =>
		(general_locally_sorted A R).cons x y (append A tail ((List A).cons pivot ys)) edge
			(*rest ys (qsr_all_tail A (&(\x:A => R x pivot)) x ((List A).cons y tail) lower) upper bound));
qsl_append :: (A:@)->(R:A->A->@)->(pivot:A)->(xs:List A)->general_locally_sorted A R xs->
	(ys:List A)->qsr_All A (&(\x:A => R x pivot)) xs->general_locally_sorted A R ys->
	qsr_All A (&(\y:A => R pivot y)) ys->general_locally_sorted A R (append A xs ((List A).cons pivot ys));

qsl_join := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\k:Nat => \pivot:A => \down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
	\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(xs:SizedList A m)->general_locally_sorted A R (quickSortAcc A (&le) m (down m bound) xs) =>
	\parts:Partition A k => \ordered:qsr_PartOrdered A R pivot k parts =>
	ordered @(value self => general_locally_sorted A R (qsr_join_values A (&le) k pivot (&down) value))
	@parts l lower r upper lb rb pl pr =>
		qsl_append A R pivot (quickSortAcc A (&le) l (down l lb) lower) (ih l lb lower)
			(quickSortAcc A (&le) r (down r rb) upper)
			(qsr_quick_all A (&(\x:A => R x pivot)) (&le) l (down l lb) lower pl)
			(ih r rb upper) (qsr_quick_all A (&(\x:A => R pivot x)) (&le) r (down r rb) upper pr);

qsl_step := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat => \xs:SizedList A n =>
	xs @(size self => (down:(m:Nat)->LT m size->Acc Nat LT m)->
		(ih:(m:Nat)->(bound:LT m size)->(values:SizedList A m)->general_locally_sorted A R (quickSortAcc A (&le) m (down m bound) values))->
		general_locally_sorted A R (quickSortAcc A (&le) size ((Acc Nat LT).acc size (&down)) self))
	@nil => (\down:(m:Nat)->LT m Nat.zero->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m Nat.zero)->(values:SizedList A m)->general_locally_sorted A R (quickSortAcc A (&le) m (down m bound) values) =>
		(general_locally_sorted A R).nil)
	@cons k h t => (\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(values:SizedList A m)->general_locally_sorted A R (quickSortAcc A (&le) m (down m bound) values) =>
		qsl_join A R (&le) k h (&down) (&ih) (partition A (&le) h k t)
			(qsr_partition_ordered A R (&le)
				(&(\x:A => \y:A => qsr_decision_connection A R x y (le x y) (decide x y))) h k t));
qsl_acc := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat => \access:Acc Nat LT n =>
	access @(size self => (xs:SizedList A size)->general_locally_sorted A R (quickSortAcc A (&le) size self xs))
	@acc size down => (\xs:SizedList A size => qsl_step A R (&le) decide size xs (&down) &*down);
qsl_acc :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(n:Nat)->(access:Acc Nat LT n)->(xs:SizedList A n)->
	general_locally_sorted A R (quickSortAcc A (&le) n access xs);

quick_locally_sorted := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	(measure A xs) @(self => general_locally_sorted A R (self @measured size values => quickSortAcc A (&le) size (natAccessible size) values))
	@measured size values => qsl_acc A R (&le) decide size (natAccessible size) values;
quick_locally_sorted :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->
	general_locally_sorted A R (quickSort A (&le) xs);
