Returns := \A : @ => @\computation : &A => @\value : A => {
	assumed : (computation : &A) -> (value : A) -> * computation value;
};

Bool := @{
	true : *;
	false : *;
};
