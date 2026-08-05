bad := (perform (#.scope_text_once &(perform (#.print #"inner"))))
	@#.return y => y
	@#.scope_text_once delayed k => {
		first := k #"first";
		second := k #"second";
	};
