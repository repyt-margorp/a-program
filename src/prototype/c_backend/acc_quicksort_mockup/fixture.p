import Bool;
import Nat;
import List;
import LT;
import Acc;
import SizedList;
import Measured;
import Partition;
import accessibleSucc;
import natAccessible;
import measure;
import partition;
import partitionLower;
import partitionUpper;
import quickSortAcc;
import natLessOrEqual;
import quickSort;

bool_type := Bool;
nat_type := Nat;
empty := (List Nat).nil;
sort := \xs : List Nat => quickSort Nat &natLessOrEqual xs;
count := \n : Nat => n @zero => #0 @succ prior => #int_add #1 *prior;
fingerprint := \xs : List Nat => xs @nil => #0
	@cons n tail => #int_add (#int_mul #5 *tail) (#int_add #1 (count n));
print_list := \xs : List Nat => xs @nil => #print #"|"
	@cons n tail => { #print (#int_to_text (count n)); #print #","; *tail; };
zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
mixed := (List Nat).cons three ((List Nat).cons zero ((List Nat).cons two ((List Nat).cons one ((List Nat).cons two empty))));
descending := (List Nat).cons three ((List Nat).cons two ((List Nat).cons one ((List Nat).cons zero empty)));
reference := { print_list (sort empty); print_list (sort mixed); print_list (sort descending); };
