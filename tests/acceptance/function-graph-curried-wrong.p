Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
append := \A:@ => \left:List A => left
	@nil => (\right:List A => right)
	@cons head tail => (\right:List A => (List A).cons head (*tail right));
Graph := @append Nat;
nil := (List Nat).nil;
right := (List Nat).cons Nat.zero nil;
// The recursive proof returns right, not nil, for these call arguments.
wrong := Graph.cons Nat.zero nil right nil (Graph.nil right);
