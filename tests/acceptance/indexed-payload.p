Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
Box := @\A:@ => {
	box : (B:@) -> B -> * B;
	other : (B:@) -> B -> * B;
};
get := \A:@ => \v:Box A =>
	v @box B value => value
	  @other B value => value;
get :: (A:@) -> Box A -> A;
one := Nat.succ Nat.zero;
main := get Nat (Box.box Nat one);
otherMain := get Nat (Box.other Nat one);
expected := one;

Dependent := @\A:@ => @\x:A => {
	at : (B:@) -> (y:B) -> * B y;
};
dependentGet := \A:@ => \x:A => \v:Dependent A x =>
	v @at B y => y;
dependentGet :: (A:@) -> (x:A) -> Dependent A x -> A;
dependentMain := dependentGet Nat one (Dependent.at Nat one);

mixed := \A:@ => \v:Box A =>
	v @box B value => value
	  @other B value => Bool.true;
mixedMain := mixed Nat (Box.box Nat one);
mixedOther := mixed Nat (Box.other Nat one);
boolExpected := Bool.true;

getFunction := \A:@ => \v:Box A =>
	v @box B value => (\ignored:B => value)
	  @other B value => (\ignored:B => value);
getFunction :: (A:@) -> Box A -> A -> A;
functionMain := getFunction Nat (Box.other Nat one) Nat.zero;
