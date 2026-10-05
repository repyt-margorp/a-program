// Runtime source data must survive both actual Acc recursive calls.
runtime_sort := \reverse : Bool => quickSort Nat &(choose_compare reverse);
runtime_true := print_list (runtime_sort Bool.true mixed);
runtime_false := print_list (runtime_sort Bool.false mixed);
opposite_runtime_sort := \reverse : Bool => quickSort Nat &(opposite_compare reverse);
opposite_runtime_true := print_list (opposite_runtime_sort Bool.true mixed);
opposite_runtime_false := print_list (opposite_runtime_sort Bool.false mixed);
// Ordinary admitted wrappers outside the explicit runtime capture contract.
unused_runtime_sort := \reverse : Bool => quickSort Nat &(choose_compare Bool.true);
nat_runtime_sort := \mode : Nat => quickSort Nat &(choose_compare Bool.true);
bad_runtime_sort := \reverse : Bool => quickSort Nat &(bad_mode_compare reverse);
extra_runtime_sort := \reverse : Bool => \other : Bool => quickSort Nat &(choose_compare reverse);
wrong_runtime_result := \reverse : Bool => reverse;
