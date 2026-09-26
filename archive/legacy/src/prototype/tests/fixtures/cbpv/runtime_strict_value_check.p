Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

negate := \b : Bool =>
	b @true  => Bool.false
	  @false => Bool.true;

toNat := \b : Bool =>
	b @true  => Nat.succ Nat.zero
	  @false => Nat.zero;

identityNat := \n : Nat => n;

identityBool := \b : Bool => b;

appArgument := toNat (negate Bool.false);

constructorArgument := Nat.succ (identityNat Nat.zero);

matchScrutinee := (identityBool Bool.false)
	@true  => Nat.succ Nat.zero
	@false => Nat.zero;
