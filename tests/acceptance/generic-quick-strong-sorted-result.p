import List;
import Bool;
import general_decision;
import general_strongly_sorted;
import locally_sorted_to_strongly_sorted;
import quick_locally_sorted;
import quickSort;

quick_strongly_sorted := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	locally_sorted_to_strongly_sorted A R trans (quickSort A (&le) xs)
		(quick_locally_sorted A R (&le) decide xs);
quick_strongly_sorted :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->
	general_strongly_sorted A R (quickSort A (&le) xs);
