Bool := @{false:*; true:*;};
Nat := @{zero:*; succ:*->*;};
Carrier := \b:Bool => b @false => Nat @true => Bool;
extract := \b:Bool => \v:Carrier b => {
	alias := v;
	result := b @false => alias @true => Nat.zero;
}.result;
zero := Nat.zero;
one := Nat.succ zero;
main := extract Bool.false one;
base := extract Bool.true Bool.true;
handled := \b:Bool => \v:Carrier b =>
	v @#.return x => (b @false => x @true => Nat.zero);
handledMain := handled Bool.false one;
handledBase := handled Bool.true Bool.true;
