Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };

m := {
	x : #.Text := perform (#.print #"x");
	Bool.true;
};

main := m
	@#.return b => (b @true => Nat.zero @false => Bool.true)
	@#.print x k => k x;
