once32 := \f : #Int32 -> #Int32 => \x : #Int32 => f x;
twice32 := \f : #Int32 -> #Int32 => \x : #Int32 => f (f x);
compose32 := \f : #Int32 -> #Int32 => \g : #Int32 -> #Int32 => \x : #Int32 => f (g x);
captured32 := \f : #Int32 -> #Int32 => \x : #Int32 => {
	local := \y : #Int32 => #int_add (f y) x;
	local #7;
};
unused32 := \f : #Int32 -> #Int32 => \x : #Int32 => x;
once64 := \f : #Int64 -> #Int64 => \x : #Int64 => f x;
twice64 := \f : #Int64 -> #Int64 => \x : #Int64 => f (f x);
captured64 := \f : #Int64 -> #Int64 => \x : #Int64 => {
	local := \y : #Int64 => #int64_add (f y) x;
	local x;
};
mixed := \f : #Int32 -> #Int64 => \x : #Int32 => f x;
binary := \f : #Int32 -> #Int32 -> #Int32 => \x : #Int32 => f x x;
returned := \f : #Int32 -> #Int32 => f;
effect := \f : #Int32 -> #Int32 => \x : #Int32 => { #print #"effect"; f x; };
sample_add := \x : #Int32 => #int_add x #7;
sample_negative := \x : #Int32 => #int_neg x;
observe := \x : #Int32 => {
	#print (#int_to_text (once32 &sample_add x)); #print #"|";
	#print (#int_to_text (twice32 &sample_add x)); #print #"|";
	#print (#int_to_text (compose32 &sample_add &sample_negative x)); #print #"|";
	#print (#int_to_text (captured32 &sample_add x)); #print #"|";
	#print (#int_to_text (unused32 &sample_add x)); #print #"|";
};
reference := { observe #0; observe #17; observe #-2147483648; observe #2147483647; };
