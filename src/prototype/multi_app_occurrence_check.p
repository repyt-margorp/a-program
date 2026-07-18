Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };

identityBool :: Bool -> Bool;
identityBool := \x : Bool => x;

identityNat :: Nat -> Nat;
identityNat := \x : Nat => x;

higherBool := \f : Bool -> Bool => f;
higherNat := \f : Nat -> Nat => f;

useHigherBool := higherBool &identityBool;
useHigherNat := higherNat &identityNat;

useAscribedBool := (identityBool :: Bool -> Bool) Bool.true;
useAscribedNat := (identityNat :: Nat -> Nat) Nat.zero;

matchAscribed := {
	b := (identityBool :: Bool -> Bool) Bool.true;
	b @true => (identityNat :: Nat -> Nat) Nat.zero
	  @false => (identityNat :: Nat -> Nat) (Nat.succ Nat.zero)
};
