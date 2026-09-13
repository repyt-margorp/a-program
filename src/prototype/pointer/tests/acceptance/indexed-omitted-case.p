Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
Vec := \A:@ => @\n:Nat => {
	nil : * Nat.zero;
	cons : (k:Nat) -> A -> * k -> * (Nat.succ k);
};
head := \A:@ => \n:Nat => \v:Vec A (Nat.succ n) =>
	v @cons k x rest => x;
empty := \A:@ => \v:Vec A Nat.zero => v @nil => Bool.true;
select := \A:@ => \n:Nat => \v:Vec A (Nat.succ n) =>
	v @cons k x rest => \ignored:Bool => x;
NatVec := Vec Nat;
input := NatVec.cons Nat.zero (Nat.succ Nat.zero) NatVec.nil;
main := head Nat Nat.zero input;
functionMain := select Nat Nat.zero input Bool.false;
expected := Nat.succ Nat.zero;
emptyMain := empty Nat NatVec.nil;
emptyExpected := Bool.true;
