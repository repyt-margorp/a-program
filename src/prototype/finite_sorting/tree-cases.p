// Reuse the labelled Boolean-key cases shared by the other backends.
tree_items := tree_backend Item &item_order &item_le &item_decide;
tree_local_certificate := \xs:List Item => sorting_local Item &item_order tree_items xs;
tree_local_certificate :: (xs:List Item)->general_locally_sorted Item &item_order (treeSort Item &item_le xs);
tree_content_certificate := \xs:List Item => sorting_content Item &item_order tree_items xs;
tree_content_certificate :: (xs:List Item)->permutation Item xs (treeSort Item &item_le xs);
tree_strong_certificate := \xs:List Item => sorting_strong Item &item_order &item_trans tree_items xs;
tree_strong_certificate :: (xs:List Item)->general_strongly_sorted Item &item_order (treeSort Item &item_le xs);
tree_vector := vec_contents Item three (checked_vector tree_items three vector_input);
tree_positions := sorting_positions Item &item_order tree_items three duplicates input_size;
tree_report := Report.mk
	(outputs (checked_list tree_items empty) (checked_list tree_items singleton)
		(checked_list tree_items reversed) (checked_list tree_items ordered)
		(checked_list tree_items duplicates) (treeSort Item &item_le duplicates) tree_vector)
	(labels (origin_label tree_positions first) (origin_label tree_positions second) (origin_label tree_positions third));
tree_report_expected := insertion_report_expected;
tree_report_wrong := quick_report_expected;
tree_value := sorting_vector_value Item &item_order tree_items three vector_input first
	&(\v:Item => same_label one (item_label v)) (same_label.refl one);
tree_value :: same_label one (item_label (vec_lookup Item three vector_input
	(position_forward three tree_positions first)));
