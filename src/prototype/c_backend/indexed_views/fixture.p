import Bool;
import Nat;
import LT;
import Acc;
import List;
import SizedList;
import Measured;
import Partition;

bool_type := Bool;
nat_type := Nat;
lt_family := LT;
acc_family := Acc Nat LT;
sized_family := SizedList Nat;
measured_type := Measured Nat;
partition_type := Partition Nat Nat.zero;
list_type := List Nat;
other_measured_type := Measured Bool;
