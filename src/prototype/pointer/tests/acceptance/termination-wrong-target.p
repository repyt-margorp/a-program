Nat := @{zero:*; succ:*->*;};
left := &{Nat.zero;};
right := &{Nat.succ Nat.zero;};
proof := #.terminates left;
proof :: #.Terminates right;
