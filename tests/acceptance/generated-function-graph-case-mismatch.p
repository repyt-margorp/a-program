Nat := @{zero : *; succ : * -> *;};
copy := \n : Nat => n @zero => Nat.zero @succ k => Nat.succ *k;
bad := \input : Nat => \output : Nat => \graph : @copy input output =>
	graph @Nat.zero => Nat.zero @Nat.succ k output proof => Nat.zero;
