import Nat;
import Bool;
import List;
import quickSort;
import quick_correct;
always_false := \x:Nat => \y:Nat => Bool.false;
// A graph for a different comparator is not evidence for the ordered one.
wrong := \xs:List Nat => \ys:List Nat => \g:@quickSort Nat (&always_false) xs ys =>
	quick_correct xs ys g;
