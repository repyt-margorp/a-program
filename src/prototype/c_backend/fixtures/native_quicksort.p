import Bool;
import Nat;
import List;
import natLessOrEqual;
import quickSort;

bool_type := Bool;
nat_type := Nat;
nat_list := List Nat;
sort := \xs : List Nat => quickSort Nat &natLessOrEqual xs;
sort :: List Nat -> List Nat;
