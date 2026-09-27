// One ordinary function, with proofs about that function's actual output.
sorting_backend := \A:@ => \R:A->A->@ => @{
	mk:(sort:List A->List A)->
		((xs:List A)->general_locally_sorted A R (sort xs))->
		((xs:List A)->permutation A xs (sort xs))->*;
};
sorting_run := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\xs:List A => backend @mk sort local content => sort xs;
sorting_local := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	backend @(self => (xs:List A)->general_locally_sorted A R (sorting_run A R self xs))
	@mk sort local content => (\xs:List A => local xs);
sorting_content := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	backend @(self => (xs:List A)->permutation A xs (sorting_run A R self xs))
	@mk sort local content => (\xs:List A => content xs);
sorting_local :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->(xs:List A)->
	general_locally_sorted A R (sorting_run A R backend xs);
sorting_content :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->(xs:List A)->
	permutation A xs (sorting_run A R backend xs);

sorting_strong := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\backend:sorting_backend A R => \xs:List A =>
	locally_sorted_to_strongly_sorted A R trans (sorting_run A R backend xs)
		(sorting_local A R backend xs);
sorting_strong :: (A:@)->(R:A->A->@)->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(backend:sorting_backend A R)->(xs:List A)->
	general_strongly_sorted A R (sorting_run A R backend xs);

sorting_reordering := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs =>
	permutation_reordering A xs (sorting_run A R backend xs)
		(sorting_content A R backend xs) n size;
sorting_reordering :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(xs:List A)->(size:list_size A n xs)->
	reordering A n xs size (sorting_run A R backend xs);
sorting_size := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_size A n xs size (sorting_run A R backend xs)
		(sorting_reordering A R backend n xs size);
sorting_positions := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_positions A n xs size (sorting_run A R backend xs)
		(sorting_reordering A R backend n xs size);
sorting_value := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_observe A n xs size (sorting_run A R backend xs)
		(sorting_reordering A R backend n xs size);
sorting_value :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(xs:List A)->(size:list_size A n xs)->(i:Fin n)->(P:A->@)->
	P (sized_lookup A n (sorting_run A R backend xs) (sorting_size A R backend n xs size) i)->
	P (sized_lookup A n xs size (position_forward n (sorting_positions A R backend n xs size) i));

// The supported traversal is the List spine / Vec order, not an arbitrary container.
sorting_vector := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n =>
	vector_refill A n (sorting_run A R backend (vec_contents A n v))
		(sorting_size A R backend n (vec_contents A n v) (vector_contents_size A n v));
sorting_vector :: (A:@)->(R:A->A->@)->sorting_backend A R->(n:Nat)->Vec A n->Vec A n;
sorting_vector_observe := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n =>
	refill_contents A n (sorting_run A R backend (vec_contents A n v))
		(sorting_size A R backend n (vec_contents A n v) (vector_contents_size A n v));
sorting_vector_observe :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(v:Vec A n)->(P:List A->@)->P (sorting_run A R backend (vec_contents A n v))->
	P (vec_contents A n (sorting_vector A R backend n v));
sorting_vector_local := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n => sorting_vector_observe A R backend n v
		&(\ys:List A => general_locally_sorted A R ys)
		(sorting_local A R backend (vec_contents A n v));
sorting_vector_content := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n => sorting_vector_observe A R backend n v
		&(\ys:List A => permutation A (vec_contents A n v) ys)
		(sorting_content A R backend (vec_contents A n v));
sorting_vector_local :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(v:Vec A n)->general_locally_sorted A R (vec_contents A n (sorting_vector A R backend n v));
sorting_vector_content :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(v:Vec A n)->permutation A (vec_contents A n v) (vec_contents A n (sorting_vector A R backend n v));
sorting_vector_value := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n => \i:Fin n => \P:A->@ =>
	\proof:P (vec_lookup A n (sorting_vector A R backend n v) i) =>
	vector_roundtrip_back A n v
		(position_forward n (sorting_positions A R backend n (vec_contents A n v) (vector_contents_size A n v)) i)
		P (sorting_value A R backend n (vec_contents A n v) (vector_contents_size A n v) i P proof);
sorting_vector_value :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->P (vec_lookup A n (sorting_vector A R backend n v) i)->
	P (vec_lookup A n v (position_forward n
		(sorting_positions A R backend n (vec_contents A n v) (vector_contents_size A n v)) i));

// A permutation acts contravariantly on positions: the new index selects an old one.
position_action := \A:@ => \n:Nat => \p:position_permutation n => \values:Fin n->A =>
	\i:Fin n => values (position_forward n p i);
position_action_identity := \A:@ => \n:Nat => \values:Fin n->A =>
	\i:Fin n => \P:A->@ => \proof:P (values i) => proof;
position_action_identity :: (A:@)->(n:Nat)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->
	P (values i)->P (position_action A n (position_identity n) values i);
position_action_compose := \A:@ => \n:Nat => \p:position_permutation n =>
	\q:position_permutation n => \values:Fin n->A => \i:Fin n => \P:A->@ =>
	\proof:P (position_action A n q &(position_action A n p values) i) =>
	position_observe n (position_forward n p (position_forward n q i))
		(position_forward n (position_compose n p q) i) (position_compose_forward n p q i)
		&(\j:Fin n => P (values j)) proof;
position_action_compose :: (A:@)->(n:Nat)->(p:position_permutation n)->
	(q:position_permutation n)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->
	P (position_action A n q &(position_action A n p values) i)->
	P (position_action A n (position_compose n p q) values i);
