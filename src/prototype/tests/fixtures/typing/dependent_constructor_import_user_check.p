import Sigma;

Nat := @{
	zero : *;
	succ : * -> *;
};

ConstNat := \x : Nat => Nat;
main := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
