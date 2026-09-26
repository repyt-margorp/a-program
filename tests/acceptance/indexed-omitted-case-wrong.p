Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> A -> * k -> * (Nat.succ k);
};
head := \A:@ => \n:Nat => \v:Vec A n => v @cons k x rest => x;
