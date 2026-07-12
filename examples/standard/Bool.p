Bool := @{
	true : *;
	false : *;
};

not :: Bool -> Bool;
not := \b : Bool =>
	b @true		=> Bool.false
	  @false	=> Bool.true;

and :: Bool -> Bool -> Bool;
and := \a : Bool =>
	a @true		=> (\b : Bool => b)
	  @false	=> (\b : Bool => Bool.false);

or :: Bool -> Bool -> Bool;
or := \a : Bool =>
	a @true		=> (\b : Bool => Bool.true)
	  @false	=> (\b : Bool => b);

xor :: Bool -> Bool -> Bool;
xor := \a : Bool =>
	a @true		=> (\b : Bool => not b)
	  @false	=> (\b : Bool => b);

implies :: Bool -> Bool -> Bool;
implies := \a : Bool =>
	a @true		=> (\b : Bool => b)
	  @false	=> (\b : Bool => Bool.true);

ifBool :: Bool -> Bool -> Bool -> Bool;
ifBool := \condition : Bool =>
	condition @true		=> (\thenValue : Bool => \elseValue : Bool => thenValue)
	          @false	=> (\thenValue : Bool => \elseValue : Bool => elseValue);
