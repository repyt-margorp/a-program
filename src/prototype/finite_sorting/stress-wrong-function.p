// Append after cases.p. Must never be accepted. Pending at 20M on 7160cbe,
// and at 80M on the closed-readback candidate (2026-09-28), not a rejection.
bad := (sorting_backend Item &item_order).mk &(\xs:List Item => xs)
	&(sorting_local Item &item_order quick_items) &(sorting_content Item &item_order quick_items);
