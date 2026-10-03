// Interface audit: Nat lengths are not a restriction on payload type A.
quick_generic_interface := quick_backend;
quick_generic_interface :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
insertion_generic_interface := insertion_backend;
insertion_generic_interface :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
tree_generic_interface := tree_backend;
tree_generic_interface :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
merge_generic_interface := merge_backend;
merge_generic_interface :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
bubble_generic_interface := bubble_transitive_backend;
bubble_generic_interface :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->sorting_backend A R;
