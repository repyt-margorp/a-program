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
	b @true => false
	  @false => true;

main := toNat (negate false);
