Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };

m := {
	x : #.Text := perform (#.print #"x");
	Bool.true
};

main := handle (m) with (#.print) x k => k x; return b => b @true => Nat.zero @false => Bool.true;
