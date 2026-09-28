// Right-to-left adjacent swaps, followed by sorting the remaining suffix.
// SizedList supplies the decreasing length; there is no truncating fuel case.
bubble_pair := \A:@ => \n:Nat => \x:A => \y:A => \tail:SizedList A n => \answer:Bool => answer
	@true => (SizedList A).cons (Nat.succ n) x ((SizedList A).cons n y tail)
	@false => (SizedList A).cons (Nat.succ n) y ((SizedList A).cons n x tail);
bubble_join := \A:@ => \le:A->A->Bool => \x:A => \n:Nat => \xs:SizedList A n => xs
	@(size self => SizedList A (Nat.succ size))
	@nil => (SizedList A).cons Nat.zero x (SizedList A).nil
	@cons k h t => bubble_pair A k x h t (le x h);
bubble_pass := \A:@ => \le:A->A->Bool => \n:Nat => \xs:SizedList A n => xs
	@(size self => SizedList A size)
	@nil => (SizedList A).nil
	@cons k h t => bubble_join A &le h k *t;
bubble_pass :: (A:@)->(le:A->A->Bool)->(n:Nat)->SizedList A n->SizedList A n;
bubble_sort_step := \A:@ => \n:Nat => \xs:SizedList A n => xs
	@(size self => (SizedList A (predecessor size)->List A)->List A)
	@nil => (\rest:SizedList A Nat.zero->List A => (List A).nil)
	@cons k h t => (\rest:SizedList A k->List A => (List A).cons h (rest t));
bubble_sort_sized := \A:@ => \le:A->A->Bool => \n:Nat => n
	@(size => SizedList A size->List A)
	@zero => (\xs:SizedList A Nat.zero => xs @nil => (List A).nil)
	@succ k => (\xs:SizedList A (Nat.succ k) => bubble_sort_step A (Nat.succ k) (bubble_pass A &le (Nat.succ k) xs) &*k);
bubble_sort := \A:@ => \le:A->A->Bool => \xs:List A => measure A xs
	@measured n values => bubble_sort_sized A &le n values;
bubble_sort :: (A:@)->(le:A->A->Bool)->List A->List A;

sized_values := \A:@ => \n:Nat => \xs:SizedList A n => xs
	@nil => (List A).nil
	@cons k h t => (List A).cons h *t;
sized_contents_values := \A:@ => \n:Nat => \xs:SizedList A n => \values:List A =>
	\p:sized_contents A n xs values => p
	@(size input source self => permutation A source (sized_values A size input))
	@nil => (permutation A).nil
	@cons k h t values prior => (permutation A).keep h values (sized_values A k t) *prior;

bubble_pair_content := \A:@ => \n:Nat => \x:A => \y:A => \tail:SizedList A n => \answer:Bool => answer
	@(result => permutation A ((List A).cons x ((List A).cons y (sized_values A n tail)))
		(sized_values A (Nat.succ (Nat.succ n)) (bubble_pair A n x y tail result)))
	@true => permutation_refl A ((List A).cons x ((List A).cons y (sized_values A n tail)))
	@false => (permutation A).swap x y (sized_values A n tail);
bubble_join_content := \A:@ => \le:A->A->Bool => \x:A => \n:Nat => \xs:SizedList A n => xs
	@(size self => permutation A ((List A).cons x (sized_values A size self))
		(sized_values A (Nat.succ size) (bubble_join A &le x size self)))
	@nil => permutation_refl A ((List A).cons x (List A).nil)
	@cons k h t => bubble_pair_content A k x h t (le x h);
bubble_pass_content := \A:@ => \le:A->A->Bool => \n:Nat => \xs:SizedList A n => xs
	@(size self => permutation A (sized_values A size self) (sized_values A size (bubble_pass A &le size self)))
	@nil => (permutation A).nil
	@cons k h t => (permutation A).compose ((List A).cons h (sized_values A k t))
		((List A).cons h (sized_values A k (bubble_pass A &le k t)))
		(sized_values A (Nat.succ k) (bubble_join A &le h k (bubble_pass A &le k t)))
		((permutation A).keep h (sized_values A k t) (sized_values A k (bubble_pass A &le k t)) *t)
		(bubble_join_content A &le h k (bubble_pass A &le k t));
bubble_pass_content :: (A:@)->(le:A->A->Bool)->(n:Nat)->(xs:SizedList A n)->
	permutation A (sized_values A n xs) (sized_values A n (bubble_pass A &le n xs));
bubble_step_content := \A:@ => \n:Nat => \xs:SizedList A n => xs
	@(size self => (rest:SizedList A (predecessor size)->List A)->
		((values:SizedList A (predecessor size))->permutation A (sized_values A (predecessor size) values) (rest values))->
		permutation A (sized_values A size self) (bubble_sort_step A size self &rest))
	@nil => (\rest:SizedList A Nat.zero->List A =>
		\proof:(values:SizedList A Nat.zero)->permutation A (sized_values A Nat.zero values) (rest values) =>
		(permutation A).nil)
	@cons k h t => (\rest:SizedList A k->List A =>
		\proof:(values:SizedList A k)->permutation A (sized_values A k values) (rest values) =>
		(permutation A).keep h (sized_values A k t) (rest t) (proof t));
bubble_sized_content := \A:@ => \le:A->A->Bool => \n:Nat => n
	@(size => (xs:SizedList A size)->permutation A (sized_values A size xs) (bubble_sort_sized A &le size xs))
	@zero => (\xs:SizedList A Nat.zero => xs
		@(size self => permutation A (sized_values A size self) (bubble_sort_sized A &le size self))
		@nil => (permutation A).nil)
	@succ k => (\xs:SizedList A (Nat.succ k) => (permutation A).compose
		(sized_values A (Nat.succ k) xs) (sized_values A (Nat.succ k) (bubble_pass A &le (Nat.succ k) xs))
		(bubble_sort_sized A &le (Nat.succ k) xs)
		(bubble_pass_content A &le (Nat.succ k) xs)
		(bubble_step_content A (Nat.succ k) (bubble_pass A &le (Nat.succ k) xs) &(bubble_sort_sized A &le k) &*k));
bubble_sized_content :: (A:@)->(le:A->A->Bool)->(n:Nat)->(xs:SizedList A n)->
	permutation A (sized_values A n xs) (bubble_sort_sized A &le n xs);
bubble_measured_content := \A:@ => \le:A->A->Bool => \xs:List A => \out:Measured A =>
	\p:measurement_contents A xs out => p
	@(values self => permutation A xs (values @measured n input => bubble_sort_sized A &le n input))
	@measured n input prior => (permutation A).compose xs (sized_values A n input)
		(bubble_sort_sized A &le n input) (sized_contents_values A n input xs prior) (bubble_sized_content A &le n input);
bubble_content := \A:@ => \le:A->A->Bool => \xs:List A =>
	bubble_measured_content A &le xs (measure A xs) (measure_content_result A xs);
bubble_content :: (A:@)->(le:A->A->Bool)->(xs:List A)->permutation A xs (bubble_sort A &le xs);
