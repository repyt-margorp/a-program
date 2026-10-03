Bool := @{false : *; true : *;};
Packet := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> #Int32 -> *;};
Envelope := @{none : *; pair : Packet -> #Int32 -> *;};
Outer := @{outer : Envelope -> *;};
Recursive := @{nil : *; cons : Envelope -> * -> *;};
Chain := @{nil : *; cons : #Int32 -> * -> *;};
ChainBox := @{box : Chain -> *;};
Callback := @{callback : (#Int32 -> #Int32) -> *;};
Dependent := @{pack : (A : @) -> A -> *;};
number := \p : Packet => p @empty => #0
	@small n b => (b @false => n @true => #int_add n #1)
	@wide n m => m;
wrap := \p : Packet => \n : #Int32 => Envelope.pair p n;
measure := \e : Envelope => e @none => #0 @pair p n => #int_add (number p) n;
unwrap := \e : Envelope => e @none => Packet.empty @pair p n => p;
identity := \e : Envelope => e;
rebuild := \e : Envelope => Envelope.pair (unwrap e) (measure e);
outer := \e : Envelope => Outer.outer e;
outer_number := \o : Outer => o @outer e => measure e;
captured := \e : Envelope => { f := \n : #Int32 => #int_add (measure e) n; f #7; };
constant := Envelope.none;
