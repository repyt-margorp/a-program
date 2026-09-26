main := ((#.scope_text &((#.print #"inner"))))
	@#.return y => y
	@#.scope_text delayed k => {
		inner : #.Text := delayed;
		k inner;
	};
