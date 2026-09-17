import Nat;
import List;
import Sorted;
import quickSort;
import quick_correct;
import natLessOrEqual;
// Sorted output does not imply that the arbitrary input was sorted.
wrong := \xs:List Nat => \ys:List Nat => \g:@quickSort Nat (&natLessOrEqual) xs ys =>
	(quick_correct xs ys g :: Sorted xs);
