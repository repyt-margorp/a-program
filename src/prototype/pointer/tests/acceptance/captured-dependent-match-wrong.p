Bool := @{false:*; true:*;};
Nat := @{zero:*; succ:*->*;};
Precedes := @\left:Bool => @\right:Bool => {
	falseBeforeTrue : * Bool.false Bool.true;
};

bad := \b:Bool => \ih:(y:Bool)->Precedes y b->Nat => b
	@false => ih Bool.false Precedes.falseBeforeTrue
	@true => Nat.zero;
