Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
select := \b:Bool => b @(self => Nat->Nat->Nat)
	@false => (\x:Nat => \y:Nat => x)
	@true => (\x:Nat => \y:Nat => y);
select :: Bool->Nat->Nat->Nat;
Family := @\n:Nat => {mark:(value:Nat)->* value;};
proof := \b:Bool => \x:Nat => \y:Nat => b @(self => Family (select self x y))
	@false => Family.mark x
	@true => Family.mark y;
proof :: (b:Bool)->(x:Nat)->(y:Nat)->Family (select b x y);
one := Nat.succ Nat.zero;
main := select Bool.true Nat.zero one;
expected := one;
