Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

Result := \b : Bool =>
	b
		@true => Nat
		@false => Bool;

choose := \b : Bool =>
	b
		@true => Nat.zero
		@false => Bool.true;

choose :: (q : Bool) -> Result q;

main := choose Bool.true;
