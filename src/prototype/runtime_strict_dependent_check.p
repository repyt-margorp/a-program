Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := { x : #.Text := perform (#.print #"x"); Bool.true; };
select := \b : Bool =>
	b @true => Nat.zero @false => Bool.true;
main := select m;
