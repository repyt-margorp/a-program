import List;
import Nat;
import Sorted;
import insertionSort;
import sort_correct;
// The result is sorted; that does not prove the original input was sorted.
wrong := \xs:List Nat => \ys:List Nat => \graph:@insertionSort xs ys =>
	(sort_correct xs ys graph :: Sorted xs);
