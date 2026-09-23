import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Acc;
import LT;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
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
yes_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.true => d @yes p => p;
no_order := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \x:A => \y:A => \d:general_decision A R x y Bool.false => d @no p => p;
all_trans := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \y:A => \xs:List A => \bound:(general_all_from A R) y xs => bound
	@nil => (\x:A => \xy:R x y => ((general_all_from A R) x).nil)
	@cons head tail yh yt => (\x:A => \xy:R x y =>
		((general_all_from A R) x).cons head tail (trans x y xy head yh) (*yt x xy));
all_trans :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(y:A)->(xs:List A)->(general_all_from A R) y xs->(x:A)->R x y->(general_all_from A R) x xs;
tail_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => rest;
tail_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_sorted A R) tail;
tail_bound := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \proof:(general_sorted A R) ((List A).cons head tail) => proof
	@cons h t bound rest => bound;
tail_bound :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(head:A)->(tail:List A)->(general_sorted A R) ((List A).cons head tail)->(general_all_from A R) head tail;
all_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => bound;
all_head :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->R lo head;
all_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \head:A => \tail:List A =>
	\proof:(general_all_from A R) lo ((List A).cons head tail) => proof
		@cons h t bound rest => rest;
all_tail :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(lo:A)->(head:A)->(tail:List A)->(general_all_from A R) lo ((List A).cons head tail)->(general_all_from A R) lo tail;
AllTo := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(head:A)->(tail:List A)->R head hi->* tail->* ((List A).cons head tail);
};
to_head := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => bound;
to_tail := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \head:A => \tail:List A => \p:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) =>
	p @cons h t bound rest => rest;
append_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\lo:A => \left:(general_all_from A R) lo (List A).nil => \right:(general_all_from A R) lo following => right)
	@case1 head tail following output rest => (\lo:A =>
		\left:(general_all_from A R) lo ((List A).cons head tail) => \right:(general_all_from A R) lo following =>
		((general_all_from A R) lo).cons head output ((all_head A le R trans le_refl decide) lo head tail left) (*rest lo ((all_tail A le R trans le_refl decide) lo head tail left) right));
append_lower :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(lo:A)->(general_all_from A R) lo xs->(general_all_from A R) lo ys->(general_all_from A R) lo zs;
append_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\hi:A => \left:(AllTo A le R trans le_refl decide) hi (List A).nil => \right:(AllTo A le R trans le_refl decide) hi following => right)
	@case1 head tail following output rest => (\hi:A =>
		\left:(AllTo A le R trans le_refl decide) hi ((List A).cons head tail) => \right:(AllTo A le R trans le_refl decide) hi following =>
		((AllTo A le R trans le_refl decide) hi).cons head output ((to_head A le R trans le_refl decide) hi head tail left) (*rest hi ((to_tail A le R trans le_refl decide) hi head tail left) right));
append_upper :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(hi:A)->(AllTo A le R trans le_refl decide) hi xs->(AllTo A le R trans le_refl decide) hi ys->(AllTo A le R trans le_refl decide) hi zs;
append_sorted_step := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \head:A => \tail:List A => \following:List A => \output:List A =>
	\rest:@append A tail following output =>
	\pivot:A => \ih:(general_sorted A R) tail->(AllTo A le R trans le_refl decide) pivot tail->(general_sorted A R) following->(general_all_from A R) pivot following->(general_sorted A R) output =>
	\sl:(general_sorted A R) ((List A).cons head tail) =>
		\ul:(AllTo A le R trans le_refl decide) pivot ((List A).cons head tail) => \sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following =>
		(general_sorted A R).cons head output
			((append_lower A le R trans le_refl decide) tail following output rest head ((tail_bound A le R trans le_refl decide) head tail sl)
				((all_trans A le R trans le_refl decide) pivot following lr head ((to_head A le R trans le_refl decide) pivot head tail ul)))
			(ih ((tail_sorted A le R trans le_refl decide) head tail sl) ((to_tail A le R trans le_refl decide) pivot head tail ul) sr lr);
append_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => (\sl:(general_sorted A R) (List A).nil => \ul:(AllTo A le R trans le_refl decide) pivot (List A).nil =>
		\sr:(general_sorted A R) following => \lr:(general_all_from A R) pivot following => sr)
	@case1 head tail following output rest => (append_sorted_step A le R trans le_refl decide) head tail following output rest pivot &*rest;
append_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	(general_sorted A R) xs->(AllTo A le R trans le_refl decide) pivot xs->(general_sorted A R) ys->(general_all_from A R) pivot ys->(general_sorted A R) zs;
All := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->P h->* t->* ((List A).cons h t);
};
SizedAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(n:Nat)->(h:A)->(t:SizedList A n)->P h->* n t->
		* (Nat.succ n) ((SizedList A).cons n h t);
};
PartAll := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) P l left->(SizedAll A le R trans le_refl decide) P r right->* ((Partition A n).parts l left r right lb rb);
};
all_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => head;
all_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \t:List A => \p:(All A le R trans le_refl decide) P ((List A).cons h t) => p
	@cons a b head tail => tail;
sized_first := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => head;
sized_rest := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \h:A => \t:SizedList A n =>
	\p:(SizedAll A le R trans le_refl decide) P (Nat.succ n) ((SizedList A).cons n h t) => p @cons k a b head tail => tail;
part_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
part_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
append_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 right => (\pl:(All A le R trans le_refl decide) P (List A).nil => \pr:(All A le R trans le_refl decide) P right => pr)
	@case1 h t right output rest => (\pl:(All A le R trans le_refl decide) P ((List A).cons h t) => \pr:(All A le R trans le_refl decide) P right =>
		((All A le R trans le_refl decide) P).cons h output ((all_first A le R trans le_refl decide) P h t pl) (*rest ((all_rest A le R trans le_refl decide) P h t pl) pr));
append_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(xs:List A)->(ys:List A)->(zs:List A)->
	@append A xs ys zs->(All A le R trans le_refl decide) P xs->(All A le R trans le_refl decide) P ys->(All A le R trans le_refl decide) P zs;
lower_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionLower A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			(((SizedAll A le R trans le_refl decide) P).cons l h left ph ((part_left A le R trans le_refl decide) P n l left r right lb rb prior))
			((part_right A le R trans le_refl decide) P n l left r right lb rb prior));
lower_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionLower A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
upper_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionUpper A h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:(PartAll A le R trans le_refl decide) P n ((Partition A n).parts l left r right lb rb) =>
		((PartAll A le R trans le_refl decide) P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			((part_left A le R trans le_refl decide) P n l left r right lb rb prior)
			(((SizedAll A le R trans le_refl decide) P).cons r h right ph ((part_right A le R trans le_refl decide) P n l left r right lb rb prior)));
upper_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionUpper A h n input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
decision_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \h:A => \n:Nat => \d:Bool =>
	\input:Partition A n => \output:Partition A (Nat.succ n) =>
	\g:@partitionByDecision A h n d input output => g
	@case0 result trace => (lower_all A le R trans le_refl decide) P h n input result trace
	@case1 result trace => (upper_all A le R trans le_refl decide) P h n input result trace;
decision_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(h:A)->(n:Nat)->(d:Bool)->
	(input:Partition A n)->(output:Partition A (Nat.succ n))->
	@partitionByDecision A h n d input output->P h->(PartAll A le R trans le_refl decide) P n input->(PartAll A le R trans le_refl decide) P (Nat.succ n) output;
partition_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil =>
		((PartAll A le R trans le_refl decide) P Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
			(LT.step Nat.zero) (LT.step Nat.zero) ((SizedAll A le R trans le_refl decide) P).nil ((SizedAll A le R trans le_refl decide) P).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
				(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
				(((SizedAll A le R trans le_refl decide) P).cons l h left ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
				((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior))))
	@case2 k h t comparison l left r right lb rb rest =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k h t) =>
			((PartAll A le R trans le_refl decide) P (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
				(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
				((part_left A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))
				(((SizedAll A le R trans le_refl decide) P).cons r h right ((sized_first A le R trans le_refl decide) P k h t prior)
					((part_right A le R trans le_refl decide) P k l left r right lb rb (*rest ((sized_rest A le R trans le_refl decide) P k h t prior)))));
partition_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(SizedAll A le R trans le_refl decide) P n xs->(PartAll A le R trans le_refl decide) P n output;
PartOrdered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => @\parts:Partition A n => {
	parts:(l:Nat)->(left:SizedList A l)->(r:Nat)->(right:SizedList A r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		(SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot)) l left->(SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h)) r right->
		* ((Partition A n).parts l left r right lb rb);
};
ordered_left := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
ordered_right := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:(PartOrdered A le R trans le_refl decide) pivot n ((Partition A n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
partition_ordered := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \n:Nat => \xs:SizedList A n => \output:Partition A n =>
	\g:@partition A (&le) pivot n xs output => g
	@case0 => ((PartOrdered A le R trans le_refl decide) pivot Nat.zero).parts Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil
		(LT.step Nat.zero) (LT.step Nat.zero)
		((SizedAll A le R trans le_refl decide) (&(\h:A => R h pivot))).nil ((SizedAll A le R trans le_refl decide) (&(\h:A => R pivot h))).nil
	@case1 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts (Nat.succ l) ((SizedList A).cons l h left) r right
			(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R x pivot))).cons l h left
				((yes_order A le R trans le_refl decide) h pivot (comparison @case0 bound => decide h bound))
				((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest))
			((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest)
	@case2 k h t comparison l left r right lb rb rest =>
		((PartOrdered A le R trans le_refl decide) pivot (Nat.succ k)).parts l left (Nat.succ r) ((SizedList A).cons r h right)
			(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
			((ordered_left A le R trans le_refl decide) pivot k l left r right lb rb *rest)
			(((SizedAll A le R trans le_refl decide) (&(\x:A => R pivot x))).cons r h right
				((no_order A le R trans le_refl decide) h pivot (comparison @case0 bound => decide h bound))
				((ordered_right A le R trans le_refl decide) pivot k l left r right lb rb *rest));
partition_ordered :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(pivot:A)->(n:Nat)->(xs:SizedList A n)->(output:Partition A n)->
	@partition A (&le) pivot n xs output->(PartOrdered A le R trans le_refl decide) pivot n output;
quick_all := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \P:A->@ => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (\prior:(SizedAll A le R trans le_refl decide) P Nat.zero (SizedList A).nil => ((All A le R trans le_refl decide) P).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(\prior:(SizedAll A le R trans le_refl decide) P (Nat.succ k) ((SizedList A).cons k pivot tail) =>
			(append_all A le R trans le_refl decide) P left ((List A).cons pivot right) result appending
				(*leftGraph ((part_left A le R trans le_refl decide) P k l lower r upper lb rb
					((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
						partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))
				(((All A le R trans le_refl decide) P).cons pivot right ((sized_first A le R trans le_refl decide) P k pivot tail prior)
					(*rightGraph ((part_right A le R trans le_refl decide) P k l lower r upper lb rb
						((partition_all A le R trans le_refl decide) P pivot k tail ((Partition A k).parts l lower r upper lb rb)
							partitioning ((sized_rest A le R trans le_refl decide) P k pivot tail prior))))));
quick_all :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(P:A->@)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(SizedAll A le R trans le_refl decide) P n input->(All A le R trans le_refl decide) P output;
to_upper := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \hi:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R x hi)) xs => p
	@nil => ((AllTo A le R trans le_refl decide) hi).nil
	@cons h t bound rest => ((AllTo A le R trans le_refl decide) hi).cons h t bound *rest;
to_lower := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \lo:A => \xs:List A => \p:(All A le R trans le_refl decide) (&(\x:A => R lo x)) xs => p
	@nil => ((general_all_from A R) lo).nil
	@cons h t bound rest => ((general_all_from A R) lo).cons h t bound *rest;
join_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \pivot:A => \left:List A => \right:List A => \output:List A =>
	\appending:@append A left ((List A).cons pivot right) output =>
	\sl:(general_sorted A R) left => \ul:(AllTo A le R trans le_refl decide) pivot left => \sr:(general_sorted A R) right => \lr:(general_all_from A R) pivot right =>
	(append_sorted A le R trans le_refl decide) pivot left ((List A).cons pivot right) output appending sl ul
		((general_sorted A R).cons pivot right lr sr) (((general_all_from A R) pivot).cons pivot right (le_refl pivot) lr);
quick_sorted := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat => \access:Acc Nat LT n => \input:SizedList A n => \output:List A =>
	\g:@quickSortAcc A (&le) n access input output => g
	@case0 down => (general_sorted A R).nil
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(join_sorted A le R trans le_refl decide) pivot left right result appending *leftGraph
			((to_upper A le R trans le_refl decide) pivot left ((quick_all A le R trans le_refl decide) (&(\x:A => R x pivot)) l (down l lb) lower left leftGraph
				((ordered_left A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))))
			*rightGraph
			((to_lower A le R trans le_refl decide) pivot right ((quick_all A le R trans le_refl decide) (&(\x:A => R pivot x)) r (down r rb) upper right rightGraph
				((ordered_right A le R trans le_refl decide) pivot k l lower r upper lb rb
					((partition_ordered A le R trans le_refl decide) pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning))));
quick_sorted :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList A n)->(output:List A)->
	@quickSortAcc A (&le) n access input output->(general_sorted A R) output;
quick_correct := \A:@ => \le:A->A->Bool => \R:A->A->@ => \trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \le_refl:(x:A)->R x x => \decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \output:List A => \g:@quickSort A (&le) xs output => g
	@case0 original n values measurement access accessibility sorted sorting => (quick_sorted A le R trans le_refl decide) n access values sorted sorting;
quick_correct :: (A:@)->(le:A->A->Bool)->(R:A->A->@)->(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(le_refl:(x:A)->R x x)->(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(output:List A)->@quickSort A (&le) xs output->(general_sorted A R) output;
