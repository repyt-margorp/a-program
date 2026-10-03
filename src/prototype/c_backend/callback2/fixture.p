apply32 := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => \y : #Int32 => f x y;
repeat32 := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => \y : #Int32 => f (f x y) y;
captured32 := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => \y : #Int32 => {
	local := \z : #Int32 => f z y;
	local x;
};
mixed_calls32 := \f : #Int32 -> #Int32 -> #Int32 => \g : #Int32 -> #Int32 => \x : #Int32 => \y : #Int32 => g (f x y);
unused32 := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => \y : #Int32 => #int_sub x y;
apply64 := \f : #Int64 -> #Int64 -> #Int64 => \x : #Int64 => \y : #Int64 => f x y;
repeat64 := \f : #Int64 -> #Int64 -> #Int64 => \x : #Int64 => \y : #Int64 => f (f x y) y;
captured64 := \f : #Int64 -> #Int64 -> #Int64 => \x : #Int64 => \y : #Int64 => {
	local := \z : #Int64 => f z y;
	local x;
};
ternary := \f : #Int32 -> #Int32 -> #Int32 -> #Int32 => \x : #Int32 => f x x x;
mixed_width := \f : #Int32 -> #Int64 -> #Int64 => \x : #Int32 => \y : #Int64 => f x y;
mixed_result := \f : #Int32 -> #Int32 -> #Int64 => \x : #Int32 => f x x;
returned := \f : #Int32 -> #Int32 -> #Int32 => f;
effect := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => { #print #"effect"; f x x; };
subtract := \x : #Int32 => \y : #Int32 => #int_sub x y;
negate := \x : #Int32 => #int_neg x;
observe := \x : #Int32 => \y : #Int32 => {
	#print (#int_to_text (apply32 &subtract x y)); #print #"|";
	#print (#int_to_text (repeat32 &subtract x y)); #print #"|";
	#print (#int_to_text (captured32 &subtract x y)); #print #"|";
	#print (#int_to_text (mixed_calls32 &subtract &negate x y)); #print #"|";
	#print (#int_to_text (unused32 &subtract x y)); #print #"|";
};
reference := { observe #0 #7; observe #17 #-7; observe #-2147483648 #1; observe #2147483647 #-1; };
