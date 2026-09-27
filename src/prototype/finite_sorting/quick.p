quick_backend := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	(sorting_backend A R).mk &(quickSort A &le)
		&(quick_locally_sorted A R &le decide) &(quick_content_result A &le);
quick_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
