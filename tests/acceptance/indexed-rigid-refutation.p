Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := \A:@ => @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> A -> * k -> * (Nat.succ k);
};
head := \A:@ => \n:Nat => \v:Vec A (Nat.succ n) =>
	v @nil => Bool.false
	  @cons k x rest => x;
NatVec := Vec Nat;
main := head Nat Nat.zero (NatVec.cons Nat.zero (Nat.succ Nat.zero) NatVec.nil);
expected := Nat.succ Nat.zero;
BoolVec := Vec Bool;
boolMain := head Bool Nat.zero (BoolVec.cons Nat.zero Bool.true BoolVec.nil);
boolExpected := Bool.true;
empty := \A:@ => \v:Vec A Nat.zero =>
	v @nil => Nat.zero
	  @cons k x rest => Bool.false;
emptyMain := empty Nat NatVec.nil;
emptyExpected := Nat.zero;
select := \A:@ => \n:Nat => \v:Vec A (Nat.succ n) =>
	v @nil => Bool.false
	  @cons k x rest => \ignored:Bool => x;
functionMain := select Nat Nat.zero (NatVec.cons Nat.zero expected NatVec.nil) Bool.false;
Grid := @\n:Nat => @\m:Nat => {
	low : (k:Nat) -> * k Nat.zero;
	high : (k:Nat) -> (j:Nat) -> * k (Nat.succ j);
};
readGrid := \n:Nat => \m:Nat => \g:Grid n (Nat.succ m) =>
	g @low k => Bool.false
	  @high k j => Nat.succ j;
secondIndexMain := readGrid expected Nat.zero (Grid.high expected Nat.zero);
