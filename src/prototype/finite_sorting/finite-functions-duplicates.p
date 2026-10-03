// Identical full payloads still have distinct old/new Fin positions.
finite_repeat_vector := (Vec Item).cons high
	((Vec Item).cons low_one ((Vec Item).cons low_one (Vec Item).nil));
finite_repeat_values := \i:Fin three => vec_lookup Item three finite_repeat_vector i;
finite_repeat_input := (List Item).cons high ((List Item).cons low_one ((List Item).cons low_one empty));
finite_repeat_expected := (List Item).cons low_one ((List Item).cons low_one singleton);
finite_repeat_output := \backend:sorting_backend Item &item_order =>
	vec_contents Item three (vec_tabulate Item three
		&(sorting_function Item &item_order backend three &finite_repeat_values));
finite_repeat_restored := \backend:sorting_backend Item &item_order =>
	vec_contents Item three (vec_tabulate Item three
		&(sorting_function_restore Item &item_order backend three &finite_repeat_values));
finite_repeat_positions := \backend:sorting_backend Item &item_order =>
	sorting_function_positions Item &item_order backend three &finite_repeat_values;
finite_repeat_left_inverse := \backend:sorting_backend Item &item_order =>
	position_left_inverse three (finite_repeat_positions backend);
finite_repeat_right_inverse := \backend:sorting_backend Item &item_order =>
	position_right_inverse three (finite_repeat_positions backend);
finite_repeat_content := sorting_function_content Item &item_order finite_merge three &finite_repeat_values;
finite_repeat_origins := \backend:sorting_backend Item &item_order =>
	labels (fin_to_nat three (position_forward three (finite_repeat_positions backend) first))
		(fin_to_nat three (position_forward three (finite_repeat_positions backend) second))
		(fin_to_nat three (position_forward three (finite_repeat_positions backend) third));
finite_repeat_destinations := \backend:sorting_backend Item &item_order =>
	labels (fin_to_nat three (position_backward three (finite_repeat_positions backend) first))
		(fin_to_nat three (position_backward three (finite_repeat_positions backend) second))
		(fin_to_nat three (position_backward three (finite_repeat_positions backend) third));
finite_repeat_report := finite_report_type.mk
	(finite_five (finite_repeat_output quick_items) (finite_repeat_output insertion_items)
		(finite_repeat_output finite_tree) (finite_repeat_output finite_merge) (finite_repeat_output finite_bubble))
	(finite_five (finite_repeat_restored quick_items) (finite_repeat_restored insertion_items)
		(finite_repeat_restored finite_tree) (finite_repeat_restored finite_merge) (finite_repeat_restored finite_bubble))
	(finite_five_positions (finite_repeat_origins quick_items) (finite_repeat_origins insertion_items)
		(finite_repeat_origins finite_tree) (finite_repeat_origins finite_merge) (finite_repeat_origins finite_bubble))
	(finite_five_positions (finite_repeat_destinations quick_items) (finite_repeat_destinations insertion_items)
		(finite_repeat_destinations finite_tree) (finite_repeat_destinations finite_merge) (finite_repeat_destinations finite_bubble));
finite_repeat_report_expected := finite_report_type.mk
	(finite_five finite_repeat_expected finite_repeat_expected finite_repeat_expected finite_repeat_expected finite_repeat_expected)
	(finite_five finite_repeat_input finite_repeat_input finite_repeat_input finite_repeat_input finite_repeat_input)
	(finite_five_positions (labels two one zero) (labels one two zero) (labels one two zero)
		(labels two one zero) (labels one two zero))
	(finite_five_positions (labels two one zero) (labels two zero one) (labels two zero one)
		(labels two one zero) (labels two zero one));
