// Ordinary bounds and graph induction for the unchanged PR #30 tree sort.
import Nat;
import Bool;
import List;
import Tree;
import LE;
import Sorted;
import AllFrom;
import natLessOrEqual;
import treeInsert;
import treeBuild;
import treeToList;
import treeSort;
import append;
import trans;
import le_next;
import direct;
import unwrap;
import yes_order;
import no_order;
import all_trans;
import tail_sorted;
import tail_bound;
import all_head;
import all_tail;
TreeLower := \lo:Nat => @\tree:Tree Nat => {
	empty:* (Tree Nat).empty;
	node:(x:Nat)->(l:Tree Nat)->(r:Tree Nat)->LE lo x->* l->* r->* ((Tree Nat).node x l r);
};
TreeUpper := \hi:Nat => @\tree:Tree Nat => {
	empty:* (Tree Nat).empty;
	node:(x:Nat)->(l:Tree Nat)->(r:Tree Nat)->LE x hi->* l->* r->* ((Tree Nat).node x l r);
};
OrderedTree := @\tree:Tree Nat => {
	empty:* (Tree Nat).empty;
	node:(x:Nat)->(l:Tree Nat)->(r:Tree Nat)->TreeUpper x l->TreeLower x r->* l->* r->* ((Tree Nat).node x l r);
};
lower_root := \lo:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeLower lo ((Tree Nat).node x l r) => p @node y a b root left right => root;
lower_left := \lo:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeLower lo ((Tree Nat).node x l r) => p @node y a b root left right => left;
lower_right := \lo:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeLower lo ((Tree Nat).node x l r) => p @node y a b root left right => right;
upper_root := \hi:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeUpper hi ((Tree Nat).node x l r) => p @node y a b root left right => root;
upper_left := \hi:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeUpper hi ((Tree Nat).node x l r) => p @node y a b root left right => left;
upper_right := \hi:Nat => \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:TreeUpper hi ((Tree Nat).node x l r) => p @node y a b root left right => right;
ordered_left := \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:OrderedTree ((Tree Nat).node x l r) => p @node y a b upper lower left right => left;
ordered_right := \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:OrderedTree ((Tree Nat).node x l r) => p @node y a b upper lower left right => right;
ordered_upper := \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:OrderedTree ((Tree Nat).node x l r) => p @node y a b upper lower left right => upper;
ordered_lower := \x:Nat => \l:Tree Nat => \r:Tree Nat =>
	\p:OrderedTree ((Tree Nat).node x l r) => p @node y a b upper lower left right => lower;
insert_lower := \v:Nat => \tree:Tree Nat => \out:Tree Nat =>
	\g:@treeInsert Nat (&natLessOrEqual) v tree out => g
	@case0 => (\lo:Nat => \lv:LE lo v => \old:TreeLower lo (Tree Nat).empty =>
		(TreeLower lo).node v (Tree Nat).empty (Tree Nat).empty lv (TreeLower lo).empty (TreeLower lo).empty)
	@case1 root left right trace inserted rest => (\lo:Nat => \lv:LE lo v =>
		\old:TreeLower lo ((Tree Nat).node root left right) =>
		(TreeLower lo).node root inserted right (lower_root lo root left right old)
			(*rest lo lv (lower_left lo root left right old)) (lower_right lo root left right old))
	@case2 root left right trace inserted rest => (\lo:Nat => \lv:LE lo v =>
		\old:TreeLower lo ((Tree Nat).node root left right) =>
		(TreeLower lo).node root left inserted (lower_root lo root left right old)
			(lower_left lo root left right old) (*rest lo lv (lower_right lo root left right old)));
insert_lower :: (v:Nat)->(tree:Tree Nat)->(out:Tree Nat)->@treeInsert Nat (&natLessOrEqual) v tree out->
	(lo:Nat)->LE lo v->TreeLower lo tree->TreeLower lo out;
insert_upper := \v:Nat => \tree:Tree Nat => \out:Tree Nat =>
	\g:@treeInsert Nat (&natLessOrEqual) v tree out => g
	@case0 => (\hi:Nat => \vh:LE v hi => \old:TreeUpper hi (Tree Nat).empty =>
		(TreeUpper hi).node v (Tree Nat).empty (Tree Nat).empty vh (TreeUpper hi).empty (TreeUpper hi).empty)
	@case1 root left right trace inserted rest => (\hi:Nat => \vh:LE v hi =>
		\old:TreeUpper hi ((Tree Nat).node root left right) =>
		(TreeUpper hi).node root inserted right (upper_root hi root left right old)
			(*rest hi vh (upper_left hi root left right old)) (upper_right hi root left right old))
	@case2 root left right trace inserted rest => (\hi:Nat => \vh:LE v hi =>
		\old:TreeUpper hi ((Tree Nat).node root left right) =>
		(TreeUpper hi).node root left inserted (upper_root hi root left right old)
			(upper_left hi root left right old) (*rest hi vh (upper_right hi root left right old)));
insert_upper :: (v:Nat)->(tree:Tree Nat)->(out:Tree Nat)->@treeInsert Nat (&natLessOrEqual) v tree out->
	(hi:Nat)->LE v hi->TreeUpper hi tree->TreeUpper hi out;
ordered_insert_left := \v:Nat => \root:Nat => \left:Tree Nat => \right:Tree Nat =>
	\vr:LE v root => \inserted:Tree Nat => \rest:@treeInsert Nat (&natLessOrEqual) v left inserted =>
	\ih:OrderedTree left->OrderedTree inserted => \old:OrderedTree ((Tree Nat).node root left right) =>
	OrderedTree.node root inserted right
		(insert_upper v left inserted rest root vr (ordered_upper root left right old))
		(ordered_lower root left right old) (ih (ordered_left root left right old)) (ordered_right root left right old);
ordered_insert_right := \v:Nat => \root:Nat => \left:Tree Nat => \right:Tree Nat =>
	\rv:LE root v => \inserted:Tree Nat => \rest:@treeInsert Nat (&natLessOrEqual) v right inserted =>
	\ih:OrderedTree right->OrderedTree inserted => \old:OrderedTree ((Tree Nat).node root left right) =>
	OrderedTree.node root left inserted
		(ordered_upper root left right old)
		(insert_lower v right inserted rest root rv (ordered_lower root left right old))
		(ordered_left root left right old) (ih (ordered_right root left right old));
insert_ordered := \v:Nat => \tree:Tree Nat => \out:Tree Nat =>
	\g:@treeInsert Nat (&natLessOrEqual) v tree out => g
	@case0 => (\old:OrderedTree (Tree Nat).empty =>
		OrderedTree.node v (Tree Nat).empty (Tree Nat).empty (TreeUpper v).empty (TreeLower v).empty
			OrderedTree.empty OrderedTree.empty)
	@case1 root left right trace inserted rest => ordered_insert_left v root left right
		(trace @case0 r => yes_order v r (unwrap v r (direct v r))) inserted rest &*rest
	@case2 root left right trace inserted rest => ordered_insert_right v root left right
		(trans root (Nat.succ root) (le_next root) v
			(trace @case0 r => no_order v r (unwrap v r (direct v r)))) inserted rest &*rest;
insert_ordered :: (v:Nat)->(tree:Tree Nat)->(out:Tree Nat)->@treeInsert Nat (&natLessOrEqual) v tree out->
	OrderedTree tree->OrderedTree out;
build_ordered := \xs:List Nat => \out:Tree Nat => \g:@treeBuild Nat (&natLessOrEqual) xs out => g
	@case0 => OrderedTree.empty
	@case1 head tail built rest inserted trace => insert_ordered head built inserted trace *rest;
build_ordered :: (xs:List Nat)->(out:Tree Nat)->@treeBuild Nat (&natLessOrEqual) xs out->OrderedTree out;
AllTo := \hi:Nat => @\xs:List Nat => {
	nil:* (List Nat).nil;
	cons:(head:Nat)->(tail:List Nat)->LE head hi->* tail->* ((List Nat).cons head tail);
};
to_head := \hi:Nat => \head:Nat => \tail:List Nat => \p:AllTo hi ((List Nat).cons head tail) =>
	p @cons h t bound rest => bound;
to_tail := \hi:Nat => \head:Nat => \tail:List Nat => \p:AllTo hi ((List Nat).cons head tail) =>
	p @cons h t bound rest => rest;
append_lower := \xs:List Nat => \ys:List Nat => \zs:List Nat => \g:@append Nat xs ys zs => g
	@case0 following => (\lo:Nat => \left:AllFrom lo (List Nat).nil => \right:AllFrom lo following => right)
	@case1 head tail following output rest => (\lo:Nat =>
		\left:AllFrom lo ((List Nat).cons head tail) => \right:AllFrom lo following =>
		(AllFrom lo).cons head output (all_head lo head tail left) (*rest lo (all_tail lo head tail left) right));
append_lower :: (xs:List Nat)->(ys:List Nat)->(zs:List Nat)->@append Nat xs ys zs->
	(lo:Nat)->AllFrom lo xs->AllFrom lo ys->AllFrom lo zs;
append_upper := \xs:List Nat => \ys:List Nat => \zs:List Nat => \g:@append Nat xs ys zs => g
	@case0 following => (\hi:Nat => \left:AllTo hi (List Nat).nil => \right:AllTo hi following => right)
	@case1 head tail following output rest => (\hi:Nat =>
		\left:AllTo hi ((List Nat).cons head tail) => \right:AllTo hi following =>
		(AllTo hi).cons head output (to_head hi head tail left) (*rest hi (to_tail hi head tail left) right));
append_upper :: (xs:List Nat)->(ys:List Nat)->(zs:List Nat)->@append Nat xs ys zs->
	(hi:Nat)->AllTo hi xs->AllTo hi ys->AllTo hi zs;
append_sorted_step := \head:Nat => \tail:List Nat => \following:List Nat => \output:List Nat =>
	\rest:@append Nat tail following output =>
	\pivot:Nat => \ih:Sorted tail->AllTo pivot tail->Sorted following->AllFrom pivot following->Sorted output =>
	\sl:Sorted ((List Nat).cons head tail) =>
		\ul:AllTo pivot ((List Nat).cons head tail) => \sr:Sorted following => \lr:AllFrom pivot following =>
		Sorted.cons head output
			(append_lower tail following output rest head (tail_bound head tail sl)
				(all_trans pivot following lr head (to_head pivot head tail ul)))
			(ih (tail_sorted head tail sl) (to_tail pivot head tail ul) sr lr);
append_sorted := \pivot:Nat => \xs:List Nat => \ys:List Nat => \zs:List Nat => \g:@append Nat xs ys zs => g
	@case0 following => (\sl:Sorted (List Nat).nil => \ul:AllTo pivot (List Nat).nil =>
		\sr:Sorted following => \lr:AllFrom pivot following => sr)
	@case1 head tail following output rest => append_sorted_step head tail following output rest pivot &*rest;
append_sorted :: (pivot:Nat)->(xs:List Nat)->(ys:List Nat)->(zs:List Nat)->@append Nat xs ys zs->
	Sorted xs->AllTo pivot xs->Sorted ys->AllFrom pivot ys->Sorted zs;
list_lower := \tree:Tree Nat => \xs:List Nat => \g:@treeToList Nat tree xs => g
	@case0 => (\lo:Nat => \p:TreeLower lo (Tree Nat).empty => (AllFrom lo).nil)
	@case1 root left right lv lg rv rg output ag => (\lo:Nat => \p:TreeLower lo ((Tree Nat).node root left right) =>
		append_lower lv ((List Nat).cons root rv) output ag lo
			(*lg lo (lower_left lo root left right p))
			((AllFrom lo).cons root rv (lower_root lo root left right p) (*rg lo (lower_right lo root left right p))));
list_lower :: (tree:Tree Nat)->(xs:List Nat)->@treeToList Nat tree xs->(lo:Nat)->TreeLower lo tree->AllFrom lo xs;
list_upper := \tree:Tree Nat => \xs:List Nat => \g:@treeToList Nat tree xs => g
	@case0 => (\hi:Nat => \p:TreeUpper hi (Tree Nat).empty => (AllTo hi).nil)
	@case1 root left right lv lg rv rg output ag => (\hi:Nat => \p:TreeUpper hi ((Tree Nat).node root left right) =>
		append_upper lv ((List Nat).cons root rv) output ag hi
			(*lg hi (upper_left hi root left right p))
			((AllTo hi).cons root rv (upper_root hi root left right p) (*rg hi (upper_right hi root left right p))));
list_upper :: (tree:Tree Nat)->(xs:List Nat)->@treeToList Nat tree xs->(hi:Nat)->TreeUpper hi tree->AllTo hi xs;
le_refl := \n:Nat => n @zero => LE.zero Nat.zero @succ k => LE.succ k k *k;
list_sorted := \tree:Tree Nat => \xs:List Nat => \g:@treeToList Nat tree xs => g
	@case0 => (\p:OrderedTree (Tree Nat).empty => Sorted.nil)
	@case1 root left right lv lg rv rg output ag => (\p:OrderedTree ((Tree Nat).node root left right) =>
		append_sorted root lv ((List Nat).cons root rv) output ag
			(*lg (ordered_left root left right p))
			(list_upper left lv lg root (ordered_upper root left right p))
			(Sorted.cons root rv (list_lower right rv rg root (ordered_lower root left right p))
				(*rg (ordered_right root left right p)))
			((AllFrom root).cons root rv (le_refl root) (list_lower right rv rg root (ordered_lower root left right p))));
list_sorted :: (tree:Tree Nat)->(xs:List Nat)->@treeToList Nat tree xs->OrderedTree tree->Sorted xs;
tree_correct := \xs:List Nat => \ys:List Nat => \g:@treeSort Nat (&natLessOrEqual) xs ys => g
	@case0 input built construction output traversal => list_sorted built output traversal (build_ordered input built construction);
tree_correct :: (xs:List Nat)->(ys:List Nat)->@treeSort Nat (&natLessOrEqual) xs ys->Sorted ys;

// Direct preservation connects the same predicates to the ordinary algorithm.
insert_choice := \v:Nat => \root:Nat => \left:Tree Nat => \right:Tree Nat => \answer:Bool => answer
	@true => (Tree Nat).node root (treeInsert Nat (&natLessOrEqual) v left) right
	@false => (Tree Nat).node root left (treeInsert Nat (&natLessOrEqual) v right);
insert_lower_result := \v:Nat => \tree:Tree Nat => tree
	@(self => (lo:Nat)->LE lo v->TreeLower lo self->TreeLower lo (treeInsert Nat (&natLessOrEqual) v self))
	@empty => (\lo:Nat => \lv:LE lo v => \old:TreeLower lo (Tree Nat).empty =>
		(TreeLower lo).node v (Tree Nat).empty (Tree Nat).empty lv (TreeLower lo).empty (TreeLower lo).empty)
	@node root left right => (\lo:Nat => \lv:LE lo v => \old:TreeLower lo ((Tree Nat).node root left right) =>
		(natLessOrEqual v root) @(b => TreeLower lo (insert_choice v root left right b))
		@true => (TreeLower lo).node root (treeInsert Nat (&natLessOrEqual) v left) right
			(lower_root lo root left right old) (*left lo lv (lower_left lo root left right old)) (lower_right lo root left right old)
		@false => (TreeLower lo).node root left (treeInsert Nat (&natLessOrEqual) v right)
			(lower_root lo root left right old) (lower_left lo root left right old) (*right lo lv (lower_right lo root left right old)));
insert_lower_result :: (v:Nat)->(tree:Tree Nat)->(lo:Nat)->LE lo v->TreeLower lo tree->
	TreeLower lo (treeInsert Nat (&natLessOrEqual) v tree);
insert_upper_result := \v:Nat => \tree:Tree Nat => tree
	@(self => (hi:Nat)->LE v hi->TreeUpper hi self->TreeUpper hi (treeInsert Nat (&natLessOrEqual) v self))
	@empty => (\hi:Nat => \vh:LE v hi => \old:TreeUpper hi (Tree Nat).empty =>
		(TreeUpper hi).node v (Tree Nat).empty (Tree Nat).empty vh (TreeUpper hi).empty (TreeUpper hi).empty)
	@node root left right => (\hi:Nat => \vh:LE v hi => \old:TreeUpper hi ((Tree Nat).node root left right) =>
		(natLessOrEqual v root) @(b => TreeUpper hi (insert_choice v root left right b))
		@true => (TreeUpper hi).node root (treeInsert Nat (&natLessOrEqual) v left) right
			(upper_root hi root left right old) (*left hi vh (upper_left hi root left right old)) (upper_right hi root left right old)
		@false => (TreeUpper hi).node root left (treeInsert Nat (&natLessOrEqual) v right)
			(upper_root hi root left right old) (upper_left hi root left right old) (*right hi vh (upper_right hi root left right old)));
insert_upper_result :: (v:Nat)->(tree:Tree Nat)->(hi:Nat)->LE v hi->TreeUpper hi tree->
	TreeUpper hi (treeInsert Nat (&natLessOrEqual) v tree);
import Decision;
ordered_result_node := \v:Nat => \root:Nat => \left:Tree Nat => \right:Tree Nat =>
	\old:OrderedTree ((Tree Nat).node root left right) =>
	\il:OrderedTree (treeInsert Nat (&natLessOrEqual) v left) =>
	\ir:OrderedTree (treeInsert Nat (&natLessOrEqual) v right) => \answer:Bool => answer
	@(b => Decision v root b->OrderedTree (insert_choice v root left right b))
	@true => (\d:Decision v root Bool.true => OrderedTree.node root (treeInsert Nat (&natLessOrEqual) v left) right
		(insert_upper_result v left root (yes_order v root d) (ordered_upper root left right old))
		(ordered_lower root left right old) il (ordered_right root left right old))
	@false => (\d:Decision v root Bool.false => OrderedTree.node root left (treeInsert Nat (&natLessOrEqual) v right)
		(ordered_upper root left right old)
		(insert_lower_result v right root (trans root (Nat.succ root) (le_next root) v (no_order v root d))
			(ordered_lower root left right old)) (ordered_left root left right old) ir);
insert_ordered_result := \v:Nat => \tree:Tree Nat => \old:OrderedTree tree => old
	@(self proof => OrderedTree (treeInsert Nat (&natLessOrEqual) v self))
	@empty => OrderedTree.node v (Tree Nat).empty (Tree Nat).empty (TreeUpper v).empty (TreeLower v).empty
		OrderedTree.empty OrderedTree.empty
	@node root left right upper lower ol ord =>
		ordered_result_node v root left right (OrderedTree.node root left right upper lower ol ord)
			*ol *ord (natLessOrEqual v root) (unwrap v root (direct v root));
insert_ordered_result :: (v:Nat)->(tree:Tree Nat)->OrderedTree tree->OrderedTree (treeInsert Nat (&natLessOrEqual) v tree);
build_ordered_result := \xs:List Nat => xs @(self => OrderedTree (treeBuild Nat (&natLessOrEqual) self))
	@nil => OrderedTree.empty
	@cons h t => insert_ordered_result h (treeBuild Nat (&natLessOrEqual) t) *t;
append_graph := \xs:List Nat => xs @(self => (ys:List Nat)->@append Nat self ys (append Nat self ys))
	@nil => (\ys:List Nat => (@append Nat).case0 ys)
	@cons h t => (\ys:List Nat => (@append Nat).case1 h t ys (append Nat t ys) (*t ys));
append_graph :: (xs:List Nat)->(ys:List Nat)->@append Nat xs ys (append Nat xs ys);
traversal_graph := \tree:Tree Nat => tree @(self => @treeToList Nat self (treeToList Nat self))
	@empty => (@treeToList Nat).case0
	@node root left right => (@treeToList Nat).case1 root left right
		(treeToList Nat left) *left (treeToList Nat right) *right
		(append Nat (treeToList Nat left) ((List Nat).cons root (treeToList Nat right)))
		(append_graph (treeToList Nat left) ((List Nat).cons root (treeToList Nat right)));
traversal_graph :: (tree:Tree Nat)->@treeToList Nat tree (treeToList Nat tree);
tree_result_sorted := \xs:List Nat => list_sorted (treeBuild Nat (&natLessOrEqual) xs)
	(treeSort Nat (&natLessOrEqual) xs) (traversal_graph (treeBuild Nat (&natLessOrEqual) xs)) (build_ordered_result xs);
tree_result_sorted :: (xs:List Nat)->Sorted (treeSort Nat (&natLessOrEqual) xs);
import read_sorted;
import one;
import two;
import three;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one (List Nat).nil)));
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one ((List Nat).cons two (List Nat).nil)));
main := read_sorted (treeSort Nat (&natLessOrEqual) sample) (tree_result_sorted sample);
empty := read_sorted (treeSort Nat (&natLessOrEqual) (List Nat).nil) (tree_result_sorted (List Nat).nil);
singleton := read_sorted (treeSort Nat (&natLessOrEqual) ((List Nat).cons one (List Nat).nil))
	(tree_result_sorted ((List Nat).cons one (List Nat).nil));
already := read_sorted (treeSort Nat (&natLessOrEqual) expected_value) (tree_result_sorted expected_value);
reversed_value := (List Nat).cons two ((List Nat).cons one ((List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil)));
reversed := read_sorted (treeSort Nat (&natLessOrEqual) reversed_value) (tree_result_sorted reversed_value);
packet_value := treeSort Nat (&natLessOrEqual) sample;
direct_value := treeSort Nat (&natLessOrEqual) sample;
zero := Nat.zero;
one_value := Nat.succ zero;
