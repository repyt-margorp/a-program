// Keep the legacy repeated-insertion merge; this is not linear-time merging.
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

merge_fuel_content_parts := \le:Nat->Nat->Bool => \n:Nat => \xs:List Nat =>
	\ih:(values:List Nat)->permutation Nat values (mergeSortFuel &le n values) =>
	\parts:Halves Nat => parts
	@(self => permutation Nat xs (split_contents Nat self)->permutation Nat xs
		(self @halves l r => mergeBy Nat &le (mergeSortFuel &le n l) (mergeSortFuel &le n r)))
	@halves l r => (\prior:permutation Nat xs (append Nat l r) =>
		(permutation Nat).compose xs (append Nat l r)
			(mergeBy Nat &le (mergeSortFuel &le n l) (mergeSortFuel &le n r)) prior
			((permutation Nat).compose (append Nat l r)
				(append Nat (mergeSortFuel &le n l) (mergeSortFuel &le n r))
				(mergeBy Nat &le (mergeSortFuel &le n l) (mergeSortFuel &le n r))
				(permutation_append Nat l (mergeSortFuel &le n l) (ih l) r (mergeSortFuel &le n r) (ih r))
				(merge_content Nat &le (mergeSortFuel &le n l) (mergeSortFuel &le n r))));
merge_fuel_content := \le:Nat->Nat->Bool => \fuel:Nat => fuel
	@(n => (xs:List Nat)->permutation Nat xs (mergeSortFuel &le n xs))
	@zero => (\xs:List Nat => permutation_refl Nat xs)
	@succ n => (\xs:List Nat => xs
		@(self => permutation Nat self (mergeSortFuel &le (Nat.succ n) self))
		@nil => (permutation Nat).nil
		@cons h t => merge_fuel_content_parts &le n ((List Nat).cons h t) &*n
			(splitAlternating Nat ((List Nat).cons h t)) (split_content Nat ((List Nat).cons h t)));
merge_fuel_content :: (le:Nat->Nat->Bool)->(n:Nat)->(xs:List Nat)->
	permutation Nat xs (mergeSortFuel &le n xs);
merge_content_measured := \le:Nat->Nat->Bool => \xs:List Nat => \out:Measured Nat => out
	@(self => permutation Nat xs (self @measured n values => mergeSortFuel &le n xs))
	@measured n values => merge_fuel_content &le n xs;
merge_sort_content := \le:Nat->Nat->Bool => \xs:List Nat => merge_content_measured &le xs (measure Nat xs);
merge_sort_content :: (le:Nat->Nat->Bool)->(xs:List Nat)->permutation Nat xs (mergeSort &le xs);

// Fuel zero preserves contents but need not sort. The length bound is essential.
merge_fits := @\n:Nat => @\xs:List Nat => {
	nil:(bound:Nat)->* bound (List Nat).nil;
	cons:(bound:Nat)->(h:Nat)->(t:List Nat)->* bound t->* (Nat.succ bound) ((List Nat).cons h t);
};
merge_fits_weaken := \n:Nat => \xs:List Nat => \p:merge_fits n xs => p
	@nil bound => merge_fits.nil (Nat.succ bound)
	@cons bound h t prior => merge_fits.cons (Nat.succ bound) h t *prior;
merge_fits_tail := \n:Nat => \h:Nat => \t:List Nat => \p:merge_fits (Nat.succ n) ((List Nat).cons h t) => p
	@cons bound x xs prior => prior;
merge_fits_sized := \n:Nat => \v:SizedList Nat n => \xs:List Nat => \p:sized_contents Nat n v xs => p
	@(bound values source self => merge_fits bound source)
	@nil => merge_fits.nil Nat.zero
	@cons bound h t values prior => merge_fits.cons bound h values *prior;
merge_fits_zero := \R:Nat->Nat->@ => \xs:List Nat => \p:merge_fits Nat.zero xs => p
	@nil bound => (general_locally_sorted Nat R).nil;
merge_fits_sized :: (n:Nat)->(v:SizedList Nat n)->(xs:List Nat)->sized_contents Nat n v xs->merge_fits n xs;
merge_fits_zero :: (R:Nat->Nat->@)->(xs:List Nat)->merge_fits Nat.zero xs->general_locally_sorted Nat R xs;

merge_parts_bound := \n:Nat => @\parts:Halves Nat => {
	halves:(l:List Nat)->(r:List Nat)->merge_fits n l->merge_fits n r->* ((Halves Nat).halves l r);
};
merge_split_bound_step := \n:Nat => \h:Nat => \parts:Halves Nat => \p:merge_parts_bound n parts => p
	@(values self => merge_parts_bound (Nat.succ n)
		(values @halves l r => (Halves Nat).halves ((List Nat).cons h r) l))
	@halves l r left right => (merge_parts_bound (Nat.succ n)).halves ((List Nat).cons h r) l
		(merge_fits.cons n h r right) (merge_fits_weaken n l left);
merge_split_bound := \n:Nat => \xs:List Nat => \p:merge_fits n xs => p
	@(bound values self => merge_parts_bound bound (splitAlternating Nat values))
	@nil bound => (merge_parts_bound bound).halves (List Nat).nil (List Nat).nil
		(merge_fits.nil bound) (merge_fits.nil bound)
	@cons bound h t prior => merge_split_bound_step bound h (splitAlternating Nat t) *prior;
merge_split_bound :: (n:Nat)->(xs:List Nat)->merge_fits n xs->merge_parts_bound n (splitAlternating Nat xs);


merge_right_bound := \n:Nat => \parts:Halves Nat => parts @halves l r => merge_fits n r;
merge_split_cons_bound := \n:Nat => \h:Nat => \parts:Halves Nat => \p:merge_parts_bound n parts => p
	@(values self => merge_right_bound n (values @halves l r => (Halves Nat).halves ((List Nat).cons h r) l))
	@halves l r left right => left;

merge_fuel_local_parts := \R:Nat->Nat->@ => \le:Nat->Nat->Bool =>
	\decide:(x:Nat)->(y:Nat)->general_decision Nat R x y (le x y) => \n:Nat =>
	\ih:(xs:List Nat)->merge_fits n xs->general_locally_sorted Nat R (mergeSortFuel &le n xs) =>
	\parts:Halves Nat => parts
	@(self => merge_right_bound n self->general_locally_sorted Nat R (self @halves l r =>
		mergeBy Nat &le (mergeSortFuel &le n l) (mergeSortFuel &le n r)))
	@halves l r => (\bound:merge_fits n r => merge_local Nat R &le decide
		(mergeSortFuel &le n l) (mergeSortFuel &le n r) (ih r bound));
merge_fuel_local := \R:Nat->Nat->@ => \le:Nat->Nat->Bool =>
	\decide:(x:Nat)->(y:Nat)->general_decision Nat R x y (le x y) => \fuel:Nat => fuel
	@(n => (xs:List Nat)->merge_fits n xs->general_locally_sorted Nat R (mergeSortFuel &le n xs))
	@zero => (\xs:List Nat => merge_fits_zero R xs)
	@succ n => (\xs:List Nat => xs
		@(self => merge_fits (Nat.succ n) self->general_locally_sorted Nat R (mergeSortFuel &le (Nat.succ n) self))
		@nil => (\bound:merge_fits (Nat.succ n) (List Nat).nil => (general_locally_sorted Nat R).nil)
		@cons h t => (\bound:merge_fits (Nat.succ n) ((List Nat).cons h t) =>
			merge_fuel_local_parts R &le decide n &*n (splitAlternating Nat ((List Nat).cons h t))
				(merge_split_cons_bound n h (splitAlternating Nat t)
					(merge_split_bound n t (merge_fits_tail n h t bound)))));
merge_fuel_local :: (R:Nat->Nat->@)->(le:Nat->Nat->Bool)->
	((x:Nat)->(y:Nat)->general_decision Nat R x y (le x y))->(n:Nat)->(xs:List Nat)->
	merge_fits n xs->general_locally_sorted Nat R (mergeSortFuel &le n xs);
merge_local_measured := \R:Nat->Nat->@ => \le:Nat->Nat->Bool =>
	\decide:(x:Nat)->(y:Nat)->general_decision Nat R x y (le x y) => \xs:List Nat =>
	\out:Measured Nat => \p:measurement_contents Nat xs out => p
	@(values self => general_locally_sorted Nat R (values @measured n v => mergeSortFuel &le n xs))
	@measured n v representation => merge_fuel_local R &le decide n xs (merge_fits_sized n v xs representation);
merge_sort_local := \R:Nat->Nat->@ => \le:Nat->Nat->Bool =>
	\decide:(x:Nat)->(y:Nat)->general_decision Nat R x y (le x y) => \xs:List Nat =>
	merge_local_measured R &le decide xs (measure Nat xs) (measure_content_result Nat xs);
merge_sort_local :: (R:Nat->Nat->@)->(le:Nat->Nat->Bool)->
	((x:Nat)->(y:Nat)->general_decision Nat R x y (le x y))->(xs:List Nat)->
	general_locally_sorted Nat R (mergeSort &le xs);
merge_backend := \R:Nat->Nat->@ => \le:Nat->Nat->Bool =>
	\decide:(x:Nat)->(y:Nat)->general_decision Nat R x y (le x y) =>
	(sorting_backend Nat R).mk &(mergeSort &le) &(merge_sort_local R &le decide) &(merge_sort_content &le);
merge_backend :: (R:Nat->Nat->@)->(le:Nat->Nat->Bool)->
	((x:Nat)->(y:Nat)->general_decision Nat R x y (le x y))->sorting_backend Nat R;
