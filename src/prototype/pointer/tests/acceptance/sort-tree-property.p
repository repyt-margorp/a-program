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
import read_sorted;
import one;
import two;
import three;
four := Nat.succ three;
sample := (List Nat).cons two ((List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one (List Nat).nil)));
expected_value := (List Nat).cons Nat.zero ((List Nat).cons one ((List Nat).cons one ((List Nat).cons two (List Nat).nil)));
main := *treeSort Nat (&natLessOrEqual) sample @ys => read_sorted ys (tree_correct sample ys @ys);
empty := *treeSort Nat (&natLessOrEqual) (List Nat).nil @ys => read_sorted ys (tree_correct (List Nat).nil ys @ys);
singleton := *treeSort Nat (&natLessOrEqual) ((List Nat).cons one (List Nat).nil) @ys =>
	read_sorted ys (tree_correct ((List Nat).cons one (List Nat).nil) ys @ys);
already := *treeSort Nat (&natLessOrEqual) expected_value @ys => read_sorted ys (tree_correct expected_value ys @ys);
reversed_value := (List Nat).cons two ((List Nat).cons one ((List Nat).cons one ((List Nat).cons Nat.zero (List Nat).nil)));
reversed := *treeSort Nat (&natLessOrEqual) reversed_value @ys => read_sorted ys (tree_correct reversed_value ys @ys);
packet_value := *treeSort Nat (&natLessOrEqual) sample @ys => ys;
direct_value := treeSort Nat (&natLessOrEqual) sample;
zero := Nat.zero;
one_value := Nat.succ zero;
