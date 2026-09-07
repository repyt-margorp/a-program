Bool := @{
	true : *;
	false : *;
};

identity := \x : Bool => x;
matchExpected := { Bool.false; };
uniformBetaMatch := Bool.true
	@true => (identity Bool.false)
	@false => (identity Bool.false);
