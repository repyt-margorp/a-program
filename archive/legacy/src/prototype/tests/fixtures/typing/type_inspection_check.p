Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

identityBool := \value : Bool => value;
identityBool :: Bool -> Bool;

identityNat := \value : Nat => value;
identityNat :: Nat -> Nat;

selected := Bool.true
	@true => Nat.zero
	@false => Nat.zero;

literal := #40;
literal :: #.Int;
