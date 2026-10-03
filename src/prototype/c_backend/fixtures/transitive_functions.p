chain_three := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	h x;
};
chain_four := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	i := \y : #Int32 => h y;
	i x;
};
chain_eight := \offset : #Int32 => \x : #Int32 => {
	a := \y : #Int32 => #int_add y offset;
	b := \y : #Int32 => a y;
	c := \y : #Int32 => b y;
	d := \y : #Int32 => c y;
	e := \y : #Int32 => d y;
	f := \y : #Int32 => e y;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	h x;
};
shadowed := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	(\offset : #Int32 => h offset) x;
};
repeated := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => #int_add y offset;
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	#int_add (h x) (h x);
};
unused_effect := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => { #print #"unreached"; #int_add y offset; };
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	x;
};
chain64 := \offset : #Int64 => \x : #Int64 => {
	f := \y : #Int64 => #int64_add y offset;
	g := \y : #Int64 => f y;
	h := \y : #Int64 => g y;
	h x;
};
dynamic_callback := \f : #Int32 -> #Int32 => \x : #Int32 => f x;
demanded_effect := \offset : #Int32 => \x : #Int32 => {
	f := \y : #Int32 => { #print #"demanded"; #int_add y offset; };
	g := \y : #Int32 => f y;
	h := \y : #Int32 => g y;
	h x;
};
