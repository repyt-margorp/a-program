main := ({
	text : #.Text := (#.scope_text &((#.print #"inner")));
	(#.print text);
})
	@#.return y => y
	@#.scope_text delayed k => k #"handled"
	@#.print text k => k text;
