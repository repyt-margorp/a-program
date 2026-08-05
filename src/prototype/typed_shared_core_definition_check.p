{{
	Bool := @{
		true : *;
		false : *;
	};

	Nat := @{
		zero : *;
		succ : * -> *;
	};

	identityBool := \x : Bool => x;
	identityNat := \x : Nat => x;
	identityBool :: Bool -> Bool;
	identityNat :: Nat -> Nat;
	boolResult := identityBool Bool.true;
	main := identityNat Nat.zero;
}}.main
