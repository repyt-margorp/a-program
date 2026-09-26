graphHead := \input : NatList => \output : Nat =>
	\graph : @length input output =>
	graph
		@nil => Nat.zero
		@cons { head; } => head;

certified := *length NatList.nil;
main := length NatList.nil;
