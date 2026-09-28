bubble_items := bubble_transitive_backend Item &item_order &item_le &item_decide &item_trans;
bubble_local_certificate := \xs:List Item => sorting_local Item &item_order bubble_items xs;
bubble_local_certificate :: (xs:List Item)->general_locally_sorted Item &item_order (bubble_sort Item &item_le xs);
bubble_content_certificate := \xs:List Item => sorting_content Item &item_order bubble_items xs;
bubble_content_certificate :: (xs:List Item)->permutation Item xs (bubble_sort Item &item_le xs);
bubble_strong_certificate := \xs:List Item => sorting_strong Item &item_order &item_trans bubble_items xs;
bubble_strong_certificate :: (xs:List Item)->general_strongly_sorted Item &item_order (bubble_sort Item &item_le xs);
bubble_vector := vec_contents Item three (checked_vector bubble_items three vector_input);
bubble_positions := sorting_positions Item &item_order bubble_items three duplicates input_size;
bubble_report := Report.mk
	(outputs (checked_list bubble_items empty) (checked_list bubble_items singleton)
		(checked_list bubble_items reversed) (checked_list bubble_items ordered)
		(checked_list bubble_items duplicates) (bubble_sort Item &item_le duplicates) bubble_vector)
	(labels (origin_label bubble_positions first) (origin_label bubble_positions second) (origin_label bubble_positions third));
bubble_report_expected := insertion_report_expected;
bubble_report_wrong := quick_report_expected;
bubble_value := sorting_vector_value Item &item_order bubble_items three vector_input first
	&(\v:Item => same_label one (item_label v)) (same_label.refl one);
bubble_value :: same_label one (item_label (vec_lookup Item three vector_input
	(position_forward three bubble_positions first)));
// One pass exchanges neighbors but does not sort the whole suffix.
bubble_input := (SizedList Item).cons two high ((SizedList Item).cons one low_one
	((SizedList Item).cons zero low_two (SizedList Item).nil));
bubble_once := sized_values Item three (bubble_pass Item &item_le three bubble_input);
bubble_once_expected := (List Item).cons low_one ((List Item).cons high ((List Item).cons low_two empty));
