import Nat;

// Zero's bound has no source field to recover it from. Successor's does.
Fin := @\n:Nat => {
	zero:(k:Nat)->* (Nat.succ k);
	succ:* n->* (Nat.succ n);
};
fin_to_nat := \n:Nat => \i:Fin n => i
	@zero k => Nat.zero
	@succ rest => Nat.succ *rest;
fin_to_nat :: (n:Nat)->Fin n->Nat;
fin_weaken := \n:Nat => \i:Fin n => i
	@(size self => Fin (Nat.succ size))
	@zero k => Fin.zero (Nat.succ k)
	@succ rest => Fin.succ *rest;
fin_weaken :: (n:Nat)->Fin n->Fin (Nat.succ n);

// Exchange the first two positions, leaving shorter domains unchanged.
fin_flip := \n:Nat => \i:Fin n => i @(size self => Fin size)
	@zero k => (k @(size => Fin (Nat.succ size))
		@zero => Fin.zero Nat.zero
		@succ m => Fin.succ (Fin.zero m))
	@succ rest => (rest @(size self => Fin (Nat.succ size))
		@zero k => Fin.zero (Nat.succ k)
		@succ further => Fin.succ (Fin.succ further));
fin_flip :: (n:Nat)->Fin n->Fin n;

// An ordinary indexed relation on positions, not a new kernel Equality rule.
same_position := \n:Nat => @\left:Fin n => @\right:Fin n => {
	refl:(i:Fin n)->* i i;
};
position_sym := \n:Nat => \left:Fin n => \right:Fin n =>
	\p:same_position n left right => p
	@(l r self => same_position n r l)
	@refl i => (same_position n).refl i;
position_sym :: (n:Nat)->(left:Fin n)->(right:Fin n)->
	same_position n left right->same_position n right left;
position_trans := \n:Nat => \left:Fin n => \middle:Fin n =>
	\p:same_position n left middle => p
	@(l m self => (right:Fin n)->same_position n m right->same_position n l right)
	@refl i => (\right:Fin n => \q:same_position n i right => q);
position_trans :: (n:Nat)->(left:Fin n)->(middle:Fin n)->
	same_position n left middle->(right:Fin n)->same_position n middle right->same_position n left right;
position_cong := \n:Nat => \m:Nat => \f:Fin n->Fin m =>
	\left:Fin n => \right:Fin n => \p:same_position n left right => p
	@(l r self => same_position m (f l) (f r))
	@refl i => (same_position m).refl (f i);
position_cong :: (n:Nat)->(m:Nat)->(f:Fin n->Fin m)->(left:Fin n)->(right:Fin n)->
	same_position n left right->same_position m (f left) (f right);

fin_flip_twice := \n:Nat => \i:Fin n => i
	@(size self => same_position size (fin_flip size (fin_flip size self)) self)
	@zero k => (k @(size => same_position (Nat.succ size)
		(fin_flip (Nat.succ size) (fin_flip (Nat.succ size) (Fin.zero size))) (Fin.zero size))
		@zero => (same_position (Nat.succ Nat.zero)).refl (Fin.zero Nat.zero)
		@succ m => (same_position (Nat.succ (Nat.succ m))).refl (Fin.zero (Nat.succ m)))
	@succ rest => (rest @(size self => same_position (Nat.succ size)
		(fin_flip (Nat.succ size) (fin_flip (Nat.succ size) (Fin.succ self))) (Fin.succ self))
		@zero k => (same_position (Nat.succ (Nat.succ k))).refl (Fin.succ (Fin.zero k))
		@succ further => (same_position n).refl (Fin.succ (Fin.succ further)));
fin_flip_twice :: (n:Nat)->(i:Fin n)->same_position n (fin_flip n (fin_flip n i)) i;

// Forward maps each new position to its old position. Both laws are required.
position_permutation := \n:Nat => @{
	mk:(forward:Fin n->Fin n)->(backward:Fin n->Fin n)->
		((i:Fin n)->same_position n (backward (forward i)) i)->
		((i:Fin n)->same_position n (forward (backward i)) i)->*;
};
position_forward := \n:Nat => \p:position_permutation n => p
	@mk forward backward left right => forward;
position_backward := \n:Nat => \p:position_permutation n => p
	@mk forward backward left right => backward;
position_left_inverse := \n:Nat => \p:position_permutation n => p
	@(self => (i:Fin n)->same_position n
		(position_backward n self (position_forward n self i)) i)
	@mk forward backward left right => (\i:Fin n => left i);
position_right_inverse := \n:Nat => \p:position_permutation n => p
	@(self => (i:Fin n)->same_position n
		(position_forward n self (position_backward n self i)) i)
	@mk forward backward left right => (\i:Fin n => right i);
position_identity := \n:Nat => (position_permutation n).mk
	&(\i:Fin n => i) &(\i:Fin n => i)
	&(\i:Fin n => (same_position n).refl i)
	&(\i:Fin n => (same_position n).refl i);
position_identity :: (n:Nat)->position_permutation n;
position_flip := \n:Nat => (position_permutation n).mk
	&(fin_flip n) &(fin_flip n) &(fin_flip_twice n) &(fin_flip_twice n);
position_flip :: (n:Nat)->position_permutation n;
position_inverse := \n:Nat => \p:position_permutation n => p
	@mk forward backward left right => (position_permutation n).mk backward forward right left;
position_inverse :: (n:Nat)->position_permutation n->position_permutation n;
position_compose := \n:Nat => \p:position_permutation n => \q:position_permutation n => p
	@mk f g left_p right_p => (q
		@mk h k left_q right_q => (position_permutation n).mk
			&(\i:Fin n => f (h i)) &(\i:Fin n => k (g i))
			&(\i:Fin n => position_trans n (k (g (f (h i)))) (k (h i))
				(position_cong n n k (g (f (h i))) (h i) (left_p (h i))) i (left_q i))
			&(\i:Fin n => position_trans n (f (h (k (g i)))) (f (g i))
				(position_cong n n f (h (k (g i))) (g i) (right_q (g i))) i (right_p i)));
position_compose :: (n:Nat)->position_permutation n->position_permutation n->position_permutation n;

// Lift a permutation through a new fixed first position.
nat_predecessor := \n:Nat => n @zero => Nat.zero @succ k => k;
position_keep_map := \n:Nat => \i:Fin n => i
	@(size self => (Fin (nat_predecessor size)->Fin (nat_predecessor size))->Fin size)
	@zero k => (\f:Fin k->Fin k => Fin.zero k)
	@succ rest => (\f:Fin (nat_predecessor n)->Fin (nat_predecessor n) => Fin.succ (f rest));
position_keep_inverse_law := \n:Nat => \i:Fin n => i
	@(size self => (f:Fin (nat_predecessor size)->Fin (nat_predecessor size))->
		(g:Fin (nat_predecessor size)->Fin (nat_predecessor size))->
		((j:Fin (nat_predecessor size))->same_position (nat_predecessor size) (g (f j)) j)->
		same_position size (position_keep_map size (position_keep_map size self f) g) self)
	@zero k => (\f:Fin k->Fin k => \g:Fin k->Fin k =>
		\law:(j:Fin k)->same_position k (g (f j)) j =>
		(same_position (Nat.succ k)).refl (Fin.zero k))
	@succ rest => (\f:Fin (nat_predecessor n)->Fin (nat_predecessor n) =>
		\g:Fin (nat_predecessor n)->Fin (nat_predecessor n) =>
		\law:(j:Fin (nat_predecessor n))->same_position (nat_predecessor n) (g (f j)) j =>
		position_cong (nat_predecessor n) n &(\j:Fin (nat_predecessor n)=>Fin.succ j)
			(g (f rest)) rest (law rest));
position_keep := \n:Nat => \p:position_permutation n => (position_permutation (Nat.succ n)).mk
	&(\i:Fin (Nat.succ n) => position_keep_map (Nat.succ n) i (position_forward n p))
	&(\i:Fin (Nat.succ n) => position_keep_map (Nat.succ n) i (position_backward n p))
	&(\i:Fin (Nat.succ n) => position_keep_inverse_law (Nat.succ n) i
		(position_forward n p) (position_backward n p) &(position_left_inverse n p))
	&(\i:Fin (Nat.succ n) => position_keep_inverse_law (Nat.succ n) i
		(position_backward n p) (position_forward n p) &(position_right_inverse n p));
position_keep :: (n:Nat)->position_permutation n->position_permutation (Nat.succ n);

tuple_permute := \A:@ => \n:Nat => \values:Fin n->A => \p:position_permutation n =>
	\i:Fin n => values (position_forward n p i);
tuple_permute :: (A:@)->(n:Nat)->(Fin n->A)->position_permutation n->Fin n->A;
