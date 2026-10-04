import Bool;
import Nat;
import LT;
import Acc;
import SizedList;
import List;
import Measured;
import Partition;
import accessibleSucc;
import natAccessible;
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
succ_access := accessibleSucc;
nat_access := natAccessible;
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
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
mixed := (List Nat).cons three ((List Nat).cons zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
reference := print_list (sort mixed);
subject := \n : Nat => (nat_access n @acc current down => count current);
subjects_reference := {
	#print (#int_to_text (subject zero)); #print #",";
	#print (#int_to_text (subject one)); #print #",";
	#print (#int_to_text (subject three)); #print #"|";
};

// Admitted unreachable callback mutation; never treated as the supplied
// manual zero-down implementation. The target must explicitly refuse it.
changed_zero_access := \n : Nat => n @zero =>
	(Acc Nat LT).acc Nat.zero
		&(\y : Nat => \edge : LT y Nat.zero => edge @step k => Nat.succ Nat.zero
			@weakenRight m k prior => Nat.succ Nat.zero
			@lift m k prior => Nat.succ Nat.zero)
	@succ k => accessibleSucc k *k;
