Nat := @{zero:*; succ:*->*;};
Box := @\A:@ => {
	box : (B:@) -> B -> * B;
	other : (B:@) -> B -> * B;
};
get := \A:@ => \v:Box A =>
	v @box B value => value
	  @other B value => Nat.zero;
get :: (A:@) -> Box A -> A;
