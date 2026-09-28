// Directional decisions alone do not make this BubbleSort locally sorted.
bubble_point := @{ a:*; b:*; c:*; };
bubble_relation := @\x:bubble_point => @\y:bubble_point => {
	aa:* bubble_point.a bubble_point.a;
	bb:* bubble_point.b bubble_point.b;
	cc:* bubble_point.c bubble_point.c;
	ab:* bubble_point.a bubble_point.b;
	bc:* bubble_point.b bubble_point.c;
	cb:* bubble_point.c bubble_point.b;
	ca:* bubble_point.c bubble_point.a;
};
bubble_counter_le := \x:bubble_point => x
	@a => (\y:bubble_point => y @a => Bool.true @b => Bool.true @c => Bool.false)
	@b => (\y:bubble_point => y @a => Bool.false @b => Bool.true @c => Bool.false)
	@c => (\y:bubble_point => y @a => Bool.true @b => Bool.false @c => Bool.true);
bubble_counter_decide := \x:bubble_point => x
	@(self => (y:bubble_point)->general_decision bubble_point bubble_relation self y (bubble_counter_le self y))
	@a => (\y:bubble_point => y @(self => general_decision bubble_point bubble_relation bubble_point.a self (bubble_counter_le bubble_point.a self))
		@a => (general_decision bubble_point bubble_relation bubble_point.a bubble_point.a).yes bubble_relation.aa
		@b => (general_decision bubble_point bubble_relation bubble_point.a bubble_point.b).yes bubble_relation.ab
		@c => (general_decision bubble_point bubble_relation bubble_point.a bubble_point.c).no bubble_relation.ca)
	@b => (\y:bubble_point => y @(self => general_decision bubble_point bubble_relation bubble_point.b self (bubble_counter_le bubble_point.b self))
		@a => (general_decision bubble_point bubble_relation bubble_point.b bubble_point.a).no bubble_relation.ab
		@b => (general_decision bubble_point bubble_relation bubble_point.b bubble_point.b).yes bubble_relation.bb
		@c => (general_decision bubble_point bubble_relation bubble_point.b bubble_point.c).no bubble_relation.cb)
	@c => (\y:bubble_point => y @(self => general_decision bubble_point bubble_relation bubble_point.c self (bubble_counter_le bubble_point.c self))
		@a => (general_decision bubble_point bubble_relation bubble_point.c bubble_point.a).yes bubble_relation.ca
		@b => (general_decision bubble_point bubble_relation bubble_point.c bubble_point.b).no bubble_relation.bc
		@c => (general_decision bubble_point bubble_relation bubble_point.c bubble_point.c).yes bubble_relation.cc);
bubble_counter_decide :: (x:bubble_point)->(y:bubble_point)->general_decision bubble_point bubble_relation x y (bubble_counter_le x y);

bubble_unit := @{ unit:*; };
bubble_void := @{};
bubble_edge_type := \x:bubble_point => x
	@a => (\y:bubble_point => y @a => bubble_unit @b => bubble_unit @c => bubble_void)
	@b => (\y:bubble_point => y @a => bubble_void @b => bubble_unit @c => bubble_unit)
	@c => (\y:bubble_point => y @a => bubble_unit @b => bubble_unit @c => bubble_unit);
bubble_edge_evidence := \x:bubble_point => \y:bubble_point => \p:bubble_relation x y => p
	@(left right self => bubble_edge_type left right)
	@aa => bubble_unit.unit @bb => bubble_unit.unit @cc => bubble_unit.unit
	@ab => bubble_unit.unit @bc => bubble_unit.unit @cb => bubble_unit.unit @ca => bubble_unit.unit;
bubble_first_edge := \xs:List bubble_point => xs @(self => @)
	@nil => bubble_unit
	@cons x tail => (tail @(self => @) @nil => bubble_unit @cons y rest => bubble_edge_type x y);
bubble_local_edge := \xs:List bubble_point => \p:general_locally_sorted bubble_point bubble_relation xs => p
	@(values self => bubble_first_edge values)
	@nil => bubble_unit.unit
	@one x => bubble_unit.unit
	@cons x y tail edge rest => bubble_edge_evidence x y edge;

bubble_counter_empty := (List bubble_point).nil;
bubble_counter_input := (List bubble_point).cons bubble_point.a
	((List bubble_point).cons bubble_point.c ((List bubble_point).cons bubble_point.b bubble_counter_empty));
bubble_counter_result := bubble_sort bubble_point &bubble_counter_le bubble_counter_input;
bubble_counter_content := bubble_content bubble_point &bubble_counter_le bubble_counter_input;
bubble_counter_content :: permutation bubble_point bubble_counter_input bubble_counter_result;
bubble_counter_refute := \p:general_locally_sorted bubble_point bubble_relation bubble_counter_result =>
	bubble_local_edge bubble_counter_result p;
bubble_counter_refute :: general_locally_sorted bubble_point bubble_relation bubble_counter_result -> bubble_void;
