Bool := @{
	true : *;
	false : *;
};

choose := \decision : Bool =>
	decision
		@true => Bool.true
		@false => Bool.false;

alwaysTrue := \ignored : Bool => Bool.true;

invalidEvidence := \decision : Bool => {
	result := choose decision;
	#.returns (&(alwaysTrue decision)) result;
};
