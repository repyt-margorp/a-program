import List;
import general_locally_sorted;
import general_strongly_sorted;
import locally_sorted_to_strongly_sorted;

wrong_conversion := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\xs:List A => \proof:general_locally_sorted A R xs =>
	locally_sorted_to_strongly_sorted A R trans xs proof;
wrong_conversion :: (A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(xs:List A)->
	general_locally_sorted A R xs->general_strongly_sorted A R (List A).nil;
