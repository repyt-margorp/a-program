Nat := @{ zero : *; succ : * -> *; };

main := {
	n := { Nat.zero };
	n @zero => Nat.zero @succ k => Nat.succ k
};
