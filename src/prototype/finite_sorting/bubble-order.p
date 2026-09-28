// Transport an arbitrary element predicate along the common permutation proof.
permutation_all := \A:@ => \P:A->@ => \xs:List A => \ys:List A => \p:permutation A xs ys => p
	@nil => (\prior:qsr_All A P (List A).nil => (qsr_All A P).nil)
	@keep h left right rest => (\prior:qsr_All A P ((List A).cons h left) =>
		(qsr_All A P).cons h right (qsr_all_head A P h left prior) (*rest (qsr_all_tail A P h left prior)))
	@swap x y tail => (\prior:qsr_All A P ((List A).cons x ((List A).cons y tail)) =>
		(qsr_All A P).cons y ((List A).cons x tail)
			(qsr_all_head A P y tail (qsr_all_tail A P x ((List A).cons y tail) prior))
			((qsr_All A P).cons x tail (qsr_all_head A P x ((List A).cons y tail) prior)
				(qsr_all_tail A P y tail (qsr_all_tail A P x ((List A).cons y tail) prior))))
	@compose left middle right first second => (\prior:qsr_All A P left => *second (*first prior));
permutation_all :: (A:@)->(P:A->@)->(xs:List A)->(ys:List A)->permutation A xs ys->qsr_All A P xs->qsr_All A P ys;
sized_all_values := \A:@ => \P:A->@ => \n:Nat => \xs:SizedList A n => \prior:qsr_SizedAll A P n xs => prior
	@nil => (qsr_All A P).nil
	@cons k h t ph pt => (qsr_All A P).cons h (sized_values A k t) ph *pt;
sized_all_map := \A:@ => \P:A->@ => \Q:A->@ => \map:(x:A)->P x->Q x =>
	\n:Nat => \xs:SizedList A n => \prior:qsr_SizedAll A P n xs => prior
	@nil => (qsr_SizedAll A Q).nil
	@cons k h t ph pt => (qsr_SizedAll A Q).cons k h t (map h ph) *pt;

bubble_minimum := \A:@ => \R:A->A->@ => @\n:Nat => @\xs:SizedList A n => {
	nil:* Nat.zero (SizedList A).nil;
	cons:(k:Nat)->(h:A)->(t:SizedList A k)->qsr_SizedAll A &(\x:A=>R h x) k t->
		* (Nat.succ k) ((SizedList A).cons k h t);
};
bubble_pair_minimum := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\n:Nat => \x:A => \y:A => \tail:SizedList A n =>
	\bound:qsr_SizedAll A &(\z:A=>R y z) n tail => \answer:Bool =>
	\decision:general_decision A R x y answer => decision
	@(result self => bubble_minimum A R (Nat.succ (Nat.succ n)) (bubble_pair A n x y tail result))
	@yes edge => (bubble_minimum A R).cons (Nat.succ n) x ((SizedList A).cons n y tail)
		((qsr_SizedAll A &(\z:A=>R x z)).cons n y tail edge
			(sized_all_map A &(\z:A=>R y z) &(\z:A=>R x z) &(trans x y edge) n tail bound))
	@no edge => (bubble_minimum A R).cons (Nat.succ n) y ((SizedList A).cons n x tail)
		((qsr_SizedAll A &(\z:A=>R y z)).cons n x tail edge bound);
bubble_join_minimum := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\x:A => \n:Nat => \xs:SizedList A n => \prior:bubble_minimum A R n xs => prior
	@(size values self => bubble_minimum A R (Nat.succ size) (bubble_join A &le x size values))
	@nil => (bubble_minimum A R).cons Nat.zero x (SizedList A).nil (qsr_SizedAll A &(\y:A=>R x y)).nil
	@cons k h t bound => bubble_pair_minimum A R trans k x h t bound (le x h) (decide x h);
bubble_pass_minimum := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \n:Nat => \xs:SizedList A n => xs
	@(size self => bubble_minimum A R size (bubble_pass A &le size self))
	@nil => (bubble_minimum A R).nil
	@cons k h t => bubble_join_minimum A R &le decide trans h k (bubble_pass A &le k t) *t;
bubble_pass_minimum :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(n:Nat)->(xs:SizedList A n)->
	bubble_minimum A R n (bubble_pass A &le n xs);

bubble_step_strong := \A:@ => \R:A->A->@ => \n:Nat => \xs:SizedList A n => \prior:bubble_minimum A R n xs => prior
	@(size values self => (rest:SizedList A (predecessor size)->List A)->
		((input:SizedList A (predecessor size))->permutation A (sized_values A (predecessor size) input) (rest input))->
		((input:SizedList A (predecessor size))->general_strongly_sorted A R (rest input))->
		general_strongly_sorted A R (bubble_sort_step A size values &rest))
	@nil => (\rest:SizedList A Nat.zero->List A =>
		\content:(input:SizedList A Nat.zero)->permutation A (sized_values A Nat.zero input) (rest input) =>
		\sorted:(input:SizedList A Nat.zero)->general_strongly_sorted A R (rest input) => (general_strongly_sorted A R).nil)
	@cons k h t bound => (\rest:SizedList A k->List A =>
		\content:(input:SizedList A k)->permutation A (sized_values A k input) (rest input) =>
		\sorted:(input:SizedList A k)->general_strongly_sorted A R (rest input) =>
		(general_strongly_sorted A R).cons h (rest t)
			(qsr_all_connection A R h (rest t)
				(permutation_all A &(\x:A=>R h x) (sized_values A k t) (rest t) (content t)
					(sized_all_values A &(\x:A=>R h x) k t bound))) (sorted t));
bubble_sized_strong := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \n:Nat => n
	@(size => (xs:SizedList A size)->general_strongly_sorted A R (bubble_sort_sized A &le size xs))
	@zero => (\xs:SizedList A Nat.zero => xs
		@(size self => general_strongly_sorted A R (bubble_sort_sized A &le size self))
		@nil => (general_strongly_sorted A R).nil)
	@succ k => (\xs:SizedList A (Nat.succ k) => bubble_step_strong A R (Nat.succ k)
		(bubble_pass A &le (Nat.succ k) xs) (bubble_pass_minimum A R &le decide trans (Nat.succ k) xs)
		&(bubble_sort_sized A &le k) &(bubble_sized_content A &le k) &*k);
bubble_strong := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \xs:List A => measure A xs
	@(self => general_strongly_sorted A R (self @measured n input => bubble_sort_sized A &le n input))
	@measured n input => bubble_sized_strong A R &le decide trans n input;
bubble_strong :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(xs:List A)->general_strongly_sorted A R (bubble_sort A &le xs);
bubble_local_transitive := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \xs:List A =>
	strongly_sorted_to_locally_sorted A R (bubble_sort A &le xs) (bubble_strong A R &le decide trans xs);
// This backend's transitivity requirement is explicit, not a claim of minimality.
bubble_transitive_backend := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	(sorting_backend A R).mk &(bubble_sort A &le) &(bubble_local_transitive A R &le decide trans) &(bubble_content A &le);
bubble_transitive_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->sorting_backend A R;
