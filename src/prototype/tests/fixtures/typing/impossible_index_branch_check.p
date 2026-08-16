Nat := @{
	zero : *;
	succ : * -> *;
};

OnlyZero := @\index : Nat => {
	zero : * Nat.zero;
};

absurd := \index : Nat => \x : OnlyZero (Nat.succ index) =>
	x @zero => Nat.zero;

absurd :: (index : Nat) -> OnlyZero (Nat.succ index) -> Nat;
