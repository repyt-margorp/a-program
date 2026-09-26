Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> A -> * k -> * (Nat.succ k);
};
tail := \A:@ => \n:Nat => \v:Vec A (Nat.succ n) =>
	v @cons k x rest => rest;
tail :: (A:@) -> (n:Nat) -> Vec A (Nat.succ n) -> Vec A (Nat.succ n);
