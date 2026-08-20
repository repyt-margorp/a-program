Bool := @{
	true : *;
	false : *;
};

choose := \decision : Bool =>
	decision
		@true => Bool.true
		@false => Bool.false;

openResultEvidence := \decision : Bool => {
	result := choose decision;
	evidence := #.returns (&(choose decision)) result;
	evidence;
};
