bad := (perform (#.abort_text &(perform (#.print #"inner"))))
	@#.return y => y
	@#.abort_text delayed k => k #"invalid";
