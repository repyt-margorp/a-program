// Arbitrary-input consumers check the actual generic function, not just samples.
merge_generic_content := \A:@ => \le:A->A->Bool => \xs:List A =>
	merge_sort_content A &le xs;
merge_generic_content :: (A:@)->(le:A->A->Bool)->(xs:List A)->
	permutation A xs (merge_sort_by A &le xs);
merge_generic_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	sorting_local A R (merge_backend A R &le decide) xs;
merge_generic_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->
	general_locally_sorted A R (merge_sort_by A &le xs);
merge_generic_strong := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z => \xs:List A =>
	sorting_strong A R trans (merge_backend A R &le decide) xs;
merge_generic_strong :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(xs:List A)->
	general_strongly_sorted A R (merge_sort_by A &le xs);
merge_generic_positions := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs => sorting_positions A R backend n xs size;
merge_generic_positions :: (A:@)->(R:A->A->@)->sorting_backend A R->
	(n:Nat)->(xs:List A)->list_size A n xs->position_permutation n;
merge_bool_backend := merge_backend Bool &bool_order &bool_le &bool_decide;
merge_bool_empty := (List Bool).nil;
merge_bool_single := (List Bool).cons Bool.true merge_bool_empty;
merge_bool_reversed := (List Bool).cons Bool.true ((List Bool).cons Bool.false merge_bool_empty);
merge_bool_ordered := (List Bool).cons Bool.false merge_bool_single;
merge_bool_duplicates := (List Bool).cons Bool.true ((List Bool).cons Bool.false merge_bool_single);
merge_bool_expected := (List Bool).cons Bool.false ((List Bool).cons Bool.true merge_bool_single);
merge_bool_results := @{mk:List Bool->List Bool->List Bool->List Bool->List Bool->*;};
merge_bool_report := merge_bool_results.mk
	(sorting_run Bool &bool_order merge_bool_backend merge_bool_empty)
	(sorting_run Bool &bool_order merge_bool_backend merge_bool_single)
	(sorting_run Bool &bool_order merge_bool_backend merge_bool_reversed)
	(sorting_run Bool &bool_order merge_bool_backend merge_bool_ordered)
	(sorting_run Bool &bool_order merge_bool_backend merge_bool_duplicates);
merge_bool_report_expected := merge_bool_results.mk merge_bool_empty merge_bool_single
	merge_bool_ordered merge_bool_ordered merge_bool_expected;

// Equal keys retain their full, distinct payloads through the Fin bridge.
merge_item := @{mk:Bool->Nat->*;};
merge_item_key := \x:merge_item => x @mk key label => key;
merge_item_label := \x:merge_item => x @mk key label => label;
merge_item_order := \x:merge_item => \y:merge_item => bool_order (merge_item_key x) (merge_item_key y);
merge_item_compare := \x:merge_item => \y:merge_item => bool_le (merge_item_key x) (merge_item_key y);
merge_item_decision := \x:merge_item => \y:merge_item => \d:Bool =>
	\p:general_decision Bool &bool_order (merge_item_key x) (merge_item_key y) d => p
	@(answer self => general_decision merge_item &merge_item_order x y answer)
	@yes edge => (general_decision merge_item &merge_item_order x y).yes edge
	@no edge => (general_decision merge_item &merge_item_order x y).no edge;
merge_item_decide := \x:merge_item => \y:merge_item => merge_item_decision x y
	(merge_item_compare x y) (bool_decide (merge_item_key x) (merge_item_key y));
merge_item_trans := \x:merge_item => \y:merge_item => \p:merge_item_order x y =>
	\z:merge_item => \q:merge_item_order y z =>
	bool_trans (merge_item_key x) (merge_item_key y) p (merge_item_key z) q;
merge_item_backend := merge_backend merge_item &merge_item_order &merge_item_compare &merge_item_decide;
merge_item_zero := Nat.zero;
merge_item_one := Nat.succ merge_item_zero;
merge_item_two := Nat.succ merge_item_one;
merge_item_three := Nat.succ merge_item_two;
merge_high := merge_item.mk Bool.true merge_item_zero;
merge_low_one := merge_item.mk Bool.false merge_item_one;
merge_low_two := merge_item.mk Bool.false merge_item_two;
merge_item_input := (List merge_item).cons merge_high
	((List merge_item).cons merge_low_one ((List merge_item).cons merge_low_two (List merge_item).nil));
merge_item_expected := (List merge_item).cons merge_low_two
	((List merge_item).cons merge_low_one ((List merge_item).cons merge_high (List merge_item).nil));
merge_item_output := sorting_run merge_item &merge_item_order merge_item_backend merge_item_input;
merge_item_vector_input := (Vec merge_item).cons merge_high
	((Vec merge_item).cons merge_low_one ((Vec merge_item).cons merge_low_two (Vec merge_item).nil));
merge_item_vector := vec_contents merge_item merge_item_three
	(sorting_vector merge_item &merge_item_order merge_item_backend merge_item_three merge_item_vector_input);
merge_item_size := list_sized merge_item merge_item_input;
merge_item_positions := sorting_positions merge_item &merge_item_order merge_item_backend
	merge_item_three merge_item_input merge_item_size;
merge_item_first := Fin.zero merge_item_two;
merge_item_second := Fin.succ (Fin.zero merge_item_one);
merge_item_third := Fin.succ (Fin.succ (Fin.zero merge_item_zero));
merge_item_origin := \i:Fin merge_item_three => merge_item_label
	(sized_lookup merge_item merge_item_three merge_item_input merge_item_size
		(position_forward merge_item_three merge_item_positions i));
merge_item_origins := (List Nat).cons (merge_item_origin merge_item_first)
	((List Nat).cons (merge_item_origin merge_item_second)
		((List Nat).cons (merge_item_origin merge_item_third) (List Nat).nil));
merge_item_origins_expected := (List Nat).cons merge_item_two
	((List Nat).cons merge_item_one ((List Nat).cons merge_item_zero (List Nat).nil));
merge_item_same_label := @\left:Nat => @\right:Nat => {refl:(n:Nat)->* n n;};
merge_item_value := sorting_vector_value merge_item &merge_item_order merge_item_backend
	merge_item_three merge_item_vector_input merge_item_first
	&(\v:merge_item => merge_item_same_label merge_item_two (merge_item_label v))
	(merge_item_same_label.refl merge_item_two);
merge_item_inverse := position_left_inverse merge_item_three merge_item_positions;
merge_item_inverse_back := position_right_inverse merge_item_three merge_item_positions;
merge_item_vector_content := sorting_vector_content merge_item &merge_item_order merge_item_backend
	merge_item_three merge_item_vector_input;
merge_item_vector_local := sorting_vector_local merge_item &merge_item_order merge_item_backend
	merge_item_three merge_item_vector_input;
merge_item_strong := sorting_strong merge_item &merge_item_order &merge_item_trans merge_item_backend merge_item_input;
merge_item_strong :: general_strongly_sorted merge_item &merge_item_order (merge_sort_by merge_item &merge_item_compare merge_item_input);
