import Bool;
import Nat;
import LT;
import Acc;
import SizedList;
import List;
import Measured;
import Partition;
import natLessOrEqual;
import quickSortAcc;
import partition;
import partitionByDecision;
import partitionLower;
import partitionUpper;
import append;
import quickSort;

bool_type := Bool;
nat_type := Nat;
lt_family := LT;
acc_family := Acc Nat LT;
sized_family := SizedList Nat;
measured_type := Measured Nat;
partition_type := Partition Nat Nat.zero;
list_type := List Nat;
empty := (List Nat).nil;
sort_acc := quickSortAcc Nat &natLessOrEqual;
partition_fn := partition;
partition_nat := partition Nat &natLessOrEqual;
decision_fn := partitionByDecision;
lower_fn := partitionLower;
upper_fn := partitionUpper;
append_fn := append;
append_nat := append Nat;
sort := \xs : List Nat => quickSort Nat &natLessOrEqual xs;
count := \n : Nat => n @zero => #0 @succ k => #int_add #1 *k;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));

// Noncandidate changed-source control: discard each left head.
// Only direct append observations test this mutation; it is not QuickSort.
drop_append := \A : @ => \left : List A => left @nil => (\right : List A => right)
	@cons head tail => (\right : List A => *tail right);
drop_append_nat := drop_append Nat;
// Explicit remaining capture refusal: the original whole left is free in clauses.
keep_left := \A : @ => \left : List A => left @nil => (\right : List A => left)
	@cons head tail => (\right : List A => left);
keep_left_nat := keep_left Nat;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
left := (List Nat).cons three ((List Nat).cons zero empty);
right := (List Nat).cons two ((List Nat).cons one empty);
mixed := (List Nat).cons three ((List Nat).cons zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
reference := print_list (sort mixed);
append_reference := print_list (append_nat left right);
drop_reference := print_list (drop_append_nat left right);
