left := &{
	result := #42;
};

right := &{
	result := #43;
};

left_returns := #.returns (left) #42;
invalid_termination := #.terminates (right) left_returns;
