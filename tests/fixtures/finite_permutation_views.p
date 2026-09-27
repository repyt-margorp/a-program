// A List permutation preserves every occurrence, not only its value label.
import Nat;
import List;
import Fin;
import Vec;
import permutation;
import list_size;
import predecessor;
import position_permutation;
import position_identity;
import position_keep;
import position_flip;
import position_compose;
import vec_at;
import vec_lookup;
import vec_lookup_at;
import vec_at_observe;
import vec_at_observe_back;
import vector_refill;
import same_position;
import fin_flip;
import fin_flip_twice;
import position_forward;

list_tail := \A:@ => \xs:List A => xs @nil => (List A).nil @cons h t => t;
list_replace_tail := \A:@ => \xs:List A => \ys:List A => xs
	@nil => (List A).nil @cons h t => (List A).cons h ys;
list_flip := \A:@ => \xs:List A => xs
	@nil => (List A).nil
	@cons h t => (t @nil => (List A).cons h t @cons q r => (List A).cons q ((List A).cons h r));
size_flip_head := \A:@ => \h:A => \n:Nat => \xs:List A => \size:list_size A n xs => size
	@(bound values self => list_size A (Nat.succ bound) (list_flip A ((List A).cons h values)))
	@nil => (list_size A).cons h (list_size A).nil
	@cons q rest => (list_size A).cons q ((list_size A).cons h rest);
size_flip := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => size
	@(bound values self => list_size A bound (list_flip A values))
	@nil => (list_size A).nil
	@cons h rest => size_flip_head A h (predecessor n) (list_tail A xs) rest;
size_flip :: (A:@)->(n:Nat)->(xs:List A)->list_size A n xs->list_size A n (list_flip A xs);

sized_lookup := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs =>
	vec_lookup A n (vector_refill A n xs size);
sized_lookup :: (A:@)->(n:Nat)->(xs:List A)->list_size A n xs->Fin n->A;

position_observe := \n:Nat => \i:Fin n => \j:Fin n => \p:same_position n i j => p
	@(left right self => (P:Fin n->@)->P left->P right)
	@refl value => (\P:Fin n->@ => \proof:P value => proof);
position_observe :: (n:Nat)->(i:Fin n)->(j:Fin n)->same_position n i j->
	(P:Fin n->@)->P i->P j;

vec_flip := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => Vec A size)
	@nil => (Vec A).nil
	@cons h t => (t @(size self => Vec A (Nat.succ size))
		@nil => (Vec A).cons h (Vec A).nil
		@cons q r => (Vec A).cons q ((Vec A).cons h r));
vec_flip :: (A:@)->(n:Nat)->Vec A n->Vec A n;
vec_flip_at := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n => \a:A =>
	\p:vec_at A n v i a => p
	@(size values index value self => vec_at A size (vec_flip A size values) (fin_flip size index) value)
	@here h t => (t @(size self => vec_at A (Nat.succ size)
		(vec_flip A (Nat.succ size) ((Vec A).cons h self))
		(fin_flip (Nat.succ size) (Fin.zero size)) h)
		@nil => (vec_at A).here h (Vec A).nil
		@cons q r => (vec_at A).there q ((vec_at A).here h r))
	@there h prior => (prior @(size values index value self =>
		vec_at A (Nat.succ size) (vec_flip A (Nat.succ size) ((Vec A).cons h values))
			(fin_flip (Nat.succ size) (Fin.succ index)) value)
		@here q t => (vec_at A).here q ((Vec A).cons h t)
		@there q rest => (vec_at A).there q ((vec_at A).there h rest));
vec_flip_at :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(a:A)->vec_at A n v i a->
	vec_at A n (vec_flip A n v) (fin_flip n i) a;

flip_head_refill := \A:@ => \h:A => \n:Nat => \xs:List A => \size:list_size A n xs => size
	@(bound values self => (P:Vec A (Nat.succ bound)->@)->
		P (vec_flip A (Nat.succ bound) ((Vec A).cons h (vector_refill A bound values self)))->
		P (vector_refill A (Nat.succ bound) (list_flip A ((List A).cons h values))
			(size_flip_head A h bound values self)))
	@nil => (\P:Vec A (Nat.succ Nat.zero)->@ => \proof:P ((Vec A).cons h (Vec A).nil) => proof)
	@cons q rest => (\P:Vec A (Nat.succ n)->@ =>
		\proof:P (vec_flip A (Nat.succ n) ((Vec A).cons h (vector_refill A n xs size))) => proof);
flip_refill := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => size
	@(bound values self => (P:Vec A bound->@)->P (vec_flip A bound (vector_refill A bound values self))->
		P (vector_refill A bound (list_flip A values) (size_flip A bound values self)))
	@nil => (\P:Vec A Nat.zero->@ => \proof:P (Vec A).nil => proof)
	@cons h rest => flip_head_refill A h (predecessor n) (list_tail A xs) rest;
flip_refill :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->(P:Vec A n->@)->
	P (vec_flip A n (vector_refill A n xs size))->
	P (vector_refill A n (list_flip A xs) (size_flip A n xs size));

flip_lookup_at := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \i:Fin n =>
	flip_refill A n xs size &(\v:Vec A n => vec_at A n v i (sized_lookup A n xs size (fin_flip n i)))
		(position_observe n (fin_flip n (fin_flip n i)) i (fin_flip_twice n i)
			&(\j:Fin n => vec_at A n (vec_flip A n (vector_refill A n xs size)) j
				(sized_lookup A n xs size (fin_flip n i)))
			(vec_flip_at A n (vector_refill A n xs size) (fin_flip n i)
				(sized_lookup A n xs size (fin_flip n i))
				(vec_lookup_at A n (fin_flip n i) (vector_refill A n xs size))));
flip_lookup_at :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->(i:Fin n)->
	vec_at A n (vector_refill A n (list_flip A xs) (size_flip A n xs size)) i
		(sized_lookup A n xs size (fin_flip n i));

vector_prefix := \A:@ => \h:A => \n:Nat => n
	@(size => Vec A (predecessor size)->Vec A size)
	@zero => (\tail:Vec A Nat.zero => (Vec A).nil)
	@succ k => (\tail:Vec A k => (Vec A).cons h tail);
position_prefix := \n:Nat => n
	@(size => position_permutation (predecessor size)->position_permutation size)
	@zero => (\p:position_permutation Nat.zero => position_identity Nat.zero)
	@succ k => (\p:position_permutation k => position_keep k p);

position_compose_forward := \n:Nat => \p:position_permutation n => p
	@(self => (q:position_permutation n)->(i:Fin n)->same_position n
		(position_forward n self (position_forward n q i))
		(position_forward n (position_compose n self q) i))
	@mk f g left_p right_p => (\q:position_permutation n => q
		@(self => (i:Fin n)->same_position n (f (position_forward n self i))
			(position_forward n (position_compose n ((position_permutation n).mk f g left_p right_p) self) i))
		@mk h k left_q right_q => (\i:Fin n => (same_position n).refl (f (h i))));

prefix_at := \A:@ => \n:Nat => \i:Fin n => i
	@(bound index => (h:A)->(left:Vec A (predecessor bound))->(right:Vec A (predecessor bound))->
		(p:position_permutation (predecessor bound))->
		((j:Fin (predecessor bound))->vec_at A (predecessor bound) right j
			(vec_lookup A (predecessor bound) left (position_forward (predecessor bound) p j)))->
		vec_at A bound (vector_prefix A h bound right) index
			(vec_lookup A bound (vector_prefix A h bound left) (position_forward bound (position_prefix bound p) index)))
	@zero k => (\h:A => \left:Vec A k => \right:Vec A k => \p:position_permutation k =>
		\law:(j:Fin k)->vec_at A k right j (vec_lookup A k left (position_forward k p j)) =>
		(vec_at A).here h right)
	@succ rest => (\h:A => \left:Vec A (predecessor n) => \right:Vec A (predecessor n) =>
		\p:position_permutation (predecessor n) =>
		\law:(j:Fin (predecessor n))->vec_at A (predecessor n) right j
			(vec_lookup A (predecessor n) left (position_forward (predecessor n) p j)) =>
		(vec_at A).there h (law rest));

reordering := \A:@ => \n:Nat => \xs:List A => \source_size:list_size A n xs => \ys:List A => @{
	mk : (target_size:list_size A n ys)->(positions:position_permutation n)->
		((i:Fin n)->vec_at A n (vector_refill A n ys target_size) i
			(sized_lookup A n xs source_size (position_forward n positions i)))->*;
};
reordering_make := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \ys:List A =>
	(reordering A n xs size ys).mk;

reordering_identity := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_make A n xs size xs size ((position_permutation n).mk
		&(\i:Fin n => i) &(\i:Fin n => i)
		&(\i:Fin n => (same_position n).refl i) &(\i:Fin n => (same_position n).refl i))
		&(\i:Fin n => vec_lookup_at A n i (vector_refill A n xs size));
reordering_identity :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->
	reordering A n xs size xs;

reordering_keep := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => size
	@(bound values self => (ys:List A)->
		((tail_size:list_size A (predecessor bound) (list_tail A values))->
			reordering A (predecessor bound) (list_tail A values) tail_size ys)->
		reordering A bound values self (list_replace_tail A values ys))
	@nil => (\ys:List A =>
		\next:(tail_size:list_size A Nat.zero (List A).nil)->reordering A Nat.zero (List A).nil tail_size ys =>
		reordering_identity A Nat.zero (List A).nil (list_size A).nil)
	@cons h rest => (\ys:List A =>
		\next:(tail_size:list_size A (predecessor n) (list_tail A xs))->
			reordering A (predecessor n) (list_tail A xs) tail_size ys =>
		(next rest
			@(self => reordering A n xs size ((List A).cons h ys))
			@mk target_size positions law =>
				reordering_make A n xs size ((List A).cons h ys)
				((list_size A).cons h target_size) (position_keep (predecessor n) positions)
				&(\i:Fin n => prefix_at A n i h
					(vector_refill A (predecessor n) (list_tail A xs) rest)
					(vector_refill A (predecessor n) ys target_size) positions law)));
reordering_keep :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->(ys:List A)->
	((tail_size:list_size A (predecessor n) (list_tail A xs))->
		reordering A (predecessor n) (list_tail A xs) tail_size ys)->
	reordering A n xs size (list_replace_tail A xs ys);

reordering_flip := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs =>
	reordering_make A n xs size (list_flip A xs) (size_flip A n xs size) (position_flip n)
		&(flip_lookup_at A n xs size);
reordering_flip :: (A:@)->(n:Nat)->(xs:List A)->(size:list_size A n xs)->
	reordering A n xs size (list_flip A xs);

reordering_compose := \A:@ => \n:Nat => \xs:List A => \ys:List A => \zs:List A =>
	\size:list_size A n xs => \first:reordering A n xs size ys =>
	first @(self => ((middle_size:list_size A n ys)->reordering A n ys middle_size zs)->
		reordering A n xs size zs)
		@mk middle_size first_positions first_law =>
		(\next:(middle_size:list_size A n ys)->reordering A n ys middle_size zs =>
		(next middle_size @(self => reordering A n xs size zs)
			@mk target_size second_positions second_law =>
				reordering_make A n xs size zs target_size (position_compose n first_positions second_positions)
				&(\i:Fin n =>
					position_observe n (position_forward n first_positions (position_forward n second_positions i))
						(position_forward n (position_compose n first_positions second_positions) i)
						(position_compose_forward n first_positions second_positions i)
						&(\j:Fin n => vec_at A n (vector_refill A n zs target_size) i (sized_lookup A n xs size j))
						(vec_at_observe A n (vector_refill A n ys middle_size) (position_forward n second_positions i)
							(sized_lookup A n xs size (position_forward n first_positions (position_forward n second_positions i)))
							(first_law (position_forward n second_positions i))
								&(\value:A => vec_at A n (vector_refill A n zs target_size) i value) (second_law i)))));
reordering_compose :: (A:@)->(n:Nat)->(xs:List A)->(ys:List A)->(zs:List A)->
	(size:list_size A n xs)->reordering A n xs size ys->
	((middle_size:list_size A n ys)->reordering A n ys middle_size zs)->reordering A n xs size zs;

permutation_reordering := \A:@ => \xs:List A => \ys:List A => \p:permutation A xs ys => p
	@(left right self => (n:Nat)->(size:list_size A n left)->reordering A n left size right)
	@nil => (\n:Nat => \size:list_size A n (List A).nil => reordering_identity A n (List A).nil size)
	@keep h x y prior => (\n:Nat => \size:list_size A n ((List A).cons h x) =>
		reordering_keep A n ((List A).cons h x) size y &(*prior (predecessor n)))
	@swap x y tail => (\n:Nat => \size:list_size A n ((List A).cons x ((List A).cons y tail)) =>
		reordering_flip A n ((List A).cons x ((List A).cons y tail)) size)
	@compose x y z first second => (\n:Nat => \size:list_size A n x =>
		reordering_compose A n x y z size (*first n size) &(*second n));
permutation_reordering :: (A:@)->(xs:List A)->(ys:List A)->permutation A xs ys->
	(n:Nat)->(size:list_size A n xs)->reordering A n xs size ys;

reordering_size := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \ys:List A =>
	\p:reordering A n xs size ys => p @mk target_size positions law => target_size;
reordering_positions := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \ys:List A =>
	\p:reordering A n xs size ys => p @mk target_size positions law => positions;
reordering_observe := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \ys:List A =>
	\p:reordering A n xs size ys => p
	@(self => (i:Fin n)->(P:A->@)->
		P (sized_lookup A n ys (reordering_size A n xs size ys self) i)->
		P (sized_lookup A n xs size (position_forward n (reordering_positions A n xs size ys self) i)))
	@mk target_size positions law => (\i:Fin n => \P:A->@ =>
		\proof:P (sized_lookup A n ys target_size i) =>
		vec_at_observe A n (vector_refill A n ys target_size) i
			(sized_lookup A n xs size (position_forward n positions i)) (law i) P proof);
reordering_observe_back := \A:@ => \n:Nat => \xs:List A => \size:list_size A n xs => \ys:List A =>
	\p:reordering A n xs size ys => p
	@(self => (i:Fin n)->(P:A->@)->
		P (sized_lookup A n xs size (position_forward n (reordering_positions A n xs size ys self) i))->
		P (sized_lookup A n ys (reordering_size A n xs size ys self) i))
	@mk target_size positions law => (\i:Fin n => \P:A->@ =>
		\proof:P (sized_lookup A n xs size (position_forward n positions i)) =>
		vec_at_observe_back A n (vector_refill A n ys target_size) i
			(sized_lookup A n xs size (position_forward n positions i)) (law i) P proof);
