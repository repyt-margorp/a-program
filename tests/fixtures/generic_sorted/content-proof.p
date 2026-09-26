// Append to the generic Sorted fixture. Content preservation needs neither
// comparator correctness nor equality between different LT proof trees.
import Measured;
import measure;

permutation := \A:@ => @\left:List A => @\right:List A => {
	nil:* (List A).nil (List A).nil;
	keep:(h:A)->(xs:List A)->(ys:List A)->* xs ys->
		* ((List A).cons h xs) ((List A).cons h ys);
	swap:(x:A)->(y:A)->(xs:List A)->
		* ((List A).cons x ((List A).cons y xs)) ((List A).cons y ((List A).cons x xs));
	compose:(xs:List A)->(ys:List A)->(zs:List A)->* xs ys->* ys zs->* xs zs;
};

permutation_refl := \A:@ => \xs:List A => xs
	@nil => (permutation A).nil
	@cons h t => (permutation A).keep h t t *t;
permutation_refl :: (A:@)->(xs:List A)->permutation A xs xs;

permutation_suffix := \A:@ => \xs:List A => \ys:List A => \p:permutation A xs ys => p
	@nil => (\zs:List A => permutation_refl A zs)
	@keep h x y prior => (\zs:List A =>
		(permutation A).keep h (append A x zs) (append A y zs) (*prior zs))
	@swap x y t => (\zs:List A => (permutation A).swap x y (append A t zs))
	@compose x y z left right => (\zs:List A =>
		(permutation A).compose (append A x zs) (append A y zs) (append A z zs) (*left zs) (*right zs));
permutation_suffix :: (A:@)->(xs:List A)->(ys:List A)->permutation A xs ys->
	(zs:List A)->permutation A (append A xs zs) (append A ys zs);

permutation_prefix := \A:@ => \xs:List A => xs
	@nil => (\ys:List A => \zs:List A => \p:permutation A ys zs => p)
	@cons h t => (\ys:List A => \zs:List A => \p:permutation A ys zs =>
		(permutation A).keep h (append A t ys) (append A t zs) (*t ys zs p));
permutation_prefix :: (A:@)->(xs:List A)->(ys:List A)->(zs:List A)->permutation A ys zs->
	permutation A (append A xs ys) (append A xs zs);

permutation_append := \A:@ => \xs:List A => \xs2:List A => \px:permutation A xs xs2 =>
	\ys:List A => \ys2:List A => \py:permutation A ys ys2 =>
	(permutation A).compose (append A xs ys) (append A xs2 ys) (append A xs2 ys2)
		(permutation_suffix A xs xs2 px ys) (permutation_prefix A xs2 ys ys2 py);

permutation_move := \A:@ => \h:A => \xs:List A => xs
	@nil => (\ys:List A => permutation_refl A ((List A).cons h ys))
	@cons x t => (\ys:List A =>
		(permutation A).compose
			((List A).cons h ((List A).cons x (append A t ys)))
			((List A).cons x ((List A).cons h (append A t ys)))
			((List A).cons x (append A t ((List A).cons h ys)))
			((permutation A).swap h x (append A t ys))
			((permutation A).keep x ((List A).cons h (append A t ys))
				(append A t ((List A).cons h ys)) (*t ys)));
permutation_move :: (A:@)->(h:A)->(xs:List A)->(ys:List A)->
	permutation A ((List A).cons h (append A xs ys)) (append A xs ((List A).cons h ys));

append_content := \A:@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@case0 following => permutation_refl A following
	@case1 h t following output rest =>
		(permutation A).keep h (append A t following) output *rest;
append_content :: (A:@)->(xs:List A)->(ys:List A)->(zs:List A)->@append A xs ys zs->
	permutation A (append A xs ys) zs;

sized_contents := \A:@ => @\n:Nat => @\input:SizedList A n => @\values:List A => {
	nil:* Nat.zero (SizedList A).nil (List A).nil;
	cons:(k:Nat)->(h:A)->(t:SizedList A k)->(xs:List A)->* k t xs->
		* (Nat.succ k) ((SizedList A).cons k h t) ((List A).cons h xs);
};

partition_contents := \A:@ => \original:List A => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => @{
	parts:(xs:List A)->(ys:List A)->sized_contents A l left xs->sized_contents A r right ys->
		permutation A original (append A xs ys)->*;
};
partition_result_contents := \A:@ => \original:List A => \n:Nat => \output:Partition A n => output
	@parts l left r right lb rb => partition_contents A original l left r right;

partition_keep_lower := \A:@ => \h:A => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \original:List A =>
	\prior:partition_contents A original l left r right => prior
	@parts xs ys px py elements =>
		(partition_contents A ((List A).cons h original) (Nat.succ l) ((SizedList A).cons l h left) r right).parts
			((List A).cons h xs) ys ((sized_contents A).cons l h left xs px) py
			((permutation A).keep h original (append A xs ys) elements);

partition_keep_upper := \A:@ => \h:A => \l:Nat => \left:SizedList A l =>
	\r:Nat => \right:SizedList A r => \original:List A =>
	\prior:partition_contents A original l left r right => prior
	@parts xs ys px py elements =>
		(partition_contents A ((List A).cons h original) l left (Nat.succ r) ((SizedList A).cons r h right)).parts
			xs ((List A).cons h ys) px ((sized_contents A).cons r h right ys py)
			((permutation A).compose ((List A).cons h original) ((List A).cons h (append A xs ys))
				(append A xs ((List A).cons h ys))
				((permutation A).keep h original (append A xs ys) elements)
				(permutation_move A h xs ys));

partition_lower_content := \A:@ => \k:Nat => \h:A => \t:SizedList A k =>
	\l:Nat => \left:SizedList A l => \r:Nat => \right:SizedList A r =>
	\ih:(xs:List A)->sized_contents A k t xs->partition_contents A xs l left r right =>
	\original:List A => \representation:sized_contents A (Nat.succ k) ((SizedList A).cons k h t) original => representation
	@cons size head tail values rest =>
		partition_keep_lower A head l left r right values (ih values rest);

partition_upper_content := \A:@ => \k:Nat => \h:A => \t:SizedList A k =>
	\l:Nat => \left:SizedList A l => \r:Nat => \right:SizedList A r =>
	\ih:(xs:List A)->sized_contents A k t xs->partition_contents A xs l left r right =>
	\original:List A => \representation:sized_contents A (Nat.succ k) ((SizedList A).cons k h t) original => representation
	@cons size head tail values rest =>
		partition_keep_upper A head l left r right values (ih values rest);

partition_content_steps := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat =>
	\input:SizedList A n => \output:Partition A n => \g:@partition A &le pivot n input output => g
	@(size source result self => (original:List A)->sized_contents A size source original->
		partition_result_contents A original size result)
	@case0 => (\original:List A => \representation:sized_contents A Nat.zero (SizedList A).nil original =>
		representation @nil => (partition_contents A (List A).nil Nat.zero (SizedList A).nil Nat.zero (SizedList A).nil).parts
			(List A).nil (List A).nil (sized_contents A).nil (sized_contents A).nil (permutation A).nil)
	@case1 k h t comparison l left r right lb rb rest =>
		partition_lower_content A k h t l left r right &*rest
	@case2 k h t comparison l left r right lb rb rest =>
		partition_upper_content A k h t l left r right &*rest;
partition_content := \A:@ => \le:A->A->Bool => \pivot:A => \n:Nat =>
	\input:SizedList A n => \output:Partition A n => \g:@partition A &le pivot n input output =>
	\original:List A => \representation:sized_contents A n input original =>
		(partition_content_steps A le pivot n input output g) original representation;
partition_content :: (A:@)->(le:A->A->Bool)->(pivot:A)->(n:Nat)->
	(input:SizedList A n)->(output:Partition A n)->@partition A &le pivot n input output->
	(original:List A)->sized_contents A n input original->partition_result_contents A original n output;

permutation_join := \A:@ => \pivot:A => \xs:List A => \ys:List A =>
	\left:List A => \right:List A => \result:List A =>
	\appending:@append A left ((List A).cons pivot right) result =>
	\px:permutation A xs left => \py:permutation A ys right =>
	(permutation A).compose ((List A).cons pivot (append A xs ys))
		(append A xs ((List A).cons pivot ys)) result
		(permutation_move A pivot xs ys)
		((permutation A).compose (append A xs ((List A).cons pivot ys))
			(append A left ((List A).cons pivot right)) result
			(permutation_append A xs left px ((List A).cons pivot ys) ((List A).cons pivot right)
				((permutation A).keep pivot ys right py))
			(append_content A left ((List A).cons pivot right) result appending));

quick_partition_content := \A:@ => \pivot:A => \original:List A =>
	\l:Nat => \lower:SizedList A l => \r:Nat => \upper:SizedList A r =>
	\left:List A => \right:List A => \result:List A =>
	\appending:@append A left ((List A).cons pivot right) result =>
	\ih_left:(xs:List A)->sized_contents A l lower xs->permutation A xs left =>
	\ih_right:(ys:List A)->sized_contents A r upper ys->permutation A ys right =>
	\prior:partition_contents A original l lower r upper => prior
	@parts xs ys px py elements =>
		(permutation A).compose ((List A).cons pivot original)
			((List A).cons pivot (append A xs ys)) result
			((permutation A).keep pivot original (append A xs ys) elements)
			(permutation_join A pivot xs ys left right result appending (ih_left xs px) (ih_right ys py));

quick_step_content := \A:@ => \k:Nat => \pivot:A => \tail:SizedList A k =>
	\l:Nat => \lower:SizedList A l => \r:Nat => \upper:SizedList A r =>
	\partition_proof:(xs:List A)->sized_contents A k tail xs->partition_contents A xs l lower r upper =>
	\left:List A => \right:List A => \result:List A =>
	\appending:@append A left ((List A).cons pivot right) result =>
	\ih_left:(xs:List A)->sized_contents A l lower xs->permutation A xs left =>
	\ih_right:(ys:List A)->sized_contents A r upper ys->permutation A ys right =>
	\original:List A => \representation:sized_contents A (Nat.succ k) ((SizedList A).cons k pivot tail) original => representation
	@cons n h t values rest =>
		quick_partition_content A h values l lower r upper left right result appending &ih_left &ih_right
			(partition_proof values rest);

quick_sized_content_steps := \A:@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A &le n access input output => g
	@(size accessible source result self => (original:List A)->sized_contents A size source original->permutation A original result)
	@case0 down => (\original:List A => \representation:sized_contents A Nat.zero (SizedList A).nil original =>
		representation @(size input values self => permutation A values (List A).nil)
			@nil => (permutation A).nil)
	@case1 k pivot tail down l lower r upper lb rb partitioning left left_graph right right_graph result appending =>
		quick_step_content A k pivot tail l lower r upper
			&(partition_content A le pivot k tail ((Partition A k).parts l lower r upper lb rb) partitioning)
			left right result appending
			&*left_graph &*right_graph;
quick_sized_content := \A:@ => \le:A->A->Bool => \n:Nat => \access:Acc Nat LT n =>
	\input:SizedList A n => \output:List A => \g:@quickSortAcc A &le n access input output =>
	\original:List A => \representation:sized_contents A n input original =>
		(quick_sized_content_steps A le n access input output g) original representation;
quick_sized_content :: (A:@)->(le:A->A->Bool)->(n:Nat)->(access:Acc Nat LT n)->
	(input:SizedList A n)->(output:List A)->@quickSortAcc A &le n access input output->
	(original:List A)->sized_contents A n input original->permutation A original output;

measurement_contents := \A:@ => \original:List A => @\output:Measured A => {
	measured:(n:Nat)->(values:SizedList A n)->sized_contents A n values original->
		* ((Measured A).measured n values);
};
measurement_elements := \A:@ => \original:List A => \n:Nat => \values:SizedList A n =>
	\p:measurement_contents A original ((Measured A).measured n values) => p
	@measured k input representation => representation;
measure_content := \A:@ => \original:List A => \output:Measured A => \g:@measure A original output => g
	@case0 => (measurement_contents A (List A).nil).measured Nat.zero (SizedList A).nil (sized_contents A).nil
	@case1 h t n values rest => (measurement_contents A ((List A).cons h t)).measured
		(Nat.succ n) ((SizedList A).cons n h values)
		((sized_contents A).cons n h values t (measurement_elements A t n values *rest));
measure_content :: (A:@)->(original:List A)->(output:Measured A)->@measure A original output->
	measurement_contents A original output;

quick_content := \A:@ => \le:A->A->Bool => \input:List A => \output:List A => \g:@quickSort A &le input output => g
	@case0 original n values measurement access accessibility sorted sorting =>
		quick_sized_content A le n access values sorted sorting original
			(measurement_elements A original n values
				(measure_content A original ((Measured A).measured n values) measurement));
quick_content :: (A:@)->(le:A->A->Bool)->(input:List A)->(output:List A)->
	@quickSort A &le input output->permutation A input output;
