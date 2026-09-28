// One predicate covers both lower and upper tree bounds.
tree_all := \A:@ => \P:A->@ => @\tree:Tree A => {
	empty:* (Tree A).empty;
	node:(root:A)->(left:Tree A)->(right:Tree A)->P root->* left->* right->
		* ((Tree A).node root left right);
};
tree_insert_choice := \A:@ => \le:A->A->Bool => \v:A => \root:A =>
	\left:Tree A => \right:Tree A => \answer:Bool => answer
	@true => (Tree A).node root (treeInsert A &le v left) right
	@false => (Tree A).node root left (treeInsert A &le v right);
tree_insert_all := \A:@ => \P:A->@ => \le:A->A->Bool => \v:A => \pv:P v =>
	\tree:Tree A => \prior:tree_all A P tree => prior
	@(self proof => tree_all A P (treeInsert A &le v self))
	@empty => (tree_all A P).node v (Tree A).empty (Tree A).empty pv
		(tree_all A P).empty (tree_all A P).empty
	@node root left right pr pl pu => ((le v root)
		@(answer => tree_all A P (tree_insert_choice A &le v root left right answer))
		@true => (tree_all A P).node root (treeInsert A &le v left) right pr *pl pu
		@false => (tree_all A P).node root left (treeInsert A &le v right) pr pl *pu);
tree_insert_all :: (A:@)->(P:A->@)->(le:A->A->Bool)->(v:A)->P v->
	(tree:Tree A)->tree_all A P tree->tree_all A P (treeInsert A &le v tree);

tree_ordered := \A:@ => \R:A->A->@ => @\tree:Tree A => {
	empty:* (Tree A).empty;
	node:(root:A)->(left:Tree A)->(right:Tree A)->
		tree_all A &(\x:A=>R x root) left->tree_all A &(\x:A=>R root x) right->
		* left->* right->* ((Tree A).node root left right);
};
tree_insert_ordered_choice := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\v:A => \root:A => \left:Tree A => \right:Tree A =>
	\lower:tree_all A &(\x:A=>R x root) left => \upper:tree_all A &(\x:A=>R root x) right =>
	\pl:tree_ordered A R left => \pr:tree_ordered A R right =>
	\il:tree_ordered A R (treeInsert A &le v left) =>
	\ir:tree_ordered A R (treeInsert A &le v right) =>
	\answer:Bool => \decision:general_decision A R v root answer => decision
	@(result proof => tree_ordered A R (tree_insert_choice A &le v root left right result))
	@yes edge => (tree_ordered A R).node root (treeInsert A &le v left) right
		(tree_insert_all A &(\x:A=>R x root) &le v edge left lower) upper il pr
	@no edge => (tree_ordered A R).node root left (treeInsert A &le v right)
		lower (tree_insert_all A &(\x:A=>R root x) &le v edge right upper) pl ir;
tree_insert_ordered := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	\v:A => \tree:Tree A => \prior:tree_ordered A R tree => prior
	@(self proof => tree_ordered A R (treeInsert A &le v self))
	@empty => (tree_ordered A R).node v (Tree A).empty (Tree A).empty
		(tree_all A &(\x:A=>R x v)).empty (tree_all A &(\x:A=>R v x)).empty
		(tree_ordered A R).empty (tree_ordered A R).empty
	@node root left right lower upper pl pr =>
		tree_insert_ordered_choice A R &le v root left right lower upper pl pr *pl *pr
			(le v root) (decide v root);
tree_build_ordered := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A => xs
	@(self => tree_ordered A R (treeBuild A &le self))
	@nil => (tree_ordered A R).empty
	@cons h t => tree_insert_ordered A R &le decide h (treeBuild A &le t) *t;
tree_list_all := \A:@ => \P:A->@ => \tree:Tree A => \prior:tree_all A P tree => prior
	@(self proof => qsr_All A P (treeToList A self))
	@empty => (qsr_All A P).nil
	@node root left right pr pl pu => qsr_append_all A P
		(treeToList A left) ((List A).cons root (treeToList A right)) *pl
		((qsr_All A P).cons root (treeToList A right) pr *pu);
tree_list_local := \A:@ => \R:A->A->@ => \tree:Tree A => \prior:tree_ordered A R tree => prior
	@(self proof => general_locally_sorted A R (treeToList A self))
	@empty => (general_locally_sorted A R).nil
	@node root left right lower upper pl pr => qsl_append A R root
		(treeToList A left) *pl (treeToList A right)
		(tree_list_all A &(\x:A=>R x root) left lower) *pr
		(tree_list_all A &(\x:A=>R root x) right upper);
tree_local := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) => \xs:List A =>
	tree_list_local A R (treeBuild A &le xs) (tree_build_ordered A R &le decide xs);
tree_local :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->(xs:List A)->
	general_locally_sorted A R (treeSort A &le xs);

// Content preservation does not assume that the comparison describes an order.
tree_insert_content := \A:@ => \le:A->A->Bool => \v:A => \tree:Tree A => tree
	@(self => permutation A ((List A).cons v (treeToList A self)) (treeToList A (treeInsert A &le v self)))
	@empty => permutation_refl A ((List A).cons v (List A).nil)
	@node root left right => ((le v root)
		@(answer => permutation A ((List A).cons v (treeToList A ((Tree A).node root left right)))
			(treeToList A (tree_insert_choice A &le v root left right answer)))
		@true => permutation_suffix A ((List A).cons v (treeToList A left))
			(treeToList A (treeInsert A &le v left)) *left ((List A).cons root (treeToList A right))
		@false => (permutation A).compose
			((List A).cons v (append A (treeToList A left) ((List A).cons root (treeToList A right))))
			(append A (treeToList A left) ((List A).cons v ((List A).cons root (treeToList A right))))
			(append A (treeToList A left) ((List A).cons root (treeToList A (treeInsert A &le v right))))
			(permutation_move A v (treeToList A left) ((List A).cons root (treeToList A right)))
			(permutation_prefix A (treeToList A left)
				((List A).cons v ((List A).cons root (treeToList A right)))
				((List A).cons root (treeToList A (treeInsert A &le v right)))
				((permutation A).compose ((List A).cons v ((List A).cons root (treeToList A right)))
					((List A).cons root ((List A).cons v (treeToList A right)))
					((List A).cons root (treeToList A (treeInsert A &le v right)))
					((permutation A).swap v root (treeToList A right))
					((permutation A).keep root ((List A).cons v (treeToList A right))
						(treeToList A (treeInsert A &le v right)) *right))));
tree_content := \A:@ => \le:A->A->Bool => \xs:List A => xs
	@(self => permutation A self (treeSort A &le self))
	@nil => (permutation A).nil
	@cons h t => (permutation A).compose ((List A).cons h t)
		((List A).cons h (treeSort A &le t)) (treeSort A &le ((List A).cons h t))
		((permutation A).keep h t (treeSort A &le t) *t)
		(tree_insert_content A &le h (treeBuild A &le t));
tree_backend := \A:@ => \R:A->A->@ => \le:A->A->Bool =>
	\decide:(x:A)->(y:A)->general_decision A R x y (le x y) =>
	(sorting_backend A R).mk &(treeSort A &le) &(tree_local A R &le decide) &(tree_content A &le);
tree_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
	((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
