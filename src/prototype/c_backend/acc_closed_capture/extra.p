// The actual source selects comparator operands in its closed capture.
forward_compare := \left : Nat => \right : Nat => natLessOrEqual left right;
forward_sort := quickSort Nat &forward_compare;
reference_forward := print_list (forward_sort mixed);
// These pure-total source comparators are outside the bounded reader.
constant_compare := \left : Nat => \right : Nat => Bool.true;
constant_sort := quickSort Nat &constant_compare;
repeated_compare := \left : Nat => \right : Nat => natLessOrEqual left left;
repeated_sort := quickSort Nat &repeated_compare;

// An actual closed Bool value remains comparator context through Acc recursion.
choose_compare := \reverse : Bool => \left : Nat => \right : Nat =>
	reverse @true => natLessOrEqual right left @false => natLessOrEqual left right;
true_sort := quickSort Nat &(choose_compare Bool.true);
false_sort := quickSort Nat &(choose_compare Bool.false);
reference_true := print_list (true_sort mixed);
reference_false := print_list (false_sort mixed);
// Opposite source clauses ensure constructor positions are not C truth guesses.
opposite_compare := \reverse : Bool => \left : Nat => \right : Nat =>
	reverse @true => natLessOrEqual left right @false => natLessOrEqual right left;
opposite_true_sort := quickSort Nat &(opposite_compare Bool.true);
opposite_false_sort := quickSort Nat &(opposite_compare Bool.false);
reference_opposite_true := print_list (opposite_true_sort mixed);
reference_opposite_false := print_list (opposite_false_sort mixed);
// Unsupported environments/unused fields/bad inactive branches remain explicit.
unused_mode_compare := \reverse : Bool => \left : Nat => \right : Nat => natLessOrEqual left right;
unused_mode_sort := quickSort Nat &(unused_mode_compare Bool.true);
nat_mode_compare := \mode : Nat => \left : Nat => \right : Nat =>
	mode @zero => natLessOrEqual left right @succ prior => natLessOrEqual right left;
nat_mode_sort := quickSort Nat &(nat_mode_compare Nat.zero);
bad_mode_compare := \reverse : Bool => \left : Nat => \right : Nat =>
	reverse @true => natLessOrEqual left left @false => natLessOrEqual right left;
bad_mode_sort := quickSort Nat &(bad_mode_compare Bool.false);
