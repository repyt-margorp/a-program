Bool := @{ true : *; false : *; };
PairText := @{ mk : #.Text -> #.Text -> *; };

second := \x : #.Text => \y : #.Text => y;

leftToRight := second
	(perform (#.print #"a"))
	(perform (#.print #"b"));

repeat := second
	(perform (#.print #"r"))
	(perform (#.print #"r"));

shared := {
	x := perform (#.print #"s");
	second x x
};

constructorOrder := PairText.mk
	(perform (#.print #"c"))
	(perform (#.print #"d"));

performArgument := perform (#.print (perform (#.print #"e")));

effectBool := {
	x : #.Text := perform (#.print #"m");
	Bool.false
};

matchScrutineeEffect := effectBool
	@true  => #"unreachable"
	@false => #"matched";
