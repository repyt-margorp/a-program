{{
	Bool := @{
		true : *;
		false : *;
	};

	id1 := \x : Bool => x;
	id2 := \y : Bool => y;
	use1 := id1 Bool.true;
	main := id2 Bool.false;
}}.main
