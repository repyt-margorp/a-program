Flag := @{off : *; on : *;};
Flags := @{nil : *; cons : Flag -> * -> *;};
Signal := @{red : *; amber : *; green : *;};
Signals := @{more : * -> Signal -> *; end : *;};
Pairs := @{nil : *; cons : #Int32 -> Flag -> * -> *;};
Value := @{value : #Int32 -> *;};
Aggregates := @{nil : *; cons : Value -> * -> *;};
Tree := @{leaf : *; fork : * -> * -> *;};
length := \xs : Flags => xs @nil => #0 @cons b tail => #int_add #1 *tail;
count_on := \xs : Flags => xs @nil => #0
	@cons b tail => #int_add (b @off => #0 @on => #1) *tail;
select := \xs : Flags => \wanted : Flag => xs @nil => Flags.nil
	@cons b tail => (b
		@off => (wanted @off => Flags.cons b *tail @on => *tail)
		@on => (wanted @off => *tail @on => Flags.cons b *tail));
append := \xs : Flags => \ys : Flags => xs @nil => ys
	@cons b tail => Flags.cons b *tail;
flip := \xs : Flags => xs @nil => Flags.nil
	@cons b tail => (b @off => Flags.cons Flag.on *tail @on => Flags.cons Flag.off *tail);
identity := \xs : Flags => xs;
cons := \b : Flag => \xs : Flags => Flags.cons b xs;
constant := Flags.nil;
signal_length := \xs : Signals => xs @more tail s => #int_add #1 *tail @end => #0;
signal_score := \xs : Signals => xs
	@more tail s => #int_add (s @red => #1 @amber => #10 @green => #100) *tail @end => #0;
signal_append := \xs : Signals => \ys : Signals => xs
	@more tail s => Signals.more *tail s @end => ys;
signal_identity := \xs : Signals => xs;
callback := \f : #Int32 -> #Int32 => \xs : Flags => f (length xs);
effect := \xs : Flags => { #print #"unsupported-effect"; length xs; };
