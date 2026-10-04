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
import append;
import quickSort;
import measure;
import natAccessible;

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
append_fn := append;
sort := \xs : List Nat => quickSort Nat &natLessOrEqual xs;
count := \n : Nat => n @zero => #0 @succ k => #int_add #1 *k;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));

// Deliberately changed-source control, not a replacement QuickSort candidate.
// Removing the pivot must alter generated code and execution, not print a sorter.
drop_acc := \A : @ => \le : A -> A -> Bool =>
	\size : Nat => \access : Acc Nat LT size =>
		access @acc current down =>
			(\input : SizedList A current => input @nil => (List A).nil
				@cons tailSize pivot tail => {
					partitioned := partition A le pivot tailSize tail;
					partitioned @parts lowerSize lower upperSize upper lowerBound upperBound => {
						lowerResult := *down lowerSize lowerBound lower;
						upperResult := *down upperSize upperBound upper;
						append A lowerResult upperResult;
					};
				});
drop_sort_acc := drop_acc Nat &natLessOrEqual;
drop_sort := \xs : List Nat => measure Nat xs @measured size values =>
	drop_acc Nat &natLessOrEqual size (natAccessible size) values;
three := Nat.succ (Nat.succ (Nat.succ Nat.zero));
two := Nat.succ (Nat.succ Nat.zero);
one := Nat.succ Nat.zero;
mixed := (List Nat).cons three ((List Nat).cons Nat.zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
reference := print_list (sort mixed);
drop_reference := print_list (drop_sort mixed);
