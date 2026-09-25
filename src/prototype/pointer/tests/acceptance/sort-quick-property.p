// Generic element-property preservation for the unchanged PR #30 QuickSort.
import Nat;
import Bool;
import List;
import SizedList;
import Partition;
import Measured;
import Acc;
import LT;
import LE;
import Sorted;
import AllFrom;
import AllTo;
import natLessOrEqual;
import partitionLower;
import partitionUpper;
import partitionByDecision;
import partition;
import quickSortAcc;
import quickSort;
import append;
import append_sorted;
import le_refl;
import direct;
import unwrap;
import yes_order;
import no_order;
import trans;
import le_next;
All := \P:Nat->@ => @\xs:List Nat => {
	nil:* (List Nat).nil;
	cons:(h:Nat)->(t:List Nat)->P h->* t->* ((List Nat).cons h t);
};
SizedAll := \P:Nat->@ => @\n:Nat => @\xs:SizedList Nat n => {
	nil:* Nat.zero (SizedList Nat).nil;
	cons:(n:Nat)->(h:Nat)->(t:SizedList Nat n)->P h->* n t->
		* (Nat.succ n) ((SizedList Nat).cons n h t);
};
PartAll := \P:Nat->@ => \n:Nat => @\parts:Partition Nat n => {
	parts:(l:Nat)->(left:SizedList Nat l)->(r:Nat)->(right:SizedList Nat r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		SizedAll P l left->SizedAll P r right->* ((Partition Nat n).parts l left r right lb rb);
};
all_first := \P:Nat->@ => \h:Nat => \t:List Nat => \p:All P ((List Nat).cons h t) => p
	@cons a b head tail => head;
all_rest := \P:Nat->@ => \h:Nat => \t:List Nat => \p:All P ((List Nat).cons h t) => p
	@cons a b head tail => tail;
sized_first := \P:Nat->@ => \n:Nat => \h:Nat => \t:SizedList Nat n =>
	\p:SizedAll P (Nat.succ n) ((SizedList Nat).cons n h t) => p @cons k a b head tail => head;
sized_rest := \P:Nat->@ => \n:Nat => \h:Nat => \t:SizedList Nat n =>
	\p:SizedAll P (Nat.succ n) ((SizedList Nat).cons n h t) => p @cons k a b head tail => tail;
part_left := \P:Nat->@ => \n:Nat => \l:Nat => \left:SizedList Nat l =>
	\r:Nat => \right:SizedList Nat r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:PartAll P n ((Partition Nat n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
part_right := \P:Nat->@ => \n:Nat => \l:Nat => \left:SizedList Nat l =>
	\r:Nat => \right:SizedList Nat r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:PartAll P n ((Partition Nat n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
append_all := \P:Nat->@ => \xs:List Nat => \ys:List Nat => \zs:List Nat => \g:@append Nat xs ys zs => g
	@case0 right => (\pl:All P (List Nat).nil => \pr:All P right => pr)
	@case1 h t right output rest => (\pl:All P ((List Nat).cons h t) => \pr:All P right =>
		(All P).cons h output (all_first P h t pl) (*rest (all_rest P h t pl) pr));
append_all :: (P:Nat->@)->(xs:List Nat)->(ys:List Nat)->(zs:List Nat)->
	@append Nat xs ys zs->All P xs->All P ys->All P zs;
lower_all := \P:Nat->@ => \h:Nat => \n:Nat => \input:Partition Nat n => \output:Partition Nat (Nat.succ n) =>
	\g:@partitionLower Nat h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:PartAll P n ((Partition Nat n).parts l left r right lb rb) =>
		(PartAll P (Nat.succ n)).parts (Nat.succ l) ((SizedList Nat).cons l h left) r right
			(LT.lift l (Nat.succ n) lb) (LT.weakenRight r (Nat.succ n) rb)
			((SizedAll P).cons l h left ph (part_left P n l left r right lb rb prior))
			(part_right P n l left r right lb rb prior));
lower_all :: (P:Nat->@)->(h:Nat)->(n:Nat)->(input:Partition Nat n)->(output:Partition Nat (Nat.succ n))->
	@partitionLower Nat h n input output->P h->PartAll P n input->PartAll P (Nat.succ n) output;
upper_all := \P:Nat->@ => \h:Nat => \n:Nat => \input:Partition Nat n => \output:Partition Nat (Nat.succ n) =>
	\g:@partitionUpper Nat h n input output => g
	@case0 l left r right lb rb => (\ph:P h => \prior:PartAll P n ((Partition Nat n).parts l left r right lb rb) =>
		(PartAll P (Nat.succ n)).parts l left (Nat.succ r) ((SizedList Nat).cons r h right)
			(LT.weakenRight l (Nat.succ n) lb) (LT.lift r (Nat.succ n) rb)
			(part_left P n l left r right lb rb prior)
			((SizedAll P).cons r h right ph (part_right P n l left r right lb rb prior)));
upper_all :: (P:Nat->@)->(h:Nat)->(n:Nat)->(input:Partition Nat n)->(output:Partition Nat (Nat.succ n))->
	@partitionUpper Nat h n input output->P h->PartAll P n input->PartAll P (Nat.succ n) output;
decision_all := \P:Nat->@ => \h:Nat => \n:Nat => \d:Bool =>
	\input:Partition Nat n => \output:Partition Nat (Nat.succ n) =>
	\g:@partitionByDecision Nat h n d input output => g
	@case0 result trace => lower_all P h n input result trace
	@case1 result trace => upper_all P h n input result trace;
decision_all :: (P:Nat->@)->(h:Nat)->(n:Nat)->(d:Bool)->
	(input:Partition Nat n)->(output:Partition Nat (Nat.succ n))->
	@partitionByDecision Nat h n d input output->P h->PartAll P n input->PartAll P (Nat.succ n) output;
partition_all := \P:Nat->@ => \pivot:Nat => \n:Nat => \xs:SizedList Nat n => \output:Partition Nat n =>
	\g:@partition Nat (&natLessOrEqual) pivot n xs output => g
	@case0 => (\prior:SizedAll P Nat.zero (SizedList Nat).nil =>
		(PartAll P Nat.zero).parts Nat.zero (SizedList Nat).nil Nat.zero (SizedList Nat).nil
			(LT.step Nat.zero) (LT.step Nat.zero) (SizedAll P).nil (SizedAll P).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		(\prior:SizedAll P (Nat.succ k) ((SizedList Nat).cons k h t) =>
			(PartAll P (Nat.succ k)).parts (Nat.succ l) ((SizedList Nat).cons l h left) r right
				(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
				((SizedAll P).cons l h left (sized_first P k h t prior)
					(part_left P k l left r right lb rb (*rest (sized_rest P k h t prior))))
				(part_right P k l left r right lb rb (*rest (sized_rest P k h t prior))))
	@case2 k h t comparison l left r right lb rb rest =>
		(\prior:SizedAll P (Nat.succ k) ((SizedList Nat).cons k h t) =>
			(PartAll P (Nat.succ k)).parts l left (Nat.succ r) ((SizedList Nat).cons r h right)
				(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
				(part_left P k l left r right lb rb (*rest (sized_rest P k h t prior)))
				((SizedAll P).cons r h right (sized_first P k h t prior)
					(part_right P k l left r right lb rb (*rest (sized_rest P k h t prior)))));
partition_all :: (P:Nat->@)->(pivot:Nat)->(n:Nat)->(xs:SizedList Nat n)->(output:Partition Nat n)->
	@partition Nat (&natLessOrEqual) pivot n xs output->SizedAll P n xs->PartAll P n output;
PartOrdered := \pivot:Nat => \n:Nat => @\parts:Partition Nat n => {
	parts:(l:Nat)->(left:SizedList Nat l)->(r:Nat)->(right:SizedList Nat r)->
		(lb:LT l (Nat.succ n))->(rb:LT r (Nat.succ n))->
		SizedAll (&(\h:Nat => LE h pivot)) l left->SizedAll (&(\h:Nat => LE pivot h)) r right->
		* ((Partition Nat n).parts l left r right lb rb);
};
ordered_left := \pivot:Nat => \n:Nat => \l:Nat => \left:SizedList Nat l =>
	\r:Nat => \right:SizedList Nat r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:PartOrdered pivot n ((Partition Nat n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => leftProof;
ordered_right := \pivot:Nat => \n:Nat => \l:Nat => \left:SizedList Nat l =>
	\r:Nat => \right:SizedList Nat r => \lb:LT l (Nat.succ n) => \rb:LT r (Nat.succ n) =>
	\p:PartOrdered pivot n ((Partition Nat n).parts l left r right lb rb) => p
	@parts a av b bv ap bp leftProof rightProof => rightProof;
partition_ordered := \pivot:Nat => \n:Nat => \xs:SizedList Nat n => \output:Partition Nat n =>
	\g:@partition Nat (&natLessOrEqual) pivot n xs output => g
	@case0 => (PartOrdered pivot Nat.zero).parts Nat.zero (SizedList Nat).nil Nat.zero (SizedList Nat).nil
		(LT.step Nat.zero) (LT.step Nat.zero)
		(SizedAll (&(\h:Nat => LE h pivot))).nil (SizedAll (&(\h:Nat => LE pivot h))).nil
	@case1 k h t comparison l left r right lb rb rest =>
		(PartOrdered pivot (Nat.succ k)).parts (Nat.succ l) ((SizedList Nat).cons l h left) r right
			(LT.lift l (Nat.succ k) lb) (LT.weakenRight r (Nat.succ k) rb)
			((SizedAll (&(\x:Nat => LE x pivot))).cons l h left
				(comparison @case0 bound => yes_order h bound (unwrap h bound (direct h bound)))
				(ordered_left pivot k l left r right lb rb *rest))
			(ordered_right pivot k l left r right lb rb *rest)
	@case2 k h t comparison l left r right lb rb rest =>
		(PartOrdered pivot (Nat.succ k)).parts l left (Nat.succ r) ((SizedList Nat).cons r h right)
			(LT.weakenRight l (Nat.succ k) lb) (LT.lift r (Nat.succ k) rb)
			(ordered_left pivot k l left r right lb rb *rest)
			((SizedAll (&(\x:Nat => LE pivot x))).cons r h right
				(comparison @case0 bound => trans bound (Nat.succ bound) (le_next bound) h
					(no_order h bound (unwrap h bound (direct h bound))))
				(ordered_right pivot k l left r right lb rb *rest));
partition_ordered :: (pivot:Nat)->(n:Nat)->(xs:SizedList Nat n)->(output:Partition Nat n)->
	@partition Nat (&natLessOrEqual) pivot n xs output->PartOrdered pivot n output;
quick_all := \P:Nat->@ => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList Nat n => \output:List Nat => \g:@quickSortAcc Nat (&natLessOrEqual) n access input output => g
	@case0 down => (\prior:SizedAll P Nat.zero (SizedList Nat).nil => (All P).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		(\prior:SizedAll P (Nat.succ k) ((SizedList Nat).cons k pivot tail) =>
			append_all P left ((List Nat).cons pivot right) result appending
				(*leftGraph (part_left P k l lower r upper lb rb
					(partition_all P pivot k tail ((Partition Nat k).parts l lower r upper lb rb)
						partitioning (sized_rest P k pivot tail prior))))
				((All P).cons pivot right (sized_first P k pivot tail prior)
					(*rightGraph (part_right P k l lower r upper lb rb
						(partition_all P pivot k tail ((Partition Nat k).parts l lower r upper lb rb)
							partitioning (sized_rest P k pivot tail prior))))));
quick_all :: (P:Nat->@)->(n:Nat)->(access:Acc Nat LT n)->(input:SizedList Nat n)->(output:List Nat)->
	@quickSortAcc Nat (&natLessOrEqual) n access input output->SizedAll P n input->All P output;
to_upper := \hi:Nat => \xs:List Nat => \p:All (&(\x:Nat => LE x hi)) xs => p
	@nil => (AllTo hi).nil
	@cons h t bound rest => (AllTo hi).cons h t bound *rest;
to_lower := \lo:Nat => \xs:List Nat => \p:All (&(\x:Nat => LE lo x)) xs => p
	@nil => (AllFrom lo).nil
	@cons h t bound rest => (AllFrom lo).cons h t bound *rest;
join_sorted := \pivot:Nat => \left:List Nat => \right:List Nat => \output:List Nat =>
	\appending:@append Nat left ((List Nat).cons pivot right) output =>
	\sl:Sorted left => \ul:AllTo pivot left => \sr:Sorted right => \lr:AllFrom pivot right =>
	append_sorted pivot left ((List Nat).cons pivot right) output appending sl ul
		(Sorted.cons pivot right lr sr) ((AllFrom pivot).cons pivot right (le_refl pivot) lr);
quick_sorted := \n:Nat => \access:Acc Nat LT n => \input:SizedList Nat n => \output:List Nat =>
	\g:@quickSortAcc Nat (&natLessOrEqual) n access input output => g
	@case0 down => Sorted.nil
	@case1 k pivot tail down l lower r upper lb rb partitioning left leftGraph right rightGraph result appending =>
		join_sorted pivot left right result appending *leftGraph
			(to_upper pivot left (quick_all (&(\x:Nat => LE x pivot)) l (down l lb) lower left leftGraph
				(ordered_left pivot k l lower r upper lb rb
					(partition_ordered pivot k tail ((Partition Nat k).parts l lower r upper lb rb) partitioning))))
			*rightGraph
			(to_lower pivot right (quick_all (&(\x:Nat => LE pivot x)) r (down r rb) upper right rightGraph
				(ordered_right pivot k l lower r upper lb rb
					(partition_ordered pivot k tail ((Partition Nat k).parts l lower r upper lb rb) partitioning))));
quick_sorted :: (n:Nat)->(access:Acc Nat LT n)->(input:SizedList Nat n)->(output:List Nat)->
	@quickSortAcc Nat (&natLessOrEqual) n access input output->Sorted output;
quick_correct := \xs:List Nat => \output:List Nat => \g:@quickSort Nat (&natLessOrEqual) xs output => g
	@case0 original n values measurement access accessibility sorted sorting => quick_sorted n access values sorted sorting;
quick_correct :: (xs:List Nat)->(output:List Nat)->@quickSort Nat (&natLessOrEqual) xs output->Sorted output;
import general_all_from;
import general_sorted;
import general_decision;
import quick_correct_existing;
import Decision;
nat_decision := \x:Nat => \y:Nat => \answer:Bool => \p:Decision x y answer => p
	@yes a b bound => (general_decision Nat &LE a b).yes bound
	@no a b bound => (general_decision Nat &LE a b).no (trans b (Nat.succ b) (le_next b) a bound);
nat_decide := \x:Nat => \y:Nat => nat_decision x y (natLessOrEqual x y) (unwrap x y (direct x y));
nat_decide :: (x:Nat)->(y:Nat)->general_decision Nat &LE x y (natLessOrEqual x y);
nat_all_from := \h:Nat => \xs:List Nat => \p:general_all_from Nat &LE h xs => p
	@nil => (AllFrom h).nil
	@cons a t bound rest => (AllFrom h).cons a t bound *rest;
nat_sorted := \xs:List Nat => \p:general_sorted Nat &LE xs => p
	@nil => Sorted.nil
	@cons h t bound rest => Sorted.cons h t (nat_all_from h t bound) *rest;
quick_result_sorted := \xs:List Nat => nat_sorted (quickSort Nat (&natLessOrEqual) xs)
	(quick_correct_existing Nat &LE (&natLessOrEqual) &trans &le_refl &nat_decide xs);
quick_result_sorted :: (xs:List Nat)->Sorted (quickSort Nat (&natLessOrEqual) xs);
import read_sorted;
import one;
import two;
import three;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one (List Nat).nil)));
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one ((List Nat).cons two (List Nat).nil)));
main := read_sorted (quickSort Nat (&natLessOrEqual) sample) (quick_result_sorted sample);
empty := read_sorted (quickSort Nat (&natLessOrEqual) (List Nat).nil) (quick_result_sorted (List Nat).nil);
singleton := read_sorted (quickSort Nat (&natLessOrEqual) ((List Nat).cons one (List Nat).nil))
	(quick_result_sorted ((List Nat).cons one (List Nat).nil));
already := read_sorted (quickSort Nat (&natLessOrEqual) expected_value) (quick_result_sorted expected_value);
reversed_value := (List Nat).cons two ((List Nat).cons one ((List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil)));
reversed := read_sorted (quickSort Nat (&natLessOrEqual) reversed_value) (quick_result_sorted reversed_value);
packet_value := quickSort Nat (&natLessOrEqual) sample;
direct_value := quickSort Nat (&natLessOrEqual) sample;
zero := Nat.zero;
one_value := Nat.succ zero;
