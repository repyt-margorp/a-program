import List;
import Nat;
import Sorted;
import natLessOrEqual;
import treeSort;
import tree_correct;
// Sorting the output cannot establish that every input was already sorted.
wrong := \xs:List Nat => \ys:List Nat => \g:@treeSort Nat (&natLessOrEqual) xs ys =>
	(tree_correct xs ys g :: Sorted xs);
