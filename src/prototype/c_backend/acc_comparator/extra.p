// The actual comparator stays a parameter rather than a closed Nat choice.
outer_parameter := quickSort Nat;
reverse_compare := \left : Nat => \right : Nat => natLessOrEqual right left;
descending_sort := quickSort Nat &reverse_compare;
reference_descending := print_list (descending_sort mixed);
// Admitted parameter shapes with unsupported bodies must remain refused.
wrong_parameter := \compare : Nat -> Nat -> Bool => \xs : List Nat => xs;
unary_parameter := \compare : Nat -> Bool => \xs : List Nat => xs;
