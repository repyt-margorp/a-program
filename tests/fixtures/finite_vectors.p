import Nat;
import Fin;
import List;

Vec := \A:@ => @\n:Nat => {nil:* Nat.zero;cons:A->* n->* (Nat.succ n);};
vec_contents := \A:@ => \n:Nat => \v:Vec A n => v
	@nil => (List A).nil
	@cons h t => (List A).cons h *t;
vec_contents :: (A:@)->(n:Nat)->Vec A n->List A;
vec_lookup := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => Fin size->A)
	@nil => (\i:Fin Nat.zero => i @(size self => A)
		@zero k => k @succ rest => rest)
	@cons h t => (\i:Fin n => i @zero k => h @succ rest => *t rest);
vec_lookup :: (A:@)->(n:Nat)->Vec A n->Fin n->A;
vec_tabulate := \A:@ => \n:Nat => n @(size => (Fin size->A)->Vec A size)
	@zero => (\f:Fin Nat.zero->A => (Vec A).nil)
	@succ k => (\f:Fin (Nat.succ k)->A =>
		(Vec A).cons (f (Fin.zero k)) (*k &(\i:Fin k => f (Fin.succ i))));
vec_tabulate :: (A:@)->(n:Nat)->(Fin n->A)->Vec A n;

// A source relation specifies the actual element at an indexed position.
vec_at := \A:@ => @\n:Nat => @\values:Vec A n => @\index:Fin n => @\value:A => {
	here:(h:A)->(t:Vec A n)->* (Nat.succ n) ((Vec A).cons h t) (Fin.zero n) h;
	there:(h:A)->* n values index value->
		* (Nat.succ n) ((Vec A).cons h values) (Fin.succ index) value;
};
Unit := @{unit:*;};
head_property := \A:@ => \n:Nat => n
	@zero => (\v:Vec A Nat.zero => Unit)
	@succ k => (\v:Vec A (Nat.succ k) => vec_at A (Nat.succ k) v (Fin.zero k)
		(vec_lookup A (Nat.succ k) v (Fin.zero k)));
vec_head_at := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => head_property A size self)
	@nil => Unit.unit
	@cons h t => (vec_at A).here h t;
vec_head_at :: (A:@)->(n:Nat)->(v:Vec A n)->head_property A n v;

predecessor := \n:Nat => n @zero => Nat.zero @succ k => k;
vec_tail := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => Vec A (predecessor size))
	@nil => (Vec A).nil
	@cons h t => t;
vec_tail :: (A:@)->(n:Nat)->Vec A n->Vec A (predecessor n);
// The motive belongs to Match. A post-check must not supply it.
tail_property := \A:@ => \n:Nat => n
	@(size => (v:Vec A size)->(i:Fin (predecessor size))->@)
	@zero => (\v:Vec A Nat.zero => \i:Fin Nat.zero => Unit)
	@succ k => (\v:Vec A (Nat.succ k) => \i:Fin k =>
		vec_at A (Nat.succ k) v (Fin.succ i) (vec_lookup A (Nat.succ k) v (Fin.succ i)));
vec_tail_at := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => (i:Fin (predecessor size))->
		vec_at A (predecessor size) (vec_tail A size self) i
			(vec_lookup A (predecessor size) (vec_tail A size self) i)->
		tail_property A size self i)
	@nil => (\i:Fin Nat.zero =>
		\p:vec_at A Nat.zero (Vec A).nil i (vec_lookup A Nat.zero (Vec A).nil i) => Unit.unit)
	@cons h t => (\i:Fin (predecessor n) =>
		\p:vec_at A (predecessor n) t i (vec_lookup A (predecessor n) t i) =>
		(vec_at A).there h p);
vec_tail_at :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin (predecessor n))->
	vec_at A (predecessor n) (vec_tail A n v) i (vec_lookup A (predecessor n) (vec_tail A n v) i)->
	tail_property A n v i;
vec_lookup_at := \A:@ => \n:Nat => \i:Fin n => i
	@(size self => (v:Vec A size)->vec_at A size v self (vec_lookup A size v self))
	@zero k => (\v:Vec A (Nat.succ k) => vec_head_at A (Nat.succ k) v)
	@succ rest => (\v:Vec A n => vec_tail_at A n v rest (*rest (vec_tail A n v)));
vec_lookup_at :: (A:@)->(n:Nat)->(i:Fin n)->(v:Vec A n)->
	vec_at A n v i (vec_lookup A n v i);

vec_tabulate_at := \A:@ => \n:Nat => \i:Fin n => i
	@(size self => (f:Fin size->A)->vec_at A size (vec_tabulate A size f) self (f self))
	@zero k => (\f:Fin (Nat.succ k)->A =>
		(vec_at A).here (f (Fin.zero k))
			(vec_tabulate A k &(\j:Fin k => f (Fin.succ j))))
	@succ rest => (\f:Fin n->A =>
		(vec_at A).there (f (Fin.zero (predecessor n)))
			(*rest &(\j:Fin (predecessor n) => f (Fin.succ j))));
vec_tabulate_at :: (A:@)->(n:Nat)->(i:Fin n)->(f:Fin n->A)->
	vec_at A n (vec_tabulate A n f) i (f i);

vec_at_observe := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n => \a:A =>
	\p:vec_at A n v i a => p
	@(size values index value self => (P:A->@)->P (vec_lookup A size values index)->P value)
	@here h t => (\P:A->@ => \proof:P h => proof)
	@there h prior => (\P:A->@ => \proof:P (vec_lookup A n v i) => *prior P proof);
vec_at_observe :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(a:A)->
	vec_at A n v i a->(P:A->@)->P (vec_lookup A n v i)->P a;

vec_at_observe_back := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n => \a:A =>
	\p:vec_at A n v i a => p
	@(size values index value self => (P:A->@)->P value->P (vec_lookup A size values index))
	@here h t => (\P:A->@ => \proof:P h => proof)
	@there h prior => (\P:A->@ => \proof:P a => *prior P proof);
vec_at_observe_back :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(a:A)->
	vec_at A n v i a->(P:A->@)->P a->P (vec_lookup A n v i);

// Two entries at the same position preserve any explicitly supplied predicate.
vec_at_unique := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n => \a:A => \b:A =>
	\p:vec_at A n v i a => \q:vec_at A n v i b => \P:A->@ => \proof:P a =>
	vec_at_observe A n v i b q P (vec_at_observe_back A n v i a p P proof);
vec_at_unique :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(a:A)->(b:A)->
	vec_at A n v i a->vec_at A n v i b->(P:A->@)->P a->P b;

vec_reconstruct_observe := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n =>
	\P:A->@ => \proof:P (vec_lookup A n v i) =>
	vec_at_observe_back A n (vec_tabulate A n &(vec_lookup A n v)) i
		(vec_lookup A n v i) (vec_tabulate_at A n i &(vec_lookup A n v)) P proof;
vec_reconstruct_observe :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->
	P (vec_lookup A n v i)->P (vec_lookup A n (vec_tabulate A n &(vec_lookup A n v)) i);

vec_reconstruct_observe_back := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n =>
	\P:A->@ => \proof:P (vec_lookup A n (vec_tabulate A n &(vec_lookup A n v)) i) =>
	vec_at_observe A n (vec_tabulate A n &(vec_lookup A n v)) i
		(vec_lookup A n v i) (vec_tabulate_at A n i &(vec_lookup A n v)) P proof;
vec_reconstruct_observe_back :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->
	P (vec_lookup A n (vec_tabulate A n &(vec_lookup A n v)) i)->P (vec_lookup A n v i);
