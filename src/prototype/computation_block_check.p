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

identity := \n : Nat => n;

main := {
	b : Bool := negate Bool.false;
	n := toNat b;
	alias := n;
	id := identity;
	id alias
};

quotedIdentity := &identity;

constructorBlock := {
	n : Nat := identity Nat.zero;
	Nat.succ n
};

matchBlock := Nat.zero
	@zero => {
		n : Nat := identity Nat.zero;
		Nat.succ n
	}
	@succ predecessor => Nat.succ Nat.zero;
