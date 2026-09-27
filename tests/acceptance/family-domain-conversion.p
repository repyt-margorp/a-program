// Arguments synthesize independently; application then checks conversion.
Nat := @{zero:*;succ:*->*;};
pred := \n:Nat => n @zero => Nat.zero @succ k => k;
Box := @\n:Nat => {mk:(k:Nat)->* k;};
Family := @\n:Nat => @\b:Box n => {mk:(k:Nat)->(b:Box k)->* k b;};
Reverse := @\n:Nat => @\b:Box (pred n) => {
	mk:(k:Nat)->(b:Box (pred k))->* k b;
};
shift := \k:Nat => \b:Box (pred k) => b;
ordinary := \k:Nat => \b:Box k => (\x:Box k=>x) (shift (Nat.succ k) b);
ordinary :: (k:Nat)->Box k->Box k;
family := \k:Nat => \b:Box k => Family k (shift (Nat.succ k) b);
reverse := \k:Nat => \b:Box k => Reverse (Nat.succ k) b;
higher := \F:(n:Nat)->Box n->@ => \k:Nat => \b:Box k =>
	F k (shift (Nat.succ k) b);
Holder := \F:Nat->@ => @\n:Nat => @\b:F n => {
	mk:(k:Nat)->(b:F k)->* k b;
};
selected := \k:Nat => \b:Box k => Holder Box k (shift (Nat.succ k) b);
zero := Nat.zero;
value := Box.mk zero;
main := family zero value;
expected := Family zero value;
back := reverse zero value;
back_expected := Reverse (Nat.succ zero) value;
variable := higher Family zero value;
selected_main := selected zero value;
selected_expected := Holder Box zero value;
