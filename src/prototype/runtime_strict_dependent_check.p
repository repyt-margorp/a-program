Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := { x : #.Text := (#.print #"x"); Bool.true; };
select := \b : Bool =>
	b @true => Nat.zero @false => Bool.true;
main := select m;
