// Append after cases.p. Must never be accepted. At 7160cbe this comparison
// remains pending at 20M steps; this is not a verified rejection.
bad := (sorting_backend Item &item_order).mk &(\xs:List Item => xs)
	&(sorting_local Item &item_order quick_items) &(sorting_content Item &item_order quick_items);
