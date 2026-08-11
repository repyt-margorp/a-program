Expr := @{
	literal : #.Int -> *;
	add : * -> * -> *;
	negate : * -> *;
};

evaluate := \expression : Expr =>
	expression @literal value => value
		@add left right => #.int_add *left *right
		@negate operand => #.int_neg *operand;
evaluate :: Expr -> #.Int;

three := Expr.literal #3;
five := Expr.literal #5;
inner := Expr.add three five;
sample := Expr.add (Expr.literal #50) (Expr.negate inner);
main := evaluate sample;
expected := { #42; };

Int32Box := @{ make : #.Int32 -> *; };
int32Box := Int32Box.make #42;

Int64Box := @{ make : #.Int64 -> *; };
boxInt64 := \value : #.Int64 => Int64Box.make value;
boxInt64 :: #.Int64 -> Int64Box;
