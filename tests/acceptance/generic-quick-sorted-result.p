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
qsr_all_connection := \A:@ => \R:A->A->@ => \head:A => \xs:List A =>
	\p:qsr_All A (&(\x:A => R head x)) xs => p
	@nil => (general_all_from A R head).nil
	@cons h t ph pt => (general_all_from A R head).cons h t ph *pt;
qsr_all_connection :: (A:@)->(R:A->A->@)->(head:A)->(xs:List A)->
	qsr_All A (&(\x:A => R head x)) xs->general_all_from A R head xs;
qsr_sorted_connection := \A:@ => \R:A->A->@ => \xs:List A => \p:qsr_Sorted A R xs => p
	@nil => (general_sorted A R).nil
	@cons h t ph pt => (general_sorted A R).cons h t (qsr_all_connection A R h t ph) *pt;
qsr_sorted_connection :: (A:@)->(R:A->A->@)->(xs:List A)->qsr_Sorted A R xs->general_sorted A R xs;
qsr_decision_connection := \A:@ => \R:A->A->@ => \x:A => \y:A => \answer:Bool =>
	\p:general_decision A R x y answer => p
	@yes proof => (qsr_Decision A R x y).yes proof
	@no proof => (qsr_Decision A R x y).no proof;
qsr_decision_connection :: (A:@)->(R:A->A->@)->(x:A)->(y:A)->(answer:Bool)->
	general_decision A R x y answer->qsr_Decision A R x y answer;
quick_correct_existing := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \refl:(x:A)->R x x =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	qsr_sorted_connection A R (quickSort A (&le) xs)
		(qsr_quick_correct A R (&le) trans refl
			(&(\x:A => \y:A => qsr_decision_connection A R x y (le x y) (decide x y))) xs);
quick_correct_existing :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(refl:(x:A)->R x x)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->general_sorted A R (quickSort A (&le) xs);
