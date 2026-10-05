// The actual source selects comparator operands in its closed capture.
forward_compare := \left : Nat => \right : Nat => natLessOrEqual left right;
forward_sort := quickSort Nat &forward_compare;
reference_forward := print_list (forward_sort mixed);
// These pure-total source comparators are outside the bounded reader.
constant_compare := \left : Nat => \right : Nat => Bool.true;
constant_sort := quickSort Nat &constant_compare;
repeated_compare := \left : Nat => \right : Nat => natLessOrEqual left left;
repeated_sort := quickSort Nat &repeated_compare;
