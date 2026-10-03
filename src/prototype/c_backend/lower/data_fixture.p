Bool := @{false : *; true : *;};
Packet := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> #Int32 -> *;};
Twin := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> #Int32 -> *;};
Recursive := @{nil : *; cons : #Int32 -> * -> *;};
Dependent := @{pack : (A : @) -> A -> *;};
Nested := @{nested : Packet -> *;};
Callback := @{callback : (#Int32 -> #Int32) -> *;};

small := \n : #Int32 => \b : Bool => Packet.small n b;
wide := \n : #Int64 => \m : #Int32 => Packet.wide n m;
number := \p : Packet => p @empty => #0 @small n b =>
	(b @false => n @true => #int_add n #1)
	@wide n m => m;
large := \p : Packet => \fallback : #Int64 => p @empty => fallback
	@small n b => fallback @wide n m => n;
identity := \p : Packet => p;
rebuild := \p : Packet => p @empty => Packet.empty
	@small n b => Packet.small (#int_add n #1) b
	@wide n m => Packet.wide (#int64_neg n) (#int_neg m);
captured := \p : Packet => \b : Bool => b
	@false => (\x : #Int32 => #int_add (number p) x) #3
	@true => (\x : #Int32 => #int_add (number p) x) #9;
partial := \p : Packet => p @empty => (\x : #Int32 => #int_add x #2)
	@small n b => (\x : #Int32 => #int_add n x)
	@wide n m => (\x : #Int32 => #int_add m x);
shadowed := \p : Packet => { p := rebuild p; captured p Bool.true; };
other := \p : Twin => p @empty => #0 @small n b => n @wide n m => m;
constant := Packet.empty;
effect := \p : Packet => p @empty => #0
	@small n b => { #print #"not-pure"; n; } @wide n m => m;
block_callback := \p : Packet => \b : Bool => {
	f := \x : #Int32 => #int_add (number p) x;
	b @false => f #3 @true => f #9;
};
callback := \f : #Int32 -> #Int32 => \n : #Int32 => f n;
recursive := \xs : Recursive => xs @nil => #0
	@cons n tail => #int_add n *tail;
