// Nat specialization of the generic backend; positive labels share a key.
merge_key := \n:Nat => n @zero => Bool.false @succ k => Bool.true;
merge_order := \x:Nat => \y:Nat => bool_order (merge_key x) (merge_key y);
merge_compare := \x:Nat => \y:Nat => bool_le (merge_key x) (merge_key y);
merge_decision := \x:Nat => \y:Nat => \d:Bool =>
	\p:general_decision Bool &bool_order (merge_key x) (merge_key y) d => p
	@(answer self => general_decision Nat &merge_order x y answer)
	@yes edge => (general_decision Nat &merge_order x y).yes edge
	@no edge => (general_decision Nat &merge_order x y).no edge;
merge_decide := \x:Nat => \y:Nat =>
	merge_decision x y (merge_compare x y) (bool_decide (merge_key x) (merge_key y));
merge_trans := \x:Nat => \y:Nat => \p:merge_order x y => \z:Nat => \q:merge_order y z =>
	bool_trans (merge_key x) (merge_key y) p (merge_key z) q;
legacy_merge := merge_backend Nat &merge_order &merge_compare &merge_decide;
merge_one := Nat.succ Nat.zero;
merge_two := Nat.succ merge_one;
merge_three := Nat.succ merge_two;
merge_empty := (List Nat).nil;
merge_single := (List Nat).cons merge_one merge_empty;
merge_reversed := (List Nat).cons merge_one ((List Nat).cons Nat.zero merge_empty);
merge_ordered := (List Nat).cons Nat.zero ((List Nat).cons merge_one merge_empty);
merge_duplicates := (List Nat).cons merge_two ((List Nat).cons merge_one ((List Nat).cons Nat.zero merge_empty));
merge_expected := (List Nat).cons Nat.zero ((List Nat).cons merge_two ((List Nat).cons merge_one merge_empty));
merge_certificate := sorting_content Nat &merge_order legacy_merge merge_duplicates;
merge_certificate :: permutation Nat merge_duplicates (merge_sort_by Nat &merge_compare merge_duplicates);
merge_local_certificate := sorting_local Nat &merge_order legacy_merge merge_duplicates;
merge_local_certificate :: general_locally_sorted Nat &merge_order (merge_sort_by Nat &merge_compare merge_duplicates);
merge_strong_certificate := sorting_strong Nat &merge_order &merge_trans legacy_merge merge_duplicates;
merge_strong_certificate :: general_strongly_sorted Nat &merge_order (merge_sort_by Nat &merge_compare merge_duplicates);

merge_results := @{mk:List Nat->List Nat->List Nat->List Nat->List Nat->*;};
merge_report := merge_results.mk
	(sorting_run Nat &merge_order legacy_merge merge_empty)
	(sorting_run Nat &merge_order legacy_merge merge_single)
	(sorting_run Nat &merge_order legacy_merge merge_reversed)
	(sorting_run Nat &merge_order legacy_merge merge_ordered)
	(sorting_run Nat &merge_order legacy_merge merge_duplicates);
merge_report_expected := merge_results.mk merge_empty merge_single merge_ordered merge_ordered merge_expected;
merge_report_wrong := merge_results.mk merge_empty merge_single merge_ordered merge_ordered merge_duplicates;

// Content preservation holds even at zero fuel; sortedness does not.
merge_unfinished := merge_sort_fuel_by Nat &merge_compare Nat.zero merge_reversed;
merge_unfinished_content := merge_fuel_content Nat &merge_compare Nat.zero merge_reversed;
merge_unfinished_content :: permutation Nat merge_reversed merge_unfinished;
merge_size := (list_size Nat).cons merge_two ((list_size Nat).cons merge_one
	((list_size Nat).cons Nat.zero (list_size Nat).nil));
merge_input_vector := (Vec Nat).cons merge_two ((Vec Nat).cons merge_one ((Vec Nat).cons Nat.zero (Vec Nat).nil));
merge_vector := vec_contents Nat merge_three (sorting_vector Nat &merge_order legacy_merge merge_three merge_input_vector);
merge_origin := position_forward merge_three
	(sorting_positions Nat &merge_order legacy_merge merge_three merge_duplicates merge_size) (Fin.zero merge_two);
merge_origin_expected := Fin.succ (Fin.succ (Fin.zero Nat.zero));

// The accepted fixture remains untouched. Compare the old algorithm explicitly.
merge_legacy_report := merge_results.mk
	(mergeSort &merge_compare merge_empty)
	(mergeSort &merge_compare merge_single)
	(mergeSort &merge_compare merge_reversed)
	(mergeSort &merge_compare merge_ordered)
	(mergeSort &merge_compare merge_duplicates);
