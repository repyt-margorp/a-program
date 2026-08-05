main := (perform (#.scope_text &(perform (#.print #"inner"))))
	@#.return y => y
	@#.scope_text delayed k => {
		first : #.Text := delayed;
		second : #.Text := delayed;
		k second;
	};
