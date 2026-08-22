value := #42;

computation := &{
	result := #42;
};

termination := #.terminates (computation);
termination :: #.Terminates (computation);

effectful := &{
	#.print #"total-effect";
	#42;
};

effectfulTermination := #.terminates (effectful);
effectfulTermination :: #.Terminates (effectful);
