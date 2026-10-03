closed_block := \x : #Int32 => { f := \y : #Int32 => #int_add y #1; f x; };
captured_block := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	f x;
};
shadowed_block := \x : #Int32 => {
	f := \y : #Int32 => #int_add y x;
	(\x : #Int32 => f x) #7;
};
repeated_block := \x : #Int32 => {
	f := \y : #Int32 => #int_add y #1;
	#int_add (f x) (f x);
};
curried_block := \x : #Int32 => {
	f := \left : #Int32 => \right : #Int32 => #int_add left right;
	f #7 x;
};
unused_block := \x : #Int32 => { f := \y : #Int32 => #int_add y #1; x; };
captured64 := \offset : #Int64 => \x : #Int64 => { f := \y : #Int64 => #int64_add y offset; f x; };
nested_two := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	g x;
};
nested_three := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	h x;
};
unused_effect := \x : #Int32 => { f := \y : #Int32 => { #print #"unreached-effect"; y; }; x; };
dynamic_callback := \f : #Int32 -> #Int32 => \x : #Int32 => f x;
effect_block := \x : #Int32 => { f := \y : #Int32 => { #print #"demanded-effect"; y; }; f x; };
