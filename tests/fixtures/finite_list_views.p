// Lawful sequence views and same-length refill, using ordinary source proofs.
import Nat;
import List;
import Vec;
import vec_contents;
import Fin;
import vec_lookup;
import predecessor;
import vec_at;
import vec_lookup_at;
import vec_at_observe;
import vec_at_observe_back;

list_length := \A:@ => \xs:List A => xs
	@nil => Nat.zero
	@cons h t => Nat.succ *t;
list_vector := \A:@ => \xs:List A => xs
	@(self => Vec A (list_length A self))
	@nil => (Vec A).nil
	@cons h t => (Vec A).cons h *t;
list_vector :: (A:@)->(xs:List A)->Vec A (list_length A xs);

list_vector_view := \A:@ => @\n:Nat => @\values:Vec A n => @\contents:List A => {
	nil:* Nat.zero (Vec A).nil (List A).nil;
	cons:(h:A)->* n values contents->
		* (Nat.succ n) ((Vec A).cons h values) ((List A).cons h contents);
};
list_vector_covers := \A:@ => \xs:List A => xs
	@(self => list_vector_view A (list_length A self) (list_vector A self) self)
	@nil => (list_vector_view A).nil
	@cons h t => (list_vector_view A).cons h *t;
list_vector_covers :: (A:@)->(xs:List A)->
	list_vector_view A (list_length A xs) (list_vector A xs) xs;

vector_list_covers := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => list_vector_view A size self (vec_contents A size self))
	@nil => (list_vector_view A).nil
	@cons h t => (list_vector_view A).cons h *t;
vector_list_covers :: (A:@)->(n:Nat)->(v:Vec A n)->
	list_vector_view A n v (vec_contents A n v);

view_contents_observe := \A:@ => \n:Nat => \v:Vec A n => \xs:List A =>
	\p:list_vector_view A n v xs => p
	@(size values contents self => (P:List A->@)->P (vec_contents A size values)->P contents)
	@nil => (\P:List A->@ => \proof:P (List A).nil => proof)
	@cons h prior => (\P:List A->@ => \proof:P (vec_contents A n v) =>
		*prior &(\tail:List A => P ((List A).cons h tail)) proof);
view_contents_observe :: (A:@)->(n:Nat)->(v:Vec A n)->(xs:List A)->
	list_vector_view A n v xs->(P:List A->@)->P (vec_contents A n v)->P xs;

view_contents_observe_back := \A:@ => \n:Nat => \v:Vec A n => \xs:List A =>
	\p:list_vector_view A n v xs => p
	@(size values contents self => (P:List A->@)->P contents->P (vec_contents A size values))
	@nil => (\P:List A->@ => \proof:P (List A).nil => proof)
	@cons h prior => (\P:List A->@ => \proof:P xs =>
		*prior &(\tail:List A => P ((List A).cons h tail)) proof);
view_contents_observe_back :: (A:@)->(n:Nat)->(v:Vec A n)->(xs:List A)->
	list_vector_view A n v xs->(P:List A->@)->P xs->P (vec_contents A n v);

list_roundtrip := \A:@ => \xs:List A => \P:List A->@ => \proof:P xs =>
	view_contents_observe_back A (list_length A xs) (list_vector A xs) xs
		(list_vector_covers A xs) P proof;
list_roundtrip :: (A:@)->(xs:List A)->(P:List A->@)->P xs->
	P (vec_contents A (list_length A xs) (list_vector A xs));
list_roundtrip_back := \A:@ => \xs:List A => \P:List A->@ =>
	\proof:P (vec_contents A (list_length A xs) (list_vector A xs)) =>
	view_contents_observe A (list_length A xs) (list_vector A xs) xs
		(list_vector_covers A xs) P proof;
list_roundtrip_back :: (A:@)->(xs:List A)->(P:List A->@)->
	P (vec_contents A (list_length A xs) (list_vector A xs))->P xs;

view_length_observe := \A:@ => \n:Nat => \v:Vec A n => \xs:List A =>
	\p:list_vector_view A n v xs => p
	@(size values contents self => (P:Nat->@)->P (list_length A contents)->P size)
	@nil => (\P:Nat->@ => \proof:P Nat.zero => proof)
	@cons h prior => (\P:Nat->@ => \proof:P (list_length A xs) =>
		*prior &(\size:Nat => P (Nat.succ size)) proof);
view_length_observe :: (A:@)->(n:Nat)->(v:Vec A n)->(xs:List A)->
	list_vector_view A n v xs->(P:Nat->@)->P (list_length A xs)->P n;

// A refill carries a checked traversal of exactly n replacement elements.
list_size := \A:@ => @\n:Nat => @\values:List A => {
	nil:* Nat.zero (List A).nil;
	cons:(h:A)->* n values->* (Nat.succ n) ((List A).cons h values);
};
list_sized := \A:@ => \xs:List A => xs
	@(self => list_size A (list_length A self) self)
	@nil => (list_size A).nil
	@cons h t => (list_size A).cons h *t;
list_sized :: (A:@)->(xs:List A)->list_size A (list_length A xs) xs;
vector_contents_size := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => list_size A size (vec_contents A size self))
	@nil => (list_size A).nil
	@cons h t => (list_size A).cons h *t;
vector_contents_size :: (A:@)->(n:Nat)->(v:Vec A n)->
	list_size A n (vec_contents A n v);

vector_refill := \A:@ => \n:Nat => \xs:List A => \p:list_size A n xs => p
	@(size values self => Vec A size)
	@nil => (Vec A).nil
	@cons h prior => (Vec A).cons h *prior;
vector_refill :: (A:@)->(n:Nat)->(xs:List A)->list_size A n xs->Vec A n;
vector_refill_covers := \A:@ => \n:Nat => \xs:List A => \p:list_size A n xs => p
	@(size values self => list_vector_view A size (vector_refill A size values self) values)
	@nil => (list_vector_view A).nil
	@cons h prior => (list_vector_view A).cons h *prior;
vector_refill_covers :: (A:@)->(n:Nat)->(xs:List A)->(p:list_size A n xs)->
	list_vector_view A n (vector_refill A n xs p) xs;
refill_contents := \A:@ => \n:Nat => \xs:List A => \p:list_size A n xs =>
	\P:List A->@ => \proof:P xs =>
	view_contents_observe_back A n (vector_refill A n xs p) xs
		(vector_refill_covers A n xs p) P proof;
refill_contents :: (A:@)->(n:Nat)->(xs:List A)->(p:list_size A n xs)->
	(P:List A->@)->P xs->P (vec_contents A n (vector_refill A n xs p));
refill_contents_back := \A:@ => \n:Nat => \xs:List A => \p:list_size A n xs =>
	\P:List A->@ => \proof:P (vec_contents A n (vector_refill A n xs p)) =>
	view_contents_observe A n (vector_refill A n xs p) xs
		(vector_refill_covers A n xs p) P proof;
refill_contents_back :: (A:@)->(n:Nat)->(xs:List A)->(p:list_size A n xs)->
	(P:List A->@)->P (vec_contents A n (vector_refill A n xs p))->P xs;

vector_rebuild := \A:@ => \n:Nat => \v:Vec A n =>
	vector_refill A n (vec_contents A n v) (vector_contents_size A n v);
vector_rebuild :: (A:@)->(n:Nat)->Vec A n->Vec A n;
vector_rebuild_at := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n => \a:A =>
	\p:vec_at A n v i a => p
	@(size values index value self => vec_at A size (vector_rebuild A size values) index value)
	@here h t => (vec_at A).here h (vector_rebuild A (predecessor n) t)
	@there h prior => (vec_at A).there h *prior;
vector_rebuild_at :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(a:A)->
	vec_at A n v i a->vec_at A n (vector_rebuild A n v) i a;

vector_roundtrip := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n =>
	\P:A->@ => \proof:P (vec_lookup A n v i) =>
	vec_at_observe_back A n (vector_rebuild A n v) i (vec_lookup A n v i)
		(vector_rebuild_at A n v i (vec_lookup A n v i) (vec_lookup_at A n i v)) P proof;
vector_roundtrip :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->
	P (vec_lookup A n v i)->P (vec_lookup A n (vector_rebuild A n v) i);
vector_roundtrip_back := \A:@ => \n:Nat => \v:Vec A n => \i:Fin n =>
	\P:A->@ => \proof:P (vec_lookup A n (vector_rebuild A n v) i) =>
	vec_at_observe A n (vector_rebuild A n v) i (vec_lookup A n v i)
		(vector_rebuild_at A n v i (vec_lookup A n v i) (vec_lookup_at A n i v)) P proof;
vector_roundtrip_back :: (A:@)->(n:Nat)->(v:Vec A n)->(i:Fin n)->(P:A->@)->
	P (vec_lookup A n (vector_rebuild A n v) i)->P (vec_lookup A n v i);
