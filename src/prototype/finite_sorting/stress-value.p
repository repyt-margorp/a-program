// Append after cases.p. Diagnostic, not a passing acceptance test.
// At baseline 7160cbe the full source remains pending at 100M solver steps.
// The general laws in common.p are checked; specializing the predicate's
// concrete value forces much larger conversion than evaluating the observations.
quick_value_law := sorting_vector_value Item &item_order quick_items three vector_input first
	&(\v:Item => same_label two (item_label v)) (same_label.refl two);
insertion_value_law := sorting_value Item &item_order insertion_items three duplicates input_size first
	&(\v:Item => same_label one (item_label v)) (same_label.refl one);
