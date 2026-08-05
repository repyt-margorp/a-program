main := ((#.scope_text &((#.print #"inner"))))
	@#.return y => y
	@#.scope_text delayed k => {
		first := k #"first";
		second := k #"second";
	};
