Bool := @{false : *; true : *;};
List := @{nil : *; cons : #Int32 -> Bool -> * -> *;};
Tree := @{leaf : *; fork : * -> * -> *;};
ThunkList := @{nil : *; cons : (#Int32 -> *) -> *;};

length := \xs : List => xs @nil => #0 @cons n b tail => #int_add #1 *tail;
sum := \xs : List => xs @nil => #0 @cons n b tail => #int_add n *tail;
append := \xs : List => \ys : List => xs @nil => ys
	@cons n b tail => List.cons n b *tail;
select := \xs : List => \wanted : Bool => xs @nil => List.nil
	@cons n b tail => (b
		@false => (wanted @false => List.cons n b *tail @true => *tail)
		@true => (wanted @false => *tail @true => List.cons n b *tail));
identity := \xs : List => xs;
cons := \n : #Int32 => \b : Bool => \xs : List => List.cons n b xs;
composed := \xs : List => sum (append xs xs);
constant := List.nil;
block_callback := \xs : List => { f := \n : #Int32 => #int_add n #1; f (sum xs); };
callback := \f : #Int32 -> #Int32 => \xs : List => f (sum xs);
effect := \xs : List => { #print #"unsupported-effect"; sum xs; };
