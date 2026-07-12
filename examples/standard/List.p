List := \A : @ => @{
	nil  : *;
	cons : A -> * -> *;
};

append := \A : @ =>
	\xs : List A =>
		xs @nil		=> (\ys : List A => ys)
		   @cons x rest	=> (\ys : List A => (List A).cons x (*rest ys));
