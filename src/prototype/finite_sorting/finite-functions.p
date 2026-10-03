// Finite functions reuse the ordinary List/Vec result and its labelled bijection.
sorting_value_back := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_observe_back A n xs size (sorting_run A R backend xs)
		(sorting_reordering A R backend n xs size);
sorting_value_back :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(xs:List A)->(size:list_size A n xs)->(i:Fin n)->(P:A->@)->
	P (sized_lookup A n xs size (position_forward n (sorting_positions A R backend n xs size) i))->
	P (sized_lookup A n (sorting_run A R backend xs) (sorting_size A R backend n xs size) i);

sorting_vector_value_back := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \v:Vec A n => \i:Fin n => \P:A->@ =>
	\proof:P (vec_lookup A n v (position_forward n
		(sorting_positions A R backend n (vec_contents A n v) (vector_contents_size A n v)) i)) =>
	sorting_value_back A R backend n (vec_contents A n v) (vector_contents_size A n v) i P
		(vector_roundtrip A n v (position_forward n
			(sorting_positions A R backend n (vec_contents A n v) (vector_contents_size A n v)) i) P proof);
sorting_vector_value_back :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->
	P (vec_lookup A n v (position_forward n
		(sorting_positions A R backend n (vec_contents A n v) (vector_contents_size A n v)) i))->
	P (vec_lookup A n (sorting_vector A R backend n v) i);

sorting_function := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n =>
	vec_lookup A n (sorting_vector A R backend n (vec_tabulate A n values)) i;
sorting_function :: (A:@)->(R:A->A->@)->sorting_backend A R->
	(n:Nat)->(Fin n->A)->Fin n->A;
sorting_function_positions := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => sorting_positions A R backend n
		(vec_contents A n (vec_tabulate A n values))
		(vector_contents_size A n (vec_tabulate A n values));
sorting_function_positions :: (A:@)->(R:A->A->@)->sorting_backend A R->
	(n:Nat)->(Fin n->A)->position_permutation n;

// Predicates are explicit source arguments, not object Identity or reflection.
sorting_function_value := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n => \P:A->@ =>
	\proof:P (sorting_function A R backend n values i) =>
	vec_at_observe A n (vec_tabulate A n values)
		(position_forward n (sorting_function_positions A R backend n values) i)
		(values (position_forward n (sorting_function_positions A R backend n values) i))
		(vec_tabulate_at A n (position_forward n (sorting_function_positions A R backend n values) i) values)
		P (sorting_vector_value A R backend n (vec_tabulate A n values) i P proof);
sorting_function_value :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->
	P (sorting_function A R backend n values i)->
	P (values (position_forward n (sorting_function_positions A R backend n values) i));
sorting_function_value_back := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n => \P:A->@ =>
	\proof:P (values (position_forward n (sorting_function_positions A R backend n values) i)) =>
	sorting_vector_value_back A R backend n (vec_tabulate A n values) i P
		(vec_at_observe_back A n (vec_tabulate A n values)
			(position_forward n (sorting_function_positions A R backend n values) i)
			(values (position_forward n (sorting_function_positions A R backend n values) i))
			(vec_tabulate_at A n (position_forward n (sorting_function_positions A R backend n values) i) values)
			P proof);
sorting_function_value_back :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->
	P (values (position_forward n (sorting_function_positions A R backend n values) i))->
	P (sorting_function A R backend n values i);

sorting_function_restore := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n => sorting_function A R backend n values
		(position_backward n (sorting_function_positions A R backend n values) i);
sorting_function_restore :: (A:@)->(R:A->A->@)->sorting_backend A R->
	(n:Nat)->(Fin n->A)->Fin n->A;
sorting_function_recover := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n => \P:A->@ => \proof:P (values i) =>
	sorting_function_value_back A R backend n values
		(position_backward n (sorting_function_positions A R backend n values) i) P
		(position_observe n i (position_forward n (sorting_function_positions A R backend n values)
			(position_backward n (sorting_function_positions A R backend n values) i))
			(position_sym n (position_forward n (sorting_function_positions A R backend n values)
				(position_backward n (sorting_function_positions A R backend n values) i)) i
				(position_right_inverse n (sorting_function_positions A R backend n values) i))
			&(\j:Fin n => P (values j)) proof);
sorting_function_recover :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->P (values i)->
	P (sorting_function_restore A R backend n values i);
sorting_function_recover_back := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => \i:Fin n => \P:A->@ =>
	\proof:P (sorting_function_restore A R backend n values i) =>
	position_observe n (position_forward n (sorting_function_positions A R backend n values)
		(position_backward n (sorting_function_positions A R backend n values) i)) i
		(position_right_inverse n (sorting_function_positions A R backend n values) i)
		&(\j:Fin n => P (values j)) (sorting_function_value A R backend n values
			(position_backward n (sorting_function_positions A R backend n values) i) P proof);
sorting_function_recover_back :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->(i:Fin n)->(P:A->@)->
	P (sorting_function_restore A R backend n values i)->P (values i);

// Certify the actual finite traversal; tabulation preserves each observed entry.
sorting_function_contents := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A =>
	vec_contents A n (sorting_vector A R backend n (vec_tabulate A n values));
sorting_function_content := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => sorting_vector_content A R backend n (vec_tabulate A n values);
sorting_function_content :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->permutation A (vec_contents A n (vec_tabulate A n values))
		(sorting_function_contents A R backend n values);
sorting_function_local := \A:@ => \R:A->A->@ => \backend:sorting_backend A R =>
	\n:Nat => \values:Fin n->A => sorting_vector_local A R backend n (vec_tabulate A n values);
sorting_function_local :: (A:@)->(R:A->A->@)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->general_locally_sorted A R (sorting_function_contents A R backend n values);
sorting_function_strong := \A:@ => \R:A->A->@ =>
	\trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z =>
	\backend:sorting_backend A R => \n:Nat => \values:Fin n->A =>
	locally_sorted_to_strongly_sorted A R trans (sorting_function_contents A R backend n values)
		(sorting_function_local A R backend n values);
sorting_function_strong :: (A:@)->(R:A->A->@)->
	((x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->(backend:sorting_backend A R)->
	(n:Nat)->(values:Fin n->A)->general_strongly_sorted A R (sorting_function_contents A R backend n values);
