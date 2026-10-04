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
import measure;

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
sort := \xs : List Nat => quickSort Nat &natLessOrEqual xs;
count := \n : Nat => n @zero => #0 @succ k => #int_add #1 *k;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));

// Noncandidate changed-source control: swap the comparison's actual arguments.
// Only direct partition observations qualify this mutation; it is not QuickSort.
reverse_partition := \A : @ => \le : A -> A -> Bool =>
	\pivot : A => \size : Nat => \xs : SizedList A size =>
		xs @nil => (Partition A Nat.zero).parts Nat.zero (SizedList A).nil
			Nat.zero (SizedList A).nil (LT.step Nat.zero) (LT.step Nat.zero)
		@cons tailSize head tail => {
			decision := le pivot head;
			partitionByDecision A head tailSize decision *tail;
		};
reverse_partition_nat := reverse_partition Nat &natLessOrEqual;
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
mixed := (List Nat).cons three ((List Nat).cons zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
print_sized := \size : Nat => \xs : SizedList Nat size => xs @nil => #print #"|"
	@cons tailSize head tail => { #print (#int_to_text (count head)); #print #","; *tail; };
print_parts := \bound : Nat => \p : Partition Nat bound =>
	p @parts lowerSize lower upperSize upper lowerBound upperBound => {
		#print (#int_to_text (count lowerSize)); #print #":"; print_sized lowerSize lower;
		#print (#int_to_text (count upperSize)); #print #":"; print_sized upperSize upper;
	};
reference := print_list (sort mixed);
partition_reference := measure Nat mixed @measured size values =>
	print_parts size (partition_nat one size values);
reverse_reference := measure Nat mixed @measured size values =>
	print_parts size (reverse_partition_nat one size values);
