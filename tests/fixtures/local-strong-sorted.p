import List;
import general_all_from;
import general_sorted;
import general_decision;
import Bool;

// This alias preserves the existing nominal family and its strong contract.
general_strongly_sorted := general_sorted;

general_locally_sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	one:(x:A)->* ((List A).cons x (List A).nil);
	cons:(x:A)->(y:A)->(ys:List A)->R x y->
		* ((List A).cons y ys)->* ((List A).cons x ((List A).cons y ys));
};

sorted_bound_head := \A:@ => \R:A->A->@ => \x:A => \y:A => \ys:List A =>
	\p:general_all_from A R x ((List A).cons y ys) => p @cons h t ph pt => ph;
sorted_strong_bound := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	\p:general_strongly_sorted A R ((List A).cons x xs) => p @cons h t ph pt => ph;

sorted_local_prepend := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	xs @(self => general_all_from A R x self->general_locally_sorted A R self->
		general_locally_sorted A R ((List A).cons x self))
	@nil => (\bound:general_all_from A R x (List A).nil =>
		\tail:general_locally_sorted A R (List A).nil => (general_locally_sorted A R).one x)
	@cons y ys => (\bound:general_all_from A R x ((List A).cons y ys) =>
		\tail:general_locally_sorted A R ((List A).cons y ys) =>
		(general_locally_sorted A R).cons x y ys (sorted_bound_head A R x y ys bound) tail);
sorted_local_prepend :: (A:@)->(R:A->A->@)->(x:A)->(xs:List A)->
	general_all_from A R x xs->general_locally_sorted A R xs->
	general_locally_sorted A R ((List A).cons x xs);

strongly_sorted_to_locally_sorted := \A:@ => \R:A->A->@ => \xs:List A =>
	\p:general_strongly_sorted A R xs => p
	@nil => (general_locally_sorted A R).nil
	@cons h t ph pt => sorted_local_prepend A R h t ph *pt;
strongly_sorted_to_locally_sorted :: (A:@)->(R:A->A->@)->(xs:List A)->
	general_strongly_sorted A R xs->general_locally_sorted A R xs;

sorted_bound_trans := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\y:A => \ys:List A => \p:general_all_from A R y ys => p
	@nil => (\x:A => \xy:R x y => (general_all_from A R x).nil)
	@cons h t ph pt => (\x:A => \xy:R x y =>
		(general_all_from A R x).cons h t (trans x y xy h ph) (*pt x xy));
sorted_bound_trans :: (A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(y:A)->(ys:List A)->general_all_from A R y ys->(x:A)->R x y->general_all_from A R x ys;

locally_sorted_to_strongly_sorted := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\xs:List A => \p:general_locally_sorted A R xs => p
	@nil => (general_strongly_sorted A R).nil
	@one x => (general_strongly_sorted A R).cons x (List A).nil
		(general_all_from A R x).nil (general_strongly_sorted A R).nil
	@cons x y ys xy tail => (general_strongly_sorted A R).cons x ((List A).cons y ys)
		((general_all_from A R x).cons y ys xy
			(sorted_bound_trans A R trans y ys (sorted_strong_bound A R y ys *tail) x xy)) *tail;
locally_sorted_to_strongly_sorted :: (A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(xs:List A)->
	general_locally_sorted A R xs->general_strongly_sorted A R xs;

decision_diagonal := \A:@ => \R:A->A->@ => \x:A => \answer:Bool =>
	\proof:general_decision A R x x answer => proof
	@yes evidence => evidence
	@no evidence => evidence;
decision_diagonal :: (A:@)->(R:A->A->@)->(x:A)->(answer:Bool)->
	general_decision A R x x answer->R x x;
