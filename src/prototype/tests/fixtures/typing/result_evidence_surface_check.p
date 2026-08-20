value := #42;

computation := &{
	result := #42;
};

proof := #.returns (computation) value;
proof :: #.Returns (computation) value;

termination := #.terminates (computation) proof;
termination :: #.Terminates (computation);
