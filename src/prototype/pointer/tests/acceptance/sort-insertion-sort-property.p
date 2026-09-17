// Universal Sorted proof for the unchanged PR #30 insertionSort implementation.
import Nat;
import Bool;
import LE;
import Decision;
import lift;
import natLessOrEqual;
Direct := \x:Nat => \y:Nat => @{
	mk:Decision x y (natLessOrEqual x y)->*;
};
unwrap := \x:Nat => \y:Nat => \p:Direct x y => p @mk proof => proof;
direct := \x:Nat => x
	@zero => (\y:Nat => (Direct Nat.zero y).mk (Decision.yes Nat.zero y (LE.zero y)))
	@succ left => (\y:Nat => y
		@zero => (Direct (Nat.succ left) Nat.zero).mk
			(Decision.no (Nat.succ left) Nat.zero (LE.succ Nat.zero left (LE.zero left)))
		@succ right => (Direct (Nat.succ left) (Nat.succ right)).mk
			(lift left right (natLessOrEqual left right) (unwrap left right (*left right))));
direct :: (x:Nat)->(y:Nat)->Direct x y;
import List;
import AllFrom;
import Sorted;
import all_head;
import all_tail;
import insertBy;
import insert_before;
import yes_order;
import no_order;
import trans;
import le_next;
import tail_bound;
import tail_sorted;
import insertionSort;
generic_bound := \v:Nat => \xs:List Nat => \ys:List Nat => \graph:@insertBy Nat (&natLessOrEqual) v xs ys => graph
	@case0 => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo (List Nat).nil =>
		(AllFrom lo).cons v (List Nat).nil lv (AllFrom lo).nil)
	@case1 head tail trace => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo ((List Nat).cons head tail) =>
		(AllFrom lo).cons v ((List Nat).cons head tail) lv prior)
	@case2 head tail trace inserted rest => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo ((List Nat).cons head tail) =>
		(AllFrom lo).cons head inserted (all_head lo head tail prior)
			(*rest lo lv (all_tail lo head tail prior)));
generic_bound :: (v:Nat)->(xs:List Nat)->(ys:List Nat)->@insertBy Nat (&natLessOrEqual) v xs ys->
	(lo:Nat)->LE lo v->AllFrom lo xs->AllFrom lo ys;
generic_after := \v:Nat => \head:Nat => \tail:List Nat => \hv:LE head v =>
	\inserted:List Nat => \rest:@insertBy Nat (&natLessOrEqual) v tail inserted => \ih:Sorted tail->Sorted inserted =>
	\prior:Sorted ((List Nat).cons head tail) =>
	Sorted.cons head inserted
		(generic_bound v tail inserted rest head hv (tail_bound head tail prior))
		(ih (tail_sorted head tail prior));
generic_sorted := \v:Nat => \xs:List Nat => \ys:List Nat => \graph:@insertBy Nat (&natLessOrEqual) v xs ys => graph
	@case0 => (\prior:Sorted (List Nat).nil =>
		Sorted.cons v (List Nat).nil (AllFrom v).nil Sorted.nil)
	@case1 head tail trace => insert_before v head tail
		(trace @case0 right => yes_order v right (unwrap v right (direct v right)))
	@case2 head tail trace inserted rest => generic_after v head tail
		(trans head (Nat.succ head) (le_next head) v
			(trace @case0 right => no_order v right (unwrap v right (direct v right))))
		inserted rest &*rest;
generic_sorted :: (v:Nat)->(xs:List Nat)->(ys:List Nat)->@insertBy Nat (&natLessOrEqual) v xs ys->Sorted xs->Sorted ys;
sort_correct := \xs:List Nat => \ys:List Nat => \graph:@insertionSort xs ys => graph
	@case0 => Sorted.nil
	@case1 head tail sorted rest result inserted => generic_sorted head sorted result inserted *rest;
sort_correct :: (xs:List Nat)->(ys:List Nat)->@insertionSort xs ys->Sorted ys;
import read_sorted;
import one;
import two;
import three;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one (List Nat).nil)));
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one ((List Nat).cons two (List Nat).nil)));
main := *insertionSort sample @ys => read_sorted ys (sort_correct sample ys @ys);
empty := *insertionSort (List Nat).nil @ys => read_sorted ys (sort_correct (List Nat).nil ys @ys);
singleton := *insertionSort ((List Nat).cons one (List Nat).nil) @ys =>
	read_sorted ys (sort_correct ((List Nat).cons one (List Nat).nil) ys @ys);
already := *insertionSort expected_value @ys => read_sorted ys (sort_correct expected_value ys @ys);
packet_value := *insertionSort sample @ys => ys;
direct_value := insertionSort sample;
zero := Nat.zero;
one_value := Nat.succ zero;
