main := ((#.abort_text &((#.print #"inner"))))
	@#.return y => y
	@#.abort_text delayed k => {
		(#.print #"handled");
		#"abort";
	};
