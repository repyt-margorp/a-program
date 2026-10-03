// The same finite-function API observes all five unchanged ordinary backends.
finite_tree := tree_backend Item &item_order &item_le &item_decide;
finite_merge := merge_backend Item &item_order &item_le &item_decide;
finite_bubble := bubble_transitive_backend Item &item_order &item_le &item_decide &item_trans;
finite_values := \i:Fin three => vec_lookup Item three vector_input i;
finite_output := \backend:sorting_backend Item &item_order =>
	vec_contents Item three (vec_tabulate Item three &(sorting_function Item &item_order backend three &finite_values));
finite_restored := \backend:sorting_backend Item &item_order =>
	vec_contents Item three (vec_tabulate Item three &(sorting_function_restore Item &item_order backend three &finite_values));
finite_positions := \backend:sorting_backend Item &item_order =>
	sorting_function_positions Item &item_order backend three &finite_values;
finite_origins := \backend:sorting_backend Item &item_order =>
	labels (fin_to_nat three (position_forward three (finite_positions backend) first))
		(fin_to_nat three (position_forward three (finite_positions backend) second))
		(fin_to_nat three (position_forward three (finite_positions backend) third));
finite_destinations := \backend:sorting_backend Item &item_order =>
	labels (fin_to_nat three (position_backward three (finite_positions backend) first))
		(fin_to_nat three (position_backward three (finite_positions backend) second))
		(fin_to_nat three (position_backward three (finite_positions backend) third));
finite_five := \a:List Item => \b:List Item => \c:List Item => \d:List Item => \e:List Item =>
	(List (List Item)).cons a ((List (List Item)).cons b ((List (List Item)).cons c
		((List (List Item)).cons d ((List (List Item)).cons e (List (List Item)).nil))));
finite_five_positions := \a:List Nat => \b:List Nat => \c:List Nat => \d:List Nat => \e:List Nat =>
	(List (List Nat)).cons a ((List (List Nat)).cons b ((List (List Nat)).cons c
		((List (List Nat)).cons d ((List (List Nat)).cons e (List (List Nat)).nil))));
finite_report_type := @{mk:List (List Item)->List (List Item)->List (List Nat)->List (List Nat)->*;};
finite_report := finite_report_type.mk
	(finite_five (finite_output quick_items) (finite_output insertion_items) (finite_output finite_tree)
		(finite_output finite_merge) (finite_output finite_bubble))
	(finite_five (finite_restored quick_items) (finite_restored insertion_items) (finite_restored finite_tree)
		(finite_restored finite_merge) (finite_restored finite_bubble))
	(finite_five_positions (finite_origins quick_items) (finite_origins insertion_items) (finite_origins finite_tree)
		(finite_origins finite_merge) (finite_origins finite_bubble))
	(finite_five_positions (finite_destinations quick_items) (finite_destinations insertion_items) (finite_destinations finite_tree)
		(finite_destinations finite_merge) (finite_destinations finite_bubble));
finite_report_expected := finite_report_type.mk
	(finite_five quick_expected insertion_expected insertion_expected quick_expected insertion_expected)
	(finite_five duplicates duplicates duplicates duplicates duplicates)
	(finite_five_positions (labels two one zero) (labels one two zero) (labels one two zero)
		(labels two one zero) (labels one two zero))
	(finite_five_positions (labels two one zero) (labels two zero one) (labels two zero one)
		(labels two one zero) (labels two zero one));

// Original positions select different destinations even when their keys coincide.
finite_recovery_one := \backend:sorting_backend Item &item_order =>
	sorting_function_recover Item &item_order backend three &finite_values second
		&(\v:Item => same_label one (item_label v)) (same_label.refl one);
finite_recovery_two := \backend:sorting_backend Item &item_order =>
	sorting_function_recover Item &item_order backend three &finite_values third
		&(\v:Item => same_label two (item_label v)) (same_label.refl two);
finite_recovery_return := \backend:sorting_backend Item &item_order =>
	sorting_function_recover_back Item &item_order backend three &finite_values second
		&(\v:Item => same_label one (item_label v)) (finite_recovery_one backend);
finite_merge_value := sorting_function_value_back Item &item_order finite_merge three &finite_values first
	&(\v:Item => same_label two (item_label v)) (same_label.refl two);
finite_merge_value_return := sorting_function_value Item &item_order finite_merge three &finite_values first
	&(\v:Item => same_label two (item_label v)) finite_merge_value;
finite_merge_content := sorting_function_content Item &item_order finite_merge three &finite_values;
finite_merge_local := sorting_function_local Item &item_order finite_merge three &finite_values;
finite_merge_strong := sorting_function_strong Item &item_order &item_trans finite_merge three &finite_values;

// Empty and singleton functions use the same tabulation/result path.
finite_empty_values := \i:Fin zero => vec_lookup Item zero (Vec Item).nil i;
finite_single_values := \i:Fin one => high;
finite_small_type := @{mk:List Item->List Item->List Item->*;};
finite_small_report := finite_small_type.mk
	(sorting_function_contents Item &item_order finite_merge zero &finite_empty_values)
	(sorting_function_contents Item &item_order finite_merge one &finite_single_values)
	(vec_contents Item one (vec_tabulate Item one
		&(sorting_function_restore Item &item_order finite_merge one &finite_single_values)));
finite_small_expected := finite_small_type.mk empty singleton singleton;
