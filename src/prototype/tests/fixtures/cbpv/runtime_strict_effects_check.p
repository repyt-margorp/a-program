Bool := @{ true : *; false : *; };
PairText := @{ mk : #.Text -> #.Text -> *; };

second := \x : #.Text => \y : #.Text => y;

leftToRight := second
	((#.print #"a"))
	((#.print #"b"));

repeat := second
	((#.print #"r"))
	((#.print #"r"));

shared := {
	x := (#.print #"s");
	second x x;
};

constructorOrder := PairText.mk
	((#.print #"c"))
	((#.print #"d"));

performArgument := (#.print ((#.print #"e")));

effectBool := {
	x : #.Text := (#.print #"m");
	Bool.false;
};

matchScrutineeEffect := effectBool
	@true  => #"unreachable"
	@false => #"matched";
