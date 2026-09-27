// Equal keys have different labels. No stability contract is imposed.
Item := @{ mk:Bool->Nat->*; };
item_key := \v:Item => v @mk key label => key;
item_label := \v:Item => v @mk key label => label;
item_order := \x:Item => \y:Item => bool_order (item_key x) (item_key y);
item_le := \x:Item => \y:Item => bool_le (item_key x) (item_key y);
item_decision := \x:Item => \y:Item => \d:Bool =>
	\p:general_decision Bool &bool_order (item_key x) (item_key y) d => p
	@(answer self => general_decision Item &item_order x y answer)
	@yes edge => (general_decision Item &item_order x y).yes edge
	@no edge => (general_decision Item &item_order x y).no edge;
item_decide := \x:Item => \y:Item =>
	item_decision x y (item_le x y) (bool_decide (item_key x) (item_key y));
item_trans := \x:Item => \y:Item => \p:item_order x y => \z:Item => \q:item_order y z =>
	bool_trans (item_key x) (item_key y) p (item_key z) q;
quick_items := quick_backend Item &item_order &item_le &item_decide;
insertion_items := insertion_backend Item &item_order &item_le &item_decide;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
high := Item.mk Bool.true zero;
low_one := Item.mk Bool.false one;
low_two := Item.mk Bool.false two;
empty := (List Item).nil;
singleton := (List Item).cons high empty;
reversed := (List Item).cons high ((List Item).cons low_one empty);
ordered := (List Item).cons low_one singleton;
duplicates := (List Item).cons high ((List Item).cons low_one ((List Item).cons low_two empty));
quick_expected := (List Item).cons low_two ((List Item).cons low_one singleton);
insertion_expected := (List Item).cons low_one ((List Item).cons low_two singleton);

// Check open-input proofs independently; output observation need not execute and discard them.
consumer_local := \backend:sorting_backend Item &item_order => \xs:List Item =>
	sorting_local Item &item_order backend xs;
consumer_content := \backend:sorting_backend Item &item_order => \xs:List Item =>
	sorting_content Item &item_order backend xs;
consumer_strong := \backend:sorting_backend Item &item_order => \xs:List Item =>
	sorting_strong Item &item_order &item_trans backend xs;
checked_list := \backend:sorting_backend Item &item_order => \xs:List Item =>
	sorting_run Item &item_order backend xs;
checked_vector := \backend:sorting_backend Item &item_order => \n:Nat => \v:Vec Item n =>
	sorting_vector Item &item_order backend n v;
quick_empty := checked_list quick_items empty;
quick_singleton := checked_list quick_items singleton;
quick_reversed := checked_list quick_items reversed;
quick_ordered := checked_list quick_items ordered;
quick_duplicates := checked_list quick_items duplicates;
insertion_empty := checked_list insertion_items empty;
insertion_singleton := checked_list insertion_items singleton;
insertion_reversed := checked_list insertion_items reversed;
insertion_ordered := checked_list insertion_items ordered;
insertion_duplicates := checked_list insertion_items duplicates;
quick_direct := quickSort Item &item_le duplicates;
insertion_direct := insertionSortBy Item &item_le duplicates;

vector_input := (Vec Item).cons high ((Vec Item).cons low_one ((Vec Item).cons low_two (Vec Item).nil));
quick_vector := vec_contents Item three (checked_vector quick_items three vector_input);
insertion_vector := vec_contents Item three (checked_vector insertion_items three vector_input);
input_size := list_sized Item duplicates;
first := Fin.zero two;
second := Fin.succ (Fin.zero one);
third := Fin.succ (Fin.succ (Fin.zero zero));
quick_positions := sorting_positions Item &item_order quick_items three duplicates input_size;
insertion_positions := sorting_positions Item &item_order insertion_items three duplicates input_size;
origin_label := \positions:position_permutation three => \i:Fin three =>
	item_label (sized_lookup Item three duplicates input_size (position_forward three positions i));
quick_first := origin_label quick_positions first;
quick_second := origin_label quick_positions second;
quick_third := origin_label quick_positions third;
insertion_first := origin_label insertion_positions first;
insertion_second := origin_label insertion_positions second;
insertion_third := origin_label insertion_positions third;

same_label := @\left:Nat => @\right:Nat => { refl:(n:Nat)->* n n; };
identity_value_law := position_action_identity Item three &(sized_lookup Item three duplicates input_size) first
	&(\v:Item => same_label zero (item_label v)) (same_label.refl zero);
composition_value_law := position_action_compose Item three (position_flip three) (position_flip three)
	&(sized_lookup Item three duplicates input_size) first
	&(\v:Item => same_label zero (item_label v)) (same_label.refl zero);
main := insertion_vector;

// Aggregate observations so image checks do not recompile for every element.
Report := @{ mk:List (List Item)->List Nat->*; };
outputs := \a:List Item => \b:List Item => \c:List Item => \d:List Item =>
	\e:List Item => \f:List Item => \g:List Item =>
	(List (List Item)).cons a ((List (List Item)).cons b ((List (List Item)).cons c
		((List (List Item)).cons d ((List (List Item)).cons e ((List (List Item)).cons f
			((List (List Item)).cons g (List (List Item)).nil))))));
labels := \a:Nat => \b:Nat => \c:Nat =>
	(List Nat).cons a ((List Nat).cons b ((List Nat).cons c (List Nat).nil));
quick_report := Report.mk
	(outputs quick_empty quick_singleton quick_reversed quick_ordered quick_duplicates quick_direct quick_vector)
	(labels quick_first quick_second quick_third);
quick_report_expected := Report.mk
	(outputs empty singleton ordered ordered quick_expected quick_expected quick_expected) (labels two one zero);
insertion_report := Report.mk
	(outputs insertion_empty insertion_singleton insertion_reversed insertion_ordered insertion_duplicates insertion_direct insertion_vector)
	(labels insertion_first insertion_second insertion_third);
insertion_report_expected := Report.mk
	(outputs empty singleton ordered ordered insertion_expected insertion_expected insertion_expected) (labels one two zero);
wrong_quick_report := Report.mk
	(outputs empty singleton ordered ordered quick_expected quick_expected quick_expected) (labels one two zero);
quick_list_report := outputs quick_empty quick_singleton quick_reversed quick_ordered quick_duplicates quick_direct quick_duplicates;
quick_list_expected := outputs empty singleton ordered ordered quick_expected quick_expected quick_expected;
insertion_list_report := outputs insertion_empty insertion_singleton insertion_reversed insertion_ordered insertion_duplicates insertion_direct insertion_duplicates;
insertion_list_expected := outputs empty singleton ordered ordered insertion_expected insertion_expected insertion_expected;
