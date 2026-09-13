IntBox := @{ box : #Int -> *; };
extract := \x : IntBox => x @box value => value;
extract :: IntBox -> #Int32;
main := extract (IntBox.box #42);
expected := { #42; };
minimum := extract (IntBox.box #-2147483648);
minimumExpected := { #-2147483648; };
maximum := extract (IntBox.box #2147483647);
maximumExpected := { #2147483647; };
identity64 := \value : #Int64 => value;
identity64 :: #Int64 -> #Int64;

TextBox := @{ box : #Text -> *; };
text := \x : TextBox => x @box value => value;
hello := #"not the literal";
textMain := text (TextBox.box #"hello");
textExpected := { #"hello"; };
emptyMain := text (TextBox.box #"");
emptyExpected := { #""; };

alias := #Int;
main :: alias;
IntBoxAlias := #Int32;
compatibility := \value : #Text => value;
compatibility :: #Text -> #Text;
same := { #42; } @#return value => value;
