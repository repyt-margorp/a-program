Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

choose := \b : Bool =>
	b @true => Nat.zero
	  @false => Bool.true;

main := choose Bool.true;
