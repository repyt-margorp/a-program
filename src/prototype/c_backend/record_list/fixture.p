Bool := @{false : *; true : *;};
Packet := @{empty : *; small : #Int32 -> Bool -> *; wide : #Int64 -> #Int32 -> *;};
Envelope := @{none : *; pair : Packet -> #Int32 -> *;};
Records := @{nil : *; cons : Envelope -> * -> *;};
Reverse := @{more : * -> Envelope -> *; end : *;};
Chain := @{nil : *; cons : #Int32 -> * -> *;};
ChainBox := @{box : Chain -> *;};
Aggregate := @{nil : *; cons : ChainBox -> * -> *;};
DirectAggregate := @{nil : *; cons : Chain -> * -> *;};
MultiPayload := @{nil : *; cons : Envelope -> #Int32 -> * -> *;};
Tree := @{leaf : *; fork : * -> * -> *;};
Callback := @{callback : (#Int32 -> #Int32) -> *;};
Dependent := @{pack : (A : @) -> A -> *;};
Nat := @{zero : *; succ : * -> *;};
Indexed := @\size : Nat => {nil : * Nat.zero; cons : (n : Nat) -> Envelope -> * n -> * (Nat.succ n);};
number := \p : Packet => p @empty => #0
	@small n b => (b @false => n @true => #int_add n #1)
	@wide n m => m;
measure := \e : Envelope => e @none => #0 @pair p n => #int_add (number p) n;
empty := Records.nil;
prepend := \e : Envelope => \xs : Records => Records.cons e xs;
identity := \xs : Records => xs;
length := \xs : Records => xs @nil => #0 @cons e tail => #int_add #1 *tail;
sum := \xs : Records => xs @nil => #0 @cons e tail => #int_add (measure e) *tail;
append := \xs : Records => xs
	@nil => (\ys : Records => ys)
	@cons e tail => (\ys : Records => Records.cons e (*tail ys));
reverse_empty := Reverse.end;
reverse_prepend := \e : Envelope => \xs : Reverse => Reverse.more xs e;
reverse_sum := \xs : Reverse => xs @end => #0 @more tail e => #int_add *tail (measure e);
sample := Records.cons (Envelope.pair (Packet.small #7 Bool.true) #3)
	(Records.cons Envelope.none (Records.cons (Envelope.pair Packet.empty #5) Records.nil));
sample_length := length sample;
sample_sum := sum sample;
sample_append := sum (append sample sample);
