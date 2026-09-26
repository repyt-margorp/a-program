import List;
import Nat;
import Sorted;
import natLessOrEqual;
import mergeSort;
import merge_correct;
// A sorted output does not establish that the original input was sorted.
wrong := \xs:List Nat => \ys:List Nat => \g:@mergeSort (&natLessOrEqual) xs ys =>
	(merge_correct xs ys g :: Sorted xs);
