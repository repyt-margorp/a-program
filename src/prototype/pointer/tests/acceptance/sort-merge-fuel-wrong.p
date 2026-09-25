import List;
import Nat;
import natLessOrEqual;
import mergeSortFuel;
import Fits;
import fuel_correct;
// Zero fuel returns the unsorted input. The missing bound cannot be forged.
unsorted := (List Nat).cons (Nat.succ Nat.zero) ((List Nat).cons Nat.zero (List Nat).nil);
valid_graph := (@mergeSortFuel (&natLessOrEqual)).case0 unsorted;
wrong := fuel_correct Nat.zero unsorted unsorted valid_graph (Fits.nil Nat.zero);
