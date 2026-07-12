Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

toNat := \b : Bool =>
	b @true => Nat.succ Nat.zero
	  @false => Nat.zero;

negate := \b : Bool =>
	b @true => Bool.false
	  @false => Bool.true;

main := toNat (negate Bool.false);
