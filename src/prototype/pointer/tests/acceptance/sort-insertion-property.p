// Ordinary proof terms over the exact PR #30 provider; no primitive sort rules.
import Nat;
import Bool;
import List;
import natLessOrEqual;
import insertBy;
import insertionSortBy;
import insertNat;
import insertionSort;
LE := @\left:Nat => @\right:Nat => {
	zero:(n:Nat)->* Nat.zero n;
	succ:(m:Nat)->(n:Nat)->* m n->* (Nat.succ m) (Nat.succ n);
};
step := \a:Nat => \b:Nat => \ih:(z:Nat)->LE b z->LE a z =>
	\z:Nat => \upper:LE (Nat.succ b) z => upper
		@succ c d rest => LE.succ a d (ih d rest);
trans := \x:Nat => \y:Nat => \p:LE x y => p
	@zero n => (\z:Nat => \upper:LE n z => LE.zero z)
	@succ a b prior => step a b &*prior;
trans :: (x:Nat)->(y:Nat)->LE x y->(z:Nat)->LE y z->LE x z;

Decision := @\x:Nat => @\y:Nat => @\answer:Bool => {
	yes:(a:Nat)->(b:Nat)->LE a b->* a b Bool.true;
	no:(a:Nat)->(b:Nat)->LE (Nat.succ b) a->* a b Bool.false;
};
lift := \x:Nat => \y:Nat => \answer:Bool => \proof:Decision x y answer => proof
	@yes a b prior => Decision.yes (Nat.succ a) (Nat.succ b) (LE.succ a b prior)
	@no a b prior => Decision.no (Nat.succ a) (Nat.succ b) (LE.succ (Nat.succ b) a prior);
lift :: (x:Nat)->(y:Nat)->(answer:Bool)->Decision x y answer->
	Decision (Nat.succ x) (Nat.succ y) answer;
correct := \x:Nat => \y:Nat => \answer:Bool => \proof:@natLessOrEqual x y answer => proof
	@case0 right => Decision.yes Nat.zero right (LE.zero right)
	@case1 left => Decision.no (Nat.succ left) Nat.zero (LE.succ Nat.zero left (LE.zero left))
	@case2 left right answer prior => lift left right answer *prior;
correct :: (x:Nat)->(y:Nat)->(answer:Bool)->@natLessOrEqual x y answer->Decision x y answer;

AllFrom := \head:Nat => @\xs:List Nat => {
	nil:* (List Nat).nil;
	cons:(next:Nat)->(tail:List Nat)->LE head next->* tail->* ((List Nat).cons next tail);
};
Sorted := @\xs:List Nat => {
	nil:* (List Nat).nil;
	cons:(head:Nat)->(tail:List Nat)->AllFrom head tail->* tail->* ((List Nat).cons head tail);
};
all_trans := \y:Nat => \xs:List Nat => \bound:AllFrom y xs => bound
	@nil => (\x:Nat => \xy:LE x y => (AllFrom x).nil)
	@cons head tail yh yt => (\x:Nat => \xy:LE x y =>
		(AllFrom x).cons head tail (trans x y xy head yh) (*yt x xy));
all_trans :: (y:Nat)->(xs:List Nat)->AllFrom y xs->(x:Nat)->LE x y->AllFrom x xs;
tail_sorted := \head:Nat => \tail:List Nat => \proof:Sorted ((List Nat).cons head tail) => proof
	@cons h t bound rest => rest;
tail_sorted :: (head:Nat)->(tail:List Nat)->Sorted ((List Nat).cons head tail)->Sorted tail;
tail_bound := \head:Nat => \tail:List Nat => \proof:Sorted ((List Nat).cons head tail) => proof
	@cons h t bound rest => bound;
tail_bound :: (head:Nat)->(tail:List Nat)->Sorted ((List Nat).cons head tail)->AllFrom head tail;
all_head := \lo:Nat => \head:Nat => \tail:List Nat =>
	\proof:AllFrom lo ((List Nat).cons head tail) => proof
		@cons h t bound rest => bound;
all_head :: (lo:Nat)->(head:Nat)->(tail:List Nat)->AllFrom lo ((List Nat).cons head tail)->LE lo head;
all_tail := \lo:Nat => \head:Nat => \tail:List Nat =>
	\proof:AllFrom lo ((List Nat).cons head tail) => proof
		@cons h t bound rest => rest;
all_tail :: (lo:Nat)->(head:Nat)->(tail:List Nat)->AllFrom lo ((List Nat).cons head tail)->AllFrom lo tail;
insert_bound := \v:Nat => \xs:List Nat => \ys:List Nat => \graph:@insertNat v xs ys => graph
	@case0 => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo (List Nat).nil =>
		(AllFrom lo).cons v (List Nat).nil lv (AllFrom lo).nil)
	@case1 head tail trace => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo ((List Nat).cons head tail) =>
		(AllFrom lo).cons v ((List Nat).cons head tail) lv prior)
	@case2 head tail trace inserted rest => (\lo:Nat => \lv:LE lo v => \prior:AllFrom lo ((List Nat).cons head tail) =>
		(AllFrom lo).cons head inserted (all_head lo head tail prior)
			(*rest lo lv (all_tail lo head tail prior)));
insert_bound :: (v:Nat)->(xs:List Nat)->(ys:List Nat)->@insertNat v xs ys->
	(lo:Nat)->LE lo v->AllFrom lo xs->AllFrom lo ys;
yes_order := \x:Nat => \y:Nat => \d:Decision x y Bool.true => d
	@yes a b proof => proof;
yes_order :: (x:Nat)->(y:Nat)->Decision x y Bool.true->LE x y;
no_order := \x:Nat => \y:Nat => \d:Decision x y Bool.false => d
	@no a b proof => proof;
no_order :: (x:Nat)->(y:Nat)->Decision x y Bool.false->LE (Nat.succ y) x;
le_next := \n:Nat => n
	@zero => LE.zero (Nat.succ Nat.zero)
	@succ k => LE.succ k (Nat.succ k) *k;
le_next :: (n:Nat)->LE n (Nat.succ n);
insert_before := \v:Nat => \head:Nat => \tail:List Nat => \vh:LE v head =>
	\prior:Sorted ((List Nat).cons head tail) =>
	Sorted.cons v ((List Nat).cons head tail)
		((AllFrom v).cons head tail vh (all_trans head tail (tail_bound head tail prior) v vh)) prior;
insert_after := \v:Nat => \head:Nat => \tail:List Nat => \hv:LE head v =>
	\inserted:List Nat => \rest:@insertNat v tail inserted => \ih:Sorted tail->Sorted inserted =>
	\prior:Sorted ((List Nat).cons head tail) =>
	Sorted.cons head inserted
		(insert_bound v tail inserted rest head hv (tail_bound head tail prior))
		(ih (tail_sorted head tail prior));
insert_sorted := \v:Nat => \xs:List Nat => \ys:List Nat => \graph:@insertNat v xs ys => graph
	@case0 => (\prior:Sorted (List Nat).nil =>
		Sorted.cons v (List Nat).nil (AllFrom v).nil Sorted.nil)
	@case1 head tail trace => insert_before v head tail (yes_order v head (correct v head Bool.true trace))
	@case2 head tail trace inserted rest => insert_after v head tail
		(trans head (Nat.succ head) (le_next head) v (no_order v head (correct v head Bool.false trace)))
		inserted rest &*rest;
insert_sorted :: (v:Nat)->(xs:List Nat)->(ys:List Nat)->@insertNat v xs ys->Sorted xs->Sorted ys;
read_sorted := \xs:List Nat => \proof:Sorted xs => proof
	@nil => Nat.zero
	@cons head tail bound rest => Nat.succ *rest;
one := Nat.succ Nat.zero;
two := Nat.succ one;
three := Nat.succ two;
sample := (List Nat).cons Nat.zero ((List Nat).cons two (List Nat).nil);
sorted_sample := Sorted.cons Nat.zero ((List Nat).cons two (List Nat).nil)
	((AllFrom Nat.zero).cons two (List Nat).nil (LE.zero two) (AllFrom Nat.zero).nil)
	(Sorted.cons two (List Nat).nil (AllFrom two).nil Sorted.nil);
main := *insertNat one sample @ys => read_sorted ys ((insert_sorted one sample ys @ys) sorted_sample);
duplicate := *insertNat two sample @ys => read_sorted ys ((insert_sorted two sample ys @ys) sorted_sample);
after := *insertNat three sample @ys => read_sorted ys ((insert_sorted three sample ys @ys) sorted_sample);
empty := *insertNat one (List Nat).nil @ys => read_sorted ys ((insert_sorted one (List Nat).nil ys @ys) Sorted.nil);
packet_value := *insertNat one sample @ys => ys;
direct_value := insertNat one sample;
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons two (List Nat).nil));
