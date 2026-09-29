Bool := @{false : *; true : *;};
Colour := @{red : *; green : *; blue : *;};
Twin := @{first : *; second : *;};
Empty := @{};
Box := @{box : #Int32 -> *;};
Family := \A : @ => @{unit : *;};
number := #42;
negate := \b : Bool => b @false => Bool.true @true => Bool.false;
to_int := \b : Bool => b @false => #10 @true => #20;
choose := \b : Bool => \x : #Int32 => \y : #Int32 =>
	b @false => #int_add x #1 @true => #int_mul y #2;
colour := \c : Colour => c @red => #1 @green => #2 @blue => #3;
to_colour := \b : Bool => b @false => Colour.red @true => Colour.blue;
twin := \x : Twin => x @first => Twin.second @second => Twin.first;
same := \x : Bool => x;
nested := \b : Bool => choose (negate b) #7 #11;
curried := \b : Bool => b @false => (\x : #Int32 => #int_add x #1)
	@true => (\x : #Int32 => #int_mul x #2);
through_fold := { k := #3; \b : Bool => curried b k; };
fold_arg := \b : Bool => { k := to_int b; choose b k #3; };
constant := Bool.true;
effect := \b : Bool => b @false => { #print #"not-pure"; #1; } @true => #2;
boxed := \x : Box => x @box n => n;
