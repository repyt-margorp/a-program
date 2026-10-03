// Keep the legacy repeated-insertion merge; this is not linear-time merging.
// Generalize payloads only: Nat still indexes fuel and measured lengths.
merge_sort_fuel_by := \A:@ => \le:A->A->Bool => \fuel:Nat => fuel
	@zero => (\xs:List A => xs)
	@succ remaining => (\xs:List A => xs
		@nil => (List A).nil
		@cons head tail => (splitAlternating A xs @halves left right =>
			mergeBy A &le (*remaining left) (*remaining right)));
merge_sort_fuel_by :: (A:@)->(A->A->Bool)->Nat->List A->List A;
merge_sort_by := \A:@ => \le:A->A->Bool => \xs:List A => measure A xs
	@measured size values => merge_sort_fuel_by A &le size xs;
merge_sort_by :: (A:@)->(A->A->Bool)->List A->List A;

merge_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => \ys:List A => xs
	@(self => general_locally_sorted A R ys->general_locally_sorted A R (mergeBy A &le self ys))
	@nil => (\prior:general_locally_sorted A R ys => prior)
	@cons h t => (\prior:general_locally_sorted A R ys =>
		insert_local A R &le decide h (mergeBy A &le t ys) (*t prior));
merge_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->(ys:List A)->
	general_locally_sorted A R ys->general_locally_sorted A R (mergeBy A &le xs ys);

merge_content := \A:@ => \le:A->A->Bool => \xs:List A => \ys:List A => xs
	@(self => permutation A (append A self ys) (mergeBy A &le self ys))
	@nil => permutation_refl A ys
	@cons h t => (permutation A).compose ((List A).cons h (append A t ys))
		((List A).cons h (mergeBy A &le t ys)) (insertBy A &le h (mergeBy A &le t ys))
		((permutation A).keep h (append A t ys) (mergeBy A &le t ys) *t)
		(insert_content A &le h (mergeBy A &le t ys));
merge_content :: (A:@)->(le:A->A->Bool)->(xs:List A)->(ys:List A)->
	permutation A (append A xs ys) (mergeBy A &le xs ys);

permutation_exchange := \A:@ => \xs:List A => xs
	@(self => (ys:List A)->permutation A (append A self ys) (append A ys self))
	@nil => (\ys:List A => ys
		@(self => permutation A self (append A self (List A).nil))
		@nil => (permutation A).nil
		@cons h t => (permutation A).keep h t (append A t (List A).nil) *t)
	@cons h t => (\ys:List A =>
		(permutation A).compose ((List A).cons h (append A t ys))
			((List A).cons h (append A ys t)) (append A ys ((List A).cons h t))
			((permutation A).keep h (append A t ys) (append A ys t) (*t ys))
			(permutation_move A h ys t));
permutation_exchange :: (A:@)->(xs:List A)->(ys:List A)->
	permutation A (append A xs ys) (append A ys xs);

split_contents := \A:@ => \parts:Halves A => parts @halves l r => append A l r;
split_content_step := \A:@ => \h:A => \t:List A => \parts:Halves A => parts
	@(self => permutation A t (split_contents A self)->
		permutation A ((List A).cons h t)
			(split_contents A (self @halves l r => (Halves A).halves ((List A).cons h r) l)))
	@halves l r => (\prior:permutation A t (append A l r) =>
		(permutation A).compose ((List A).cons h t) ((List A).cons h (append A l r))
			((List A).cons h (append A r l))
			((permutation A).keep h t (append A l r) prior)
			((permutation A).keep h (append A l r) (append A r l) (permutation_exchange A l r)));
split_content := \A:@ => \xs:List A => xs
	@(self => permutation A self (split_contents A (splitAlternating A self)))
	@nil => (permutation A).nil
	@cons h t => split_content_step A h t (splitAlternating A t) *t;
split_content :: (A:@)->(xs:List A)->permutation A xs (split_contents A (splitAlternating A xs));

merge_fuel_content_parts := \A:@ => \le:A->A->Bool => \n:Nat => \xs:List A =>
	\ih:(values:List A)->permutation A values (merge_sort_fuel_by A &le n values) =>
	\parts:Halves A => parts
	@(self => permutation A xs (split_contents A self)->permutation A xs
		(self @halves l r => mergeBy A &le (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r)))
	@halves l r => (\prior:permutation A xs (append A l r) =>
		(permutation A).compose xs (append A l r)
			(mergeBy A &le (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r)) prior
			((permutation A).compose (append A l r)
				(append A (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r))
				(mergeBy A &le (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r))
				(permutation_append A l (merge_sort_fuel_by A &le n l) (ih l) r (merge_sort_fuel_by A &le n r) (ih r))
				(merge_content A &le (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r))));
merge_fuel_content := \A:@ => \le:A->A->Bool => \fuel:Nat => fuel
	@(n => (xs:List A)->permutation A xs (merge_sort_fuel_by A &le n xs))
	@zero => (\xs:List A => permutation_refl A xs)
	@succ n => (\xs:List A => xs
		@(self => permutation A self (merge_sort_fuel_by A &le (Nat.succ n) self))
		@nil => (permutation A).nil
		@cons h t => merge_fuel_content_parts A &le n ((List A).cons h t) &*n
			(splitAlternating A ((List A).cons h t)) (split_content A ((List A).cons h t)));
merge_fuel_content :: (A:@)->(le:A->A->Bool)->(n:Nat)->(xs:List A)->
	permutation A xs (merge_sort_fuel_by A &le n xs);
merge_content_measured := \A:@ => \le:A->A->Bool => \xs:List A => \out:Measured A => out
	@(self => permutation A xs (self @measured n values => merge_sort_fuel_by A &le n xs))
	@measured n values => merge_fuel_content A &le n xs;
merge_sort_content := \A:@ => \le:A->A->Bool => \xs:List A => merge_content_measured A &le xs (measure A xs);
merge_sort_content :: (A:@)->(le:A->A->Bool)->(xs:List A)->permutation A xs (merge_sort_by A &le xs);

// Fuel zero preserves contents but need not sort. The length bound is essential.
merge_fits := \A:@ => @\n:Nat => @\xs:List A => {
	nil:(bound:Nat)->* bound (List A).nil;
	cons:(bound:Nat)->(h:A)->(t:List A)->* bound t->* (Nat.succ bound) ((List A).cons h t);
};
merge_fits_weaken := \A:@ => \n:Nat => \xs:List A => \p:merge_fits A n xs => p
	@nil bound => (merge_fits A).nil (Nat.succ bound)
	@cons bound h t prior => (merge_fits A).cons (Nat.succ bound) h t *prior;
merge_fits_tail := \A:@ => \n:Nat => \h:A => \t:List A => \p:merge_fits A (Nat.succ n) ((List A).cons h t) => p
	@cons bound x xs prior => prior;
merge_fits_sized := \A:@ => \n:Nat => \v:SizedList A n => \xs:List A => \p:sized_contents A n v xs => p
	@(bound values source self => merge_fits A bound source)
	@nil => (merge_fits A).nil Nat.zero
	@cons bound h t values prior => (merge_fits A).cons bound h values *prior;
merge_fits_zero := \A:@ => \R:A->A->@ => \xs:List A => \p:merge_fits A Nat.zero xs => p
	@nil bound => (general_locally_sorted A R).nil;
merge_fits_sized :: (A:@)->(n:Nat)->(v:SizedList A n)->(xs:List A)->sized_contents A n v xs->merge_fits A n xs;
merge_fits_zero :: (A:@)->(R:A->A->@)->(xs:List A)->merge_fits A Nat.zero xs->general_locally_sorted A R xs;

merge_parts_bound := \A:@ => \n:Nat => @\parts:Halves A => {
	halves:(l:List A)->(r:List A)->merge_fits A n l->merge_fits A n r->* ((Halves A).halves l r);
};
merge_split_bound_step := \A:@ => \n:Nat => \h:A => \parts:Halves A => \p:merge_parts_bound A n parts => p
	@(values self => merge_parts_bound A (Nat.succ n)
		(values @halves l r => (Halves A).halves ((List A).cons h r) l))
	@halves l r left right => (merge_parts_bound A (Nat.succ n)).halves ((List A).cons h r) l
		((merge_fits A).cons n h r right) (merge_fits_weaken A n l left);
merge_split_bound := \A:@ => \n:Nat => \xs:List A => \p:merge_fits A n xs => p
	@(bound values self => merge_parts_bound A bound (splitAlternating A values))
	@nil bound => (merge_parts_bound A bound).halves (List A).nil (List A).nil
		((merge_fits A).nil bound) ((merge_fits A).nil bound)
	@cons bound h t prior => merge_split_bound_step A bound h (splitAlternating A t) *prior;
merge_split_bound :: (A:@)->(n:Nat)->(xs:List A)->merge_fits A n xs->merge_parts_bound A n (splitAlternating A xs);


merge_right_bound := \A:@ => \n:Nat => \parts:Halves A => parts @halves l r => merge_fits A n r;
merge_split_cons_bound := \A:@ => \n:Nat => \h:A => \parts:Halves A => \p:merge_parts_bound A n parts => p
	@(values self => merge_right_bound A n (values @halves l r => (Halves A).halves ((List A).cons h r) l))
	@halves l r left right => left;

merge_fuel_local_parts := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \n:Nat =>
	\ih:(xs:List A)->merge_fits A n xs->general_locally_sorted A R (merge_sort_fuel_by A &le n xs) =>
	\parts:Halves A => parts
	@(self => merge_right_bound A n self->general_locally_sorted A R (self @halves l r =>
		mergeBy A &le (merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r)))
	@halves l r => (\bound:merge_fits A n r => merge_local A R &le decide
		(merge_sort_fuel_by A &le n l) (merge_sort_fuel_by A &le n r) (ih r bound));
merge_fuel_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \fuel:Nat => fuel
	@(n => (xs:List A)->merge_fits A n xs->general_locally_sorted A R (merge_sort_fuel_by A &le n xs))
	@zero => (\xs:List A => merge_fits_zero A R xs)
	@succ n => (\xs:List A => xs
		@(self => merge_fits A (Nat.succ n) self->general_locally_sorted A R (merge_sort_fuel_by A &le (Nat.succ n) self))
		@nil => (\bound:merge_fits A (Nat.succ n) (List A).nil => (general_locally_sorted A R).nil)
		@cons h t => (\bound:merge_fits A (Nat.succ n) ((List A).cons h t) =>
			merge_fuel_local_parts A R &le decide n &*n (splitAlternating A ((List A).cons h t))
				(merge_split_cons_bound A n h (splitAlternating A t)
					(merge_split_bound A n t (merge_fits_tail A n h t bound)))));
merge_fuel_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(n:Nat)->(xs:List A)->
	merge_fits A n xs->general_locally_sorted A R (merge_sort_fuel_by A &le n xs);
merge_local_measured := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	\out:Measured A => \p:measurement_contents A xs out => p
	@(values self => general_locally_sorted A R (values @measured n v => merge_sort_fuel_by A &le n xs))
	@measured n v representation => merge_fuel_local A R &le decide n xs (merge_fits_sized A n v xs representation);
merge_sort_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	merge_local_measured A R &le decide xs (measure A xs) (measure_content_result A xs);
merge_sort_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->
	general_locally_sorted A R (merge_sort_by A &le xs);
merge_backend := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	(sorting_backend A R).mk &(merge_sort_by A &le) &(merge_sort_local A R &le decide) &(merge_sort_content A &le);
merge_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
