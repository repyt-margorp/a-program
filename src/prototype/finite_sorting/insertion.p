// A local chain only constrains its next element; no transitivity is needed.
chain_head := \A:@ => \R:A->A->@ => \x:A => @\xs:List A => {
	nil:* (List A).nil;
	cons:(y:A)->(ys:List A)->R x y->* ((List A).cons y ys);
};
chain_bound := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	\p:general_locally_sorted A R ((List A).cons x xs) => p
	@one h => (chain_head A R h).nil
	@cons h y ys edge rest => (chain_head A R h).cons y ys edge;
chain_tail := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	\p:general_locally_sorted A R ((List A).cons x xs) => p
	@one h => (general_locally_sorted A R).nil
	@cons h y ys edge rest => rest;
chain_prepend := \A:@ => \R:A->A->@ => \x:A => \xs:List A =>
	\bound:chain_head A R x xs => bound
	@(values self => general_locally_sorted A R values->general_locally_sorted A R ((List A).cons x values))
	@nil => (\prior:general_locally_sorted A R (List A).nil => (general_locally_sorted A R).one x)
	@cons y ys edge => (\prior:general_locally_sorted A R ((List A).cons y ys) =>
		(general_locally_sorted A R).cons x y ys edge prior);

insert_choice := \A:@ => \le:A->A->Bool => \v:A => \h:A => \t:List A => \d:Bool => d
	@true => (List A).cons v ((List A).cons h t)
	@false => (List A).cons h (insertBy A &le v t);
insert_bound_choice := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\v:A => \h:A => \t:List A => \lo:A => \lv:R lo v =>
	\prior:chain_head A R lo ((List A).cons h t) => prior
	@cons y ys edge => (\d:Bool => d
		@(answer => chain_head A R lo (insert_choice A &le v y ys answer))
		@true => (chain_head A R lo).cons v ((List A).cons y ys) lv
		@false => (chain_head A R lo).cons y (insertBy A &le v ys) edge);
insert_bound := \A:@ => \R:A->A->@ => \le:A->A->Bool => \v:A => \xs:List A => xs
	@(self => (lo:A)->R lo v->chain_head A R lo self->chain_head A R lo (insertBy A &le v self))
	@nil => (\lo:A => \lv:R lo v => \prior:chain_head A R lo (List A).nil =>
		(chain_head A R lo).cons v (List A).nil lv)
	@cons h t => (\lo:A => \lv:R lo v => \prior:chain_head A R lo ((List A).cons h t) =>
		insert_bound_choice A R &le v h t lo lv prior (le v h));
insert_bound :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->(v:A)->(xs:List A)->
	(lo:A)->R lo v->chain_head A R lo xs->chain_head A R lo (insertBy A &le v xs);

insert_local_choice := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\v:A => \h:A => \t:List A => \prior:general_locally_sorted A R ((List A).cons h t) =>
	\tail:general_locally_sorted A R (insertBy A &le v t) => \d:Bool =>
	\decision:general_decision A R v h d => decision
	@(answer self => general_locally_sorted A R (insert_choice A &le v h t answer))
	@yes edge => (general_locally_sorted A R).cons v h t edge prior
	@no edge => chain_prepend A R h (insertBy A &le v t)
		(insert_bound A R &le v t h edge (chain_bound A R h t prior)) tail;
insert_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \v:A => \xs:List A => xs
	@(self => general_locally_sorted A R self->general_locally_sorted A R (insertBy A &le v self))
	@nil => (\prior:general_locally_sorted A R (List A).nil => (general_locally_sorted A R).one v)
	@cons h t => (\prior:general_locally_sorted A R ((List A).cons h t) =>
		insert_local_choice A R &le v h t prior (*t (chain_tail A R h t prior)) (le v h) (decide v h));
insert_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(v:A)->(xs:List A)->
	general_locally_sorted A R xs->general_locally_sorted A R (insertBy A &le v xs);

insert_content_choice := \A:@ => \le:A->A->Bool => \v:A => \h:A => \t:List A =>
	\rest:permutation A ((List A).cons v t) (insertBy A &le v t) => \d:Bool => d
	@(answer => permutation A ((List A).cons v ((List A).cons h t)) (insert_choice A &le v h t answer))
	@true => permutation_refl A ((List A).cons v ((List A).cons h t))
	@false => (permutation A).compose ((List A).cons v ((List A).cons h t))
		((List A).cons h ((List A).cons v t)) ((List A).cons h (insertBy A &le v t))
		((permutation A).swap v h t) ((permutation A).keep h ((List A).cons v t) (insertBy A &le v t) rest);
insert_content := \A:@ => \le:A->A->Bool => \v:A => \xs:List A => xs
	@(self => permutation A ((List A).cons v self) (insertBy A &le v self))
	@nil => permutation_refl A ((List A).cons v (List A).nil)
	@cons h t => insert_content_choice A &le v h t *t (le v h);
insert_content :: (A:@)->(le:A->A->Bool)->(v:A)->(xs:List A)->
	permutation A ((List A).cons v xs) (insertBy A &le v xs);

insertion_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => xs
	@(self => general_locally_sorted A R (insertionSortBy A &le self))
	@nil => (general_locally_sorted A R).nil
	@cons h t => insert_local A R &le decide h (insertionSortBy A &le t) *t;
insertion_content := \A:@ => \le:A->A->Bool => \xs:List A => xs
	@(self => permutation A self (insertionSortBy A &le self))
	@nil => (permutation A).nil
	@cons h t => (permutation A).compose ((List A).cons h t)
		((List A).cons h (insertionSortBy A &le t)) (insertBy A &le h (insertionSortBy A &le t))
		((permutation A).keep h t (insertionSortBy A &le t) *t)
		(insert_content A &le h (insertionSortBy A &le t));
insertion_backend := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	(sorting_backend A R).mk &(insertionSortBy A &le)
		&(insertion_local A R &le decide) &(insertion_content A &le);
insertion_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
