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
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
import general_all_from;
import general_sorted;
import general_decision;

import qsr_All;
import qsr_Decision;
import qsr_PartOrdered;
import qsr_partition_ordered;
import qsr_all_head;
import qsr_all_tail;
import qsr_append_all;
import qsr_join_values;
import qsr_quick_all;

qsr_Sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->qsr_All A (&(\x:A => R h x)) t->* t->* ((List A).cons h t);
};
qsr_sorted_tail := \A:@ => \R:A->A->@ => \h:A => \t:List A => \p:qsr_Sorted A R ((List A).cons h t) =>
	p @cons head tail bound rest => rest;
qsr_sorted_bound := \A:@ => \R:A->A->@ => \h:A => \t:List A => \p:qsr_Sorted A R ((List A).cons h t) =>
	p @cons head tail bound rest => bound;
qsr_all_trans := \A:@ => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\y:A => \xs:List A => \p:qsr_All A (&(\z:A => R y z)) xs => p
	@nil => (\x:A => \xy:R x y => (qsr_All A (&(\z:A => R x z))).nil)
	@cons h t ph pt => (\x:A => \xy:R x y =>
		(qsr_All A (&(\z:A => R x z))).cons h t (trans x y xy h ph) (*pt x xy));
qsr_all_trans :: (A:@)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(y:A)->(xs:List A)->qsr_All A (&(\z:A => R y z)) xs->(x:A)->R x y->qsr_All A (&(\z:A => R x z)) xs;
qsr_append_sorted := \A:@ => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\pivot:A => \xs:List A =>
	xs @(self => (ys:List A)->qsr_Sorted A R self->qsr_All A (&(\x:A => R x pivot)) self->
		qsr_Sorted A R ys->qsr_All A (&(\x:A => R pivot x)) ys->qsr_Sorted A R (append A self ys))
	@nil => (\ys:List A => \sl:qsr_Sorted A R (List A).nil => \ul:qsr_All A (&(\x:A => R x pivot)) (List A).nil =>
		\sr:qsr_Sorted A R ys => \lr:qsr_All A (&(\x:A => R pivot x)) ys => sr)
	@cons h t => (\ys:List A => \sl:qsr_Sorted A R ((List A).cons h t) =>
		\ul:qsr_All A (&(\x:A => R x pivot)) ((List A).cons h t) =>
		\sr:qsr_Sorted A R ys => \lr:qsr_All A (&(\x:A => R pivot x)) ys =>
		(qsr_Sorted A R).cons h (append A t ys)
			(qsr_append_all A (&(\x:A => R h x)) t ys (qsr_sorted_bound A R h t sl)
				(qsr_all_trans A R trans pivot ys lr h (qsr_all_head A (&(\x:A => R x pivot)) h t ul)))
			(*t ys (qsr_sorted_tail A R h t sl) (qsr_all_tail A (&(\x:A => R x pivot)) h t ul) sr lr));
qsr_append_sorted :: (A:@)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(pivot:A)->(xs:List A)->(ys:List A)->qsr_Sorted A R xs->qsr_All A (&(\x:A => R x pivot)) xs->
	qsr_Sorted A R ys->qsr_All A (&(\x:A => R pivot x)) ys->qsr_Sorted A R (append A xs ys);

qsr_join_sorted := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\k:Nat => \pivot:A => \down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
	\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(xs:SizedList A m)->qsr_Sorted A R (quickSortAcc A (&le) m (down m bound) xs) =>
	\parts:Partition A k => \ordered:qsr_PartOrdered A R pivot k parts =>
	ordered @(value self => qsr_Sorted A R (qsr_join_values A (&le) k pivot (&down) value))
	@parts l lower r upper lb rb pl pr =>
		qsr_append_sorted A R trans pivot (quickSortAcc A (&le) l (down l lb) lower)
			((List A).cons pivot (quickSortAcc A (&le) r (down r rb) upper))
			(ih l lb lower) (qsr_quick_all A (&(\x:A => R x pivot)) (&le) l (down l lb) lower pl)
			((qsr_Sorted A R).cons pivot (quickSortAcc A (&le) r (down r rb) upper)
				(qsr_quick_all A (&(\x:A => R pivot x)) (&le) r (down r rb) upper pr) (ih r rb upper))
			((qsr_All A (&(\x:A => R pivot x))).cons pivot (quickSortAcc A (&le) r (down r rb) upper)
				(refl pivot) (qsr_quick_all A (&(\x:A => R pivot x)) (&le) r (down r rb) upper pr));

qsr_quick_sorted_step := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y) => \n:Nat => \xs:SizedList A n =>
	xs @(size self => (down:(m:Nat)->LT m size->Acc Nat LT m)->
		(ih:(m:Nat)->(bound:LT m size)->(values:SizedList A m)->qsr_Sorted A R (quickSortAcc A (&le) m (down m bound) values))->
		qsr_Sorted A R (quickSortAcc A (&le) size ((Acc Nat LT).acc size (&down)) self))
	@nil => (\down:(m:Nat)->LT m Nat.zero->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m Nat.zero)->(values:SizedList A m)->qsr_Sorted A R (quickSortAcc A (&le) m (down m bound) values) =>
		(qsr_Sorted A R).nil)
	@cons k h t => (\down:(m:Nat)->LT m (Nat.succ k)->Acc Nat LT m =>
		\ih:(m:Nat)->(bound:LT m (Nat.succ k))->(values:SizedList A m)->qsr_Sorted A R (quickSortAcc A (&le) m (down m bound) values) =>
		qsr_join_sorted A R (&le) trans refl k h (&down) (&ih) (partition A (&le) h k t)
			(qsr_partition_ordered A R (&le) decide h k t));
qsr_quick_sorted := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y) => \n:Nat => \access:Acc Nat LT n =>
	access @(size self => (xs:SizedList A size)->qsr_Sorted A R (quickSortAcc A (&le) size self xs))
	@acc size down => (\xs:SizedList A size => qsr_quick_sorted_step A R (&le) trans refl decide size xs (&down) &*down);
qsr_quick_sorted :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(refl:(x:A)->R x x)->
	(decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y))->(n:Nat)->(access:Acc Nat LT n)->(xs:SizedList A n)->
	qsr_Sorted A R (quickSortAcc A (&le) n access xs);

qsr_quick_correct := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y) => \xs:List A =>
	(measure A xs) @(self => qsr_Sorted A R (self @measured size values => quickSortAcc A (&le) size (natAccessible size) values))
	@measured size values => qsr_quick_sorted A R (&le) trans refl decide size (natAccessible size) values;
qsr_quick_correct :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(refl:(x:A)->R x x)->
	(decide:(x:A)->(y:A)->qsr_Decision A R x y (le x y))->(xs:List A)->qsr_Sorted A R (quickSort A (&le) xs);

// Connect to the exact nominal predicates also used by the graph theorem.
import qsr_all_connection;
qsr_sorted_connection := \A:@ => \R:A->A->@ => \xs:List A => \p:qsr_Sorted A R xs => p
	@nil => (general_sorted A R).nil
	@cons h t ph pt => (general_sorted A R).cons h t (qsr_all_connection A R h t ph) *pt;
qsr_sorted_connection :: (A:@)->(R:A->A->@)->(xs:List A)->qsr_Sorted A R xs->general_sorted A R xs;
import qsr_decision_connection;
quick_correct_existing := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	qsr_sorted_connection A R (quickSort A (&le) xs)
		(qsr_quick_correct A R (&le) trans refl
			(&(\x:A => \y:A => qsr_decision_connection A R x y (le x y) (decide x y))) xs);
quick_correct_existing :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(refl:(x:A)->R x x)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->general_sorted A R (quickSort A (&le) xs);
