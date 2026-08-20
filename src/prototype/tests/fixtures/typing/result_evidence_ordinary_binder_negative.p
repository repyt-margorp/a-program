Bool := @{
	true : *;
	false : *;
};

invalidEvidence := \value : Bool =>
	#.returns (&{ Bool.true; }) value;
