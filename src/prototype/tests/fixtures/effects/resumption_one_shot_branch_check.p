Bool := @{ true : *; false : *; };

choose := \b : Bool =>
	((#.scope_text_once &((#.print #"inner"))))
		@#.return y => y
		@#.scope_text_once delayed k =>
			(b @true => k #"true" @false => k #"false");

main := choose Bool.true;
